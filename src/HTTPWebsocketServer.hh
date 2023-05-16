#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <inttypes.h>
#include <stdlib.h>

#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>

#include "BufferEvent.hh"
#include "HTTPServer.hh"
#include "StreamServer.hh"

template <typename ClientStateT>
class HTTPWebSocketServer : public HTTPServer {
public:
  HTTPWebSocketServer(EventBase& base, SSL_CTX* ssl_ctx)
      : HTTPServer(base, ssl_ctx) {}

  HTTPWebSocketServer(const HTTPWebSocketServer&) = delete;
  HTTPWebSocketServer(HTTPWebSocketServer&&) = delete;
  HTTPWebSocketServer& operator=(const HTTPWebSocketServer&) = delete;
  HTTPWebSocketServer& operator=(HTTPWebSocketServer&&) = delete;
  virtual ~HTTPWebSocketServer() = default;

protected:
  struct WebSocketClient {
    BufferEvent bev;
    std::unique_ptr<struct evhttp_connection, void (*)(struct evhttp_connection*)> http_conn;
    uint8_t ws_pending_opcode;
    std::string ws_pending_data;
    std::unique_ptr<ClientStateT> state;

    WebSocketClient(struct evhttp_connection* conn)
        : http_conn(conn, evhttp_connection_free),
          bev(evhttp_connection_get_bufferevent(this->http_conn.get())),
          ws_pending_opcode(0xFF) {}
    ~WebSocketClient() = default;

    void reset_pending_frame() {
      this->ws_pending_opcode = 0xFF;
      this->ws_pending_data.clear();
    }
  };

  std::unordered_map<struct bufferevent*, std::shared_ptr<WebSocketClient>> bev_to_websocket_client;

  // Converts an HTTP request to a WebSocket connection. If successful,
  // handle_websocket_connect is called, and this function returns a
  // WebSocketClient object; the EvHTTPREquest is no longer usable after this.
  // If the request cannot be converted (because it isn't a GET request or
  // doesn't have the appropriate request headers), this function returns
  // nullptr, and the caller still must respond to the HTTP request.
  std::shared_ptr<WebSocketClient> enable_websockets(EvHTTPRequest& req) {
    if (req.get_command() != EVHTTP_REQ_GET) {
      return nullptr;
    }
    const char* connection_header = req.get_input_header("Connection");
    if (!connection_header || strcasecmp(connection_header, "upgrade")) {
      return nullptr;
    }
    const char* upgrade_header = req.get_input_header("Upgrade");
    if (!upgrade_header || strcasecmp(upgrade_header, "websocket")) {
      return nullptr;
    }
    const char* ws_key_header = req.get_input_header("Sec-WebSocket-Key");
    if (!ws_key_header) {
      return nullptr;
    }

    // Note: it's important that we make a copy of this header's value since
    // we're about to free the original
    std::string ws_key = ws_key_header;

    std::string sec_websocket_accept_data =
        ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string sec_websocket_accept =
        base64_encode(sha1(sec_websocket_accept_data));

    // Hijack the bufferevent since it's no longer going to handle HTTP at all
    struct evhttp_connection* conn = req.get_connection();
    struct bufferevent* bev = evhttp_connection_get_bufferevent(conn);
    bufferevent_setcb(
        bev,
        &HTTPWebSocketServer::dispatch_on_websocket_read,
        nullptr,
        &HTTPWebSocketServer::dispatch_on_websocket_error,
        this);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    // Send the HTTP reply, which enables websockets
    struct evbuffer* out_buf = bufferevent_get_output(bev);
    evbuffer_add_printf(out_buf, "HTTP/1.1 101 Switching Protocols\r\n\
  Upgrade: websocket\r\n\
  Connection: upgrade\r\n\
  Sec-WebSocket-Accept: %s\r\n\
  \r\n",
        sec_websocket_accept.c_str());

    return this->bev_to_websocket_client.emplace(bev, new WebSocketClient(conn)).first->second;
  }

  static void dispatch_on_websocket_read(struct bufferevent* bev, void* ctx) {
    reinterpret_cast<HTTPWebSocketServer*>(ctx)->on_websocket_read(bev);
  }

  static void dispatch_on_websocket_error(
      struct bufferevent* bev, short events, void* ctx) {
    reinterpret_cast<HTTPWebSocketServer*>(ctx)->on_websocket_error(bev, events);
  }

  void on_websocket_read(struct bufferevent* bev) {
    EvBuffer buf(bufferevent_get_input(bev));

    // We need at most 10 bytes to determine if there's a valid frame, or as
    // little as 2
    std::string header_data(10, '\0');
    ssize_t bytes_read = buf.copyout_atmost(
        const_cast<char*>(header_data.data()), header_data.size());
    if (bytes_read < 2) {
      return; // We don't have a full header yet
    }

    // Figure out the payload size
    bool has_mask = header_data[1] & 0x80;
    size_t header_size = 2;
    size_t payload_size = header_data[1] & 0x7F;
    if (payload_size == 0x7F) {
      if (bytes_read < 10) {
        return;
      }
      payload_size = bswap64(*reinterpret_cast<const uint64_t*>(&header_data[2]));
      header_size = 10;
    } else if (payload_size == 0x7E) {
      if (bytes_read < 4) {
        return;
      }
      payload_size = bswap16(*reinterpret_cast<const uint16_t*>(&header_data[2]));
      header_size = 4;
    }
    if (has_mask) {
      header_size += 4;
    }
    if (buf.get_length() < header_size + payload_size) {
      return; // We don't have a complete message yet
    }

    // We have a complete message. Skip the header bytes (we already read them)
    // and read the masking key if needed
    buf.drain(header_size);
    uint8_t mask_key[4];
    if (has_mask) {
      buf.remove(mask_key, 4);
    }

    // Get the client's state
    auto c = this->bev_to_websocket_client.at(bev);

    // Read the message data and unmask it
    std::string payload = buf.remove(payload_size);
    if (has_mask) {
      for (size_t x = 0; x < payload_size; x++) {
        payload[x] ^= mask_key[x & 3];
      }
    }

    // If the current message is a control message, respond appropriately (these
    // can be sent in the middle of fragmented messages)
    uint8_t opcode = header_data[0] & 0x0F;
    if (opcode & 0x08) {
      if (opcode == 0x0A) {
        // Ignore ping responses

      } else if (opcode == 0x08) {
        this->send_websocket_message(bev, move(payload), 0x08);
        this->disconnect_websocket_client(bev);

      } else if (opcode == 0x09) {
        this->send_websocket_message(bev, move(payload), 0x0A);

      } else {
        this->disconnect_websocket_client(bev);
      }
      return;
    }

    // If there's an existing pending message, the current message's opcode
    // should be zero; if there's no pending message, it must not be zero
    if ((c->ws_pending_opcode != 0xFF) == (opcode != 0)) {
      this->disconnect_websocket_client(bev);
      return;
    }

    // Save the message opcode, if present, and append the frame data
    if (opcode) {
      c->ws_pending_opcode = opcode;
    }
    c->ws_pending_data += payload;

    // If the FIN bit is set, then the frame is complete - append the payload to
    // any pending payloads and call the message handler
    if (header_data[0] & 0x80) {
      this->handle_websocket_message(c, c->ws_pending_opcode, move(c->ws_pending_data));
      c->reset_pending_frame();
    }

    // If the FIN bit isn't set, we're done for now; we need to receive at least
    // one continuation frame to complete the message
  }

  void on_websocket_error(struct bufferevent* bev, short events) {
    if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
      this->disconnect_websocket_client(bev);
    }
  }

  void disconnect_websocket_client(struct bufferevent* bev) {
    auto it = this->bev_to_websocket_client.find(bev);
    this->handle_websocket_disconnect(it->second);
    this->bev_to_websocket_client.erase(it);
  }

  static void send_websocket_message(
      EvBuffer& buf, std::string&& message, uint8_t opcode = 0x01) {
    StringWriter w;
    w.put_u8(0x80 | (opcode & 0x0F));
    if (message.size() > 0xFFFF) {
      w.put_u8(0x7F);
      w.put_u64b(message.size());
    } else if (message.size() > 0x7D) {
      w.put_u8(0x7E);
      w.put_u32b(message.size());
    } else {
      w.put_u8(message.size());
    }

    auto g = buf.lock_guard();
    buf.add(w.str());
    buf.add_reference(move(message));
  }

  static void send_websocket_message(
      struct evbuffer* buf, std::string&& message, uint8_t opcode = 0x01) {
    EvBuffer evbuf(buf);
    HTTPWebSocketServer::send_websocket_message(evbuf, move(message), opcode);
  }

  static void send_websocket_message(
      struct bufferevent* bev, std::string&& message, uint8_t opcode = 0x01) {
    HTTPWebSocketServer::send_websocket_message(
        bufferevent_get_output(bev), move(message), opcode);
  }

  static void send_websocket_message(
      std::shared_ptr<WebSocketClient> c,
      std::string&& message,
      uint8_t opcode = 0x01) {
    HTTPWebSocketServer::send_websocket_message(
        c->bev.get(), move(message), opcode);
  }

  virtual void on_request(EvHTTPRequest& req) = 0;
  virtual void on_websocket_message(std::shared_ptr<WebSocketClient> c,
      uint8_t opcode, std::string&& message) = 0;

  virtual void on_websocket_connect(std::shared_ptr<WebSocketClient>) {}
  virtual void in_websocket_disconnect(std::shared_ptr<WebSocketClient>) {}
};
