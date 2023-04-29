#pragma once

#include <event2/event.h>
#include <event2/listener.h>

#include <memory>

#include "EventBase.hh"

class Listener {
public:
  Listener(EventBase& base, unsigned flags, int backlog,
      evutil_socket_t fd = -1);
  Listener(EventBase& base, unsigned flags, int backlog,
      const struct sockaddr* sa, int socklen);
  Listener(struct evconnlistener* lst);
  Listener(const Listener& lev);
  Listener(Listener&& lev);
  Listener& operator=(const Listener& lev);
  Listener& operator=(Listener&& lev);
  virtual ~Listener();

  void set_owned(bool owned);

  void enable();
  void disable();

  evutil_socket_t get_fd() const;
  EventBase get_base() const;
  struct event_base* get_base_raw() const;

  struct evconnlistener* get();

protected:
  static void dispatch_on_accept(struct evconnlistener* lev, evutil_socket_t fd,
      struct sockaddr* addr, int len, void* ctx);
  static void dispatch_on_error(struct evconnlistener* lev, void* ctx);
  virtual void on_accept(evutil_socket_t fd, struct sockaddr* addr, int len);
  virtual void on_error();

  struct evconnlistener* lev;
  bool owned;
};
