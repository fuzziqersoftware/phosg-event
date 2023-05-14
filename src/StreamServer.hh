#pragma once

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
#include "Event.hh"
#include "EventBase.hh"
#include "Listener.hh"

struct StreamServerClientBase {
  BufferEvent bev;

  StreamServerClientBase(BufferEvent&& bev) : bev(std::move(bev)) {}
};

template <typename ClientT = StreamServerClientBase>
class StreamServer {
public:
  StreamServer() = delete;
  StreamServer(const StreamServer&) = delete;
  StreamServer(StreamServer&&) = delete;
  StreamServer& operator=(const StreamServer&) = delete;
  StreamServer& operator=(StreamServer&&) = delete;
  virtual ~StreamServer() = default;

  int listen(const std::string& socket_path) {
    int fd = ::listen(socket_path, 0, SOMAXCONN);
    this->add_socket(fd);
    return fd;
  }

  int listen(const std::string& addr, int port) {
    int fd = ::listen(addr, port, SOMAXCONN);
    this->add_socket(fd);
    return fd;
  }

  int listen(int port) {
    return this->listen("", port);
  }

  void add_socket(int fd) {
    if (this->listeners.count(fd)) {
      return;
    }
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
    auto& l = this->listeners.emplace(fd, listener).first->second;
    l.set_owned(true);
  }

  void remove_socket(int fd) {
    this->listeners.erase(fd);
  }

  std::unordered_set<int> all_sockets() const {
    std::unordered_set<int> ret;
    for (const auto& it : this->listeners) {
      ret.emplace(it.first);
    }
    return ret;
  }

  inline void set_ssl_ctx(std::shared_ptr<SSL_CTX> new_ssl_ctx) {
    this->ssl_ctx = new_ssl_ctx;
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
      : base(base),
        ssl_ctx(ssl_ctx),
        log(log_prefix) {
    static_assert(std::is_base_of<StreamServerClientBase, ClientT>::value, "Client type not derived from StreamServerClientBase");
  }

  EventBase base;
  std::shared_ptr<SSL_CTX> ssl_ctx;
  std::unordered_map<int, Listener> listeners;
  std::unordered_map<struct bufferevent*, std::shared_ptr<ClientT>> bev_to_client;
  PrefixedLogger log;

  void disconnect_client(std::shared_ptr<ClientT> c) {
    auto it = this->bev_to_client.find(c->bev.get());
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
    bufferevent_setcb(
        bev_ptr,
        &StreamServer::dispatch_on_client_input,
        nullptr,
        &StreamServer::dispatch_on_client_error,
        s);
    bufferevent_enable(bev_ptr, EV_READ | EV_WRITE);

    try {
      auto c = s->on_client_connect(std::move(bev));
      s->bev_to_client.emplace(bev_ptr, std::move(c));
    } catch (const std::exception& e) {
      s->log.error("Error handling client connection: %s", e.what());
    }
  }

  static void dispatch_on_listen_error(
      struct evconnlistener* listener, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    int err = EVUTIL_SOCKET_ERROR();
    s->log.error("Failure on listening socket %d: %d (%s)",
        evconnlistener_get_fd(listener),
        err,
        evutil_socket_error_to_string(err));
  }

  static void dispatch_on_client_input(
      struct bufferevent* bev, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    std::shared_ptr<ClientT> c;
    try {
      c = s->bev_to_client.at(bev);
    } catch (const std::out_of_range&) {
      bufferevent_free(bev);
    }
    if (c) {
      try {
        s->on_client_input(c);
      } catch (const std::exception& e) {
        s->log.error("Error handling client input: %s", e.what());
        s->disconnect_client(c);
      }
    }
  }

  static void dispatch_on_client_error(
      struct bufferevent* bev, short events, void* ctx) {
    StreamServer* s = reinterpret_cast<StreamServer*>(ctx);
    std::shared_ptr<ClientT> c;
    try {
      c = s->bev_to_client.at(bev);
    } catch (const std::out_of_range&) {
      bufferevent_free(bev);
    }
    if (c) {
      if (events & BEV_EVENT_ERROR) {
        int err = EVUTIL_SOCKET_ERROR();
        s->log.warning("Client caused error %d (%s)",
            err, evutil_socket_error_to_string(err));
      }
      if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        s->disconnect_client(c);
      }
    }
  }

  virtual std::shared_ptr<ClientT> on_client_connect(BufferEvent&& bev) = 0;
  virtual void on_client_input(std::shared_ptr<ClientT>) = 0;
  virtual void on_client_disconnect(std::shared_ptr<ClientT>) {}
};
