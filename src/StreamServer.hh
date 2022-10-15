#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BufferEvent.hh"
#include "EventBase.hh"
#include "Event.hh"
#include "Listener.hh"



struct StreamClientStateBase { };

template <typename ClientStateT = StreamClientStateBase>
class StreamServer {
public:
  StreamServer() = delete;
  StreamServer(const StreamServer&) = delete;
  StreamServer(StreamServer&&) = delete;
  StreamServer& operator=(const StreamServer&) = delete;
  StreamServer& operator=(StreamServer&&) = delete;
  virtual ~StreamServer() = default;

  void listen(const std::string& socket_path) {
    this->add_socket(::listen(socket_path, 0, SOMAXCONN));
  }

  void listen(const std::string& addr, int port) {
    this->add_socket(::listen(addr, port, SOMAXCONN));
  }

  void listen(int port) {
    this->listen("", port);
  }

  void add_socket(int fd) {
    struct evconnlistener* listener = evconnlistener_new(
        this->base.get(),
        StreamServer::dispatch_on_listen_accept,
        this,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_EXEC,
        0,
        fd);
    if (!listener) {
      throw std::runtime_error("can\'t create evconnlistener");
    }
    evconnlistener_set_error_cb(
        listener, StreamServer::dispatch_on_listen_error);
    this->listeners.emplace_back(listener).set_owned(true);
  }

  inline bool is_ssl() const {
    return this->ssl_ctx != nullptr;
  }

  inline bool socket_count() const {
    return this->listeners.size();
  }

  inline size_t client_count() const {
    return this->bev_to_client.size();
  }

protected:
  explicit StreamServer(
      EventBase& base,
      std::shared_ptr<SSL_CTX> ssl_ctx = nullptr,
      const char* log_prefix = "[StreamServer] ")
    : base(base), ssl_ctx(ssl_ctx), log(log_prefix) { }

  struct Client {
    BufferEvent bev;
    struct sockaddr_storage remote_addr;
    ClientStateT state;

    Client(StreamServer* server, BufferEvent&& bev) : bev(std::move(bev)) {
      get_socket_addresses(
          bufferevent_getfd(this->bev.get()), nullptr, &this->remote_addr);

      bufferevent_setcb(
          this->bev.get(),
          &StreamServer::dispatch_on_client_input,
          nullptr,
          &StreamServer::dispatch_on_client_error,
          server);
      bufferevent_enable(this->bev.get(), EV_READ | EV_WRITE);
    }
  };

  EventBase base;
  std::shared_ptr<SSL_CTX> ssl_ctx;
  std::vector<Listener> listeners;
  std::unordered_map<struct bufferevent*, Client> bev_to_client;
  PrefixedLogger log;

  void disconnect_client(Client& c) {
    auto it = this->bev_to_client.find(c.bev.get());
    if (it != this->bev_to_client.end()) {
      this->on_client_disconnect(c);
      this->bev_to_client.erase(it);
    }
  }

  static void dispatch_on_listen_accept(
      struct evconnlistener*,
      evutil_socket_t fd,
      struct sockaddr*,
      int,
      void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    BufferEvent bev(s->base, fd, BEV_OPT_CLOSE_ON_FREE, s->ssl_ctx.get());
    bufferevent* bev_ptr = bev.get();
    auto emplace_ret = s->bev_to_client.emplace(std::piecewise_construct,
        std::forward_as_tuple(bev_ptr),
        std::forward_as_tuple(s, std::move(bev)));
    try {
      s->on_client_connect(emplace_ret.first->second);
    } catch (const std::exception& e) {
      s->log.error("Error handling client connection: %s", e.what());
      s->disconnect_client(emplace_ret.first->second);
    }
  }

  static void dispatch_on_listen_error(
      struct evconnlistener* listener, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    int err = EVUTIL_SOCKET_ERROR();
    s->log.error("[StreamServer] Failure on listening socket %d: %d (%s)",
        evconnlistener_get_fd(listener),
        err,
        evutil_socket_error_to_string(err));
  }

  static void dispatch_on_client_input(
      struct bufferevent* bev, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    Client* c = nullptr;
    try {
      c = &s->bev_to_client.at(bev);
    } catch (const std::out_of_range&) {
      bufferevent_free(bev);
    }
    if (c) {
      try {
        s->on_client_input(*c);
      } catch (const std::exception& e) {
        s->log.error("Error handling client input: %s", e.what());
        s->disconnect_client(*c);
      }
    }
  }

  static void dispatch_on_client_error(
      struct bufferevent* bev, short events, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    Client* c = nullptr;
    try {
      c = &s->bev_to_client.at(bev);
    } catch (const std::out_of_range&) {
      bufferevent_free(bev);
    }
    if (c) {
      if (events & BEV_EVENT_ERROR) {
        int err = EVUTIL_SOCKET_ERROR();
        s->log.warning("[StreamServer] Client caused error %d (%s)",
            err, evutil_socket_error_to_string(err));
      }
      if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        s->disconnect_client(*c);
      }
    }
  }

  virtual void on_client_input(Client& c) = 0;
  virtual void on_client_connect(Client&) { }
  virtual void on_client_disconnect(Client&) { }
};
