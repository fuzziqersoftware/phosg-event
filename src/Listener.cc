#include "Listener.hh"

#include <unistd.h>

using namespace std;

Listener::Listener(EventBase& base, unsigned flags, int backlog,
    evutil_socket_t fd)
    : lev(evconnlistener_new(base.get(), &Listener::dispatch_on_accept, this, flags, backlog, fd)),
      owned(true) {
  if (!this->lev) {
    throw runtime_error("evconnlistener_new");
  }
}

Listener::Listener(EventBase& base, unsigned flags, int backlog,
    const struct sockaddr* sa, int socklen)
    : lev(evconnlistener_new_bind(base.get(), &Listener::dispatch_on_accept, this, flags, backlog, sa, socklen)),
      owned(true) {
  if (!this->lev) {
    throw runtime_error("evconnlistener_new_bind");
  }
}

Listener::Listener(struct evconnlistener* lev)
    : lev(lev),
      owned(false) {}

Listener::Listener(const Listener& other)
    : lev(other.lev),
      owned(false) {}

Listener::Listener(Listener&& other)
    : lev(other.lev),
      owned(other.owned) {
  other.owned = false;
}

Listener& Listener::operator=(const Listener& other) {
  this->lev = other.lev;
  this->owned = false;
  return *this;
}

Listener& Listener::operator=(Listener&& other) {
  this->lev = other.lev;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

Listener::~Listener() {
  if (this->owned && this->lev) {
    evconnlistener_free(this->lev);
  }
}

void Listener::set_owned(bool owned) {
  this->owned = owned;
}

void Listener::enable() {
  if (evconnlistener_enable(this->lev)) {
    throw runtime_error("evconnlistener_enable");
  }
}

void Listener::disable() {
  if (evconnlistener_disable(this->lev)) {
    throw runtime_error("evconnlistener_disable");
  }
}

evutil_socket_t Listener::get_fd() const {
  return evconnlistener_get_fd(this->lev);
}

EventBase Listener::get_base() const {
  return EventBase(evconnlistener_get_base(this->lev));
}

struct event_base* Listener::get_base_raw() const {
  return evconnlistener_get_base(this->lev);
}

void Listener::dispatch_on_accept(struct evconnlistener*, evutil_socket_t fd,
    struct sockaddr* addr, int len, void* ctx) {
  reinterpret_cast<Listener*>(ctx)->on_accept(fd, addr, len);
}

void Listener::dispatch_on_error(struct evconnlistener*, void* ctx) {
  reinterpret_cast<Listener*>(ctx)->on_error();
}

void Listener::on_accept(evutil_socket_t fd, struct sockaddr*, int) {
  // Subclasses should override this. This default behavior is useless in all
  // scenarios. This implementation exists only so that Listener will not be an
  // abstract class.
  ::close(fd);
}

void Listener::on_error() {
  // Default behavior: do nothing
}

struct evconnlistener* Listener::get() {
  return this->lev;
}
