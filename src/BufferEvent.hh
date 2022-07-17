#pragma once

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <memory>

#include "EventBase.hh"
#include "EvDNSBase.hh"
#include "EvBuffer.hh"



// TODO: implement SSL and other advanced functions

class BufferEvent {
public:
  BufferEvent(EventBase& base, evutil_socket_t fd,
      enum bufferevent_options options, SSL_CTX* ssl_ctx = nullptr);
  BufferEvent(struct bufferevent* bev);
  BufferEvent(const BufferEvent& bev);
  BufferEvent(BufferEvent&& bev);
  BufferEvent& operator=(const BufferEvent& bev);
  BufferEvent& operator=(BufferEvent&& bev);
  virtual ~BufferEvent();

  void socket_connect(struct sockaddr* addr, int addrlen);
  void socket_connect_hostname(EvDNSBase* dns_base, int family,
      const char* hostname, int port);
  int socket_get_dns_error() const;

  void enable(short what);
  void disable(short what);
  short get_enabled() const;

  void setwatermark(short what, size_t low, size_t high);
  void set_timeouts(const struct timeval* read,
      const struct timeval* write);
  void set_timeouts(uint64_t read, uint64_t write);

  bool flush(short what, enum bufferevent_flush_mode state);

  void priority_set(int pri);
  int get_priority() const;

  void setfd(evutil_socket_t fd);
  evutil_socket_t getfd() const;

  EventBase get_base();
  struct bufferevent* get_underlying_raw();
  BufferEvent get_underlying();

  void lock();
  void unlock();

  EvBuffer get_input();
  EvBuffer get_output();

  void write(const void* data, size_t size);
  void write_buffer(EvBuffer& buf);
  size_t read(void* data, size_t size);
  std::string read(size_t size);
  void read_buffer(EvBuffer& buf);

  struct bufferevent* get();

protected:
  static void dispatch_on_read(struct bufferevent* bev, void* ctx);
  static void dispatch_on_write(struct bufferevent* bev, void* ctx);
  static void dispatch_on_event(struct bufferevent* bev, short what, void* ctx);
  virtual void on_read();
  virtual void on_write();
  virtual void on_event(short what);

  struct bufferevent* bev;
  bool owned;
};
