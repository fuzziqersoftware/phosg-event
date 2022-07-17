#include "BufferEvent.hh"

#include <event2/bufferevent_ssl.h>

#include <phosg/Time.hh>

using namespace std;



BufferEvent::BufferEvent(EventBase& base, evutil_socket_t fd,
    enum bufferevent_options options, SSL_CTX* ssl_ctx)
  : bev(nullptr), owned(true) {

  if (ssl_ctx) {
    SSL* ssl = SSL_new(ssl_ctx);
    try {
      this->bev = bufferevent_openssl_socket_new(
          base.get(),
          -1,
          ssl,
          BUFFEREVENT_SSL_ACCEPTING,
          BEV_OPT_CLOSE_ON_FREE);
    } catch (const exception&) {
      SSL_free(ssl);
      throw;
    }
  } else {
    this->bev = bufferevent_socket_new(base.get(), fd, options);
  }

  if (!this->bev) {
    throw runtime_error("bufferevent_socket_new");
  }

  bufferevent_setcb(
      this->bev,
      &BufferEvent::dispatch_on_read,
      &BufferEvent::dispatch_on_write,
      &BufferEvent::dispatch_on_event,
      this);
}

BufferEvent::BufferEvent(struct bufferevent* bev) : bev(bev), owned(false) { }

BufferEvent::BufferEvent(const BufferEvent& other)
  : bev(other.bev), owned(false) { }

BufferEvent::BufferEvent(BufferEvent&& other)
  : bev(other.bev), owned(other.owned) {
  other.owned = false;
}

BufferEvent& BufferEvent::operator=(const BufferEvent& other) {
  this->bev = other.bev;
  this->owned = false;
  return *this;
}

BufferEvent& BufferEvent::operator=(BufferEvent&& other) {
  this->bev = other.bev;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

BufferEvent::~BufferEvent() {
  if (this->owned && this->bev) {
    bufferevent_free(this->bev);
  }
}

void BufferEvent::socket_connect(struct sockaddr* addr, int addrlen) {
  if (bufferevent_socket_connect(this->bev, addr, addrlen)) {
    throw runtime_error("bufferevent_socket_connect");
  }
}

void BufferEvent::socket_connect_hostname(EvDNSBase* dns_base, int family,
    const char* hostname, int port) {
  if (bufferevent_socket_connect_hostname(
      this->bev, dns_base ? dns_base->get() : nullptr, family, hostname, port)) {
    throw runtime_error("bufferevent_socket_connect_hostname");
  }
}

int BufferEvent::socket_get_dns_error() const {
  return bufferevent_socket_get_dns_error(this->bev);
}

void BufferEvent::enable(short what) {
  bufferevent_enable(this->bev, what);
}

void BufferEvent::disable(short what) {
  bufferevent_disable(this->bev, what);
}

short BufferEvent::get_enabled() const {
  return bufferevent_get_enabled(this->bev);
}

void BufferEvent::setwatermark(short what, size_t low, size_t high) {
  bufferevent_setwatermark(this->bev, what, low, high);
}

void BufferEvent::set_timeouts(const struct timeval* read,
    const struct timeval* write) {
  bufferevent_set_timeouts(this->bev, read, write);
}

void BufferEvent::set_timeouts(uint64_t read, uint64_t write) {
  auto read_tv = usecs_to_timeval(read);
  auto write_tv = usecs_to_timeval(write);
  bufferevent_set_timeouts(this->bev, &read_tv, &write_tv);
}

bool BufferEvent::flush(short what, enum bufferevent_flush_mode state) {
  int ret = bufferevent_flush(this->bev, what, state);
  if (ret < 0) {
    throw runtime_error("bufferevent_flush");
  }
  return ret;
}

void BufferEvent::priority_set(int pri) {
  if (bufferevent_priority_set(this->bev, pri)) {
    throw runtime_error("bufferevent_priority_set");
  }
}

int BufferEvent::get_priority() const {
  return bufferevent_get_priority(this->bev);
}

void BufferEvent::setfd(evutil_socket_t fd) {
  if (bufferevent_setfd(this->bev, fd)) {
    throw runtime_error("bufferevent_setfd");
  }
}

evutil_socket_t BufferEvent::getfd() const {
  // Note: we intentionally don't throw when -1 is returned here, since this can
  // be an expected scenario (e.g. if the bufferevent's fd just isn't set yet).
  return bufferevent_getfd(this->bev);
}

EventBase BufferEvent::get_base() {
  return EventBase(bufferevent_get_base(this->bev));
}

struct bufferevent* BufferEvent::get_underlying_raw() {
  return bufferevent_get_underlying(this->bev);
}

BufferEvent BufferEvent::get_underlying() {
  return BufferEvent(bufferevent_get_underlying(this->bev));
}

void BufferEvent::lock() {
  bufferevent_lock(this->bev);
}

void BufferEvent::unlock() {
  bufferevent_unlock(this->bev);
}

EvBuffer BufferEvent::get_input() {
  return EvBuffer(bufferevent_get_input(this->bev));
}

EvBuffer BufferEvent::get_output() {
  return EvBuffer(bufferevent_get_output(this->bev));
}

void BufferEvent::write(const void* data, size_t size) {
  if (bufferevent_write(this->bev, data, size)) {
    throw runtime_error("bufferevent_write");
  }
}

void BufferEvent::write_buffer(EvBuffer& buf) {
  if (bufferevent_write_buffer(this->bev, buf.get())) {
    throw runtime_error("bufferevent_write_buffer");
  }
}

size_t BufferEvent::read(void* data, size_t size) {
  return bufferevent_read(this->bev, data, size);
}

std::string BufferEvent::read(size_t size) {
  // TODO: eliminate this unnecessary initialization
  std::string data(size, '\0');
  size_t bytes_read = bufferevent_read(
      this->bev, const_cast<char*>(data.data()), size);
  data.resize(bytes_read);
  return data;
}

void BufferEvent::read_buffer(EvBuffer& buf) {
  if (bufferevent_read_buffer(this->bev, buf.get())) {
    throw runtime_error("bufferevent_read_buffer");
  }
}

struct bufferevent* BufferEvent::get() {
  return this->bev;
}

void BufferEvent::dispatch_on_read(struct bufferevent*, void* ctx) {
  reinterpret_cast<BufferEvent*>(ctx)->on_read();
}

void BufferEvent::dispatch_on_write(struct bufferevent*, void* ctx) {
  reinterpret_cast<BufferEvent*>(ctx)->on_write();
}

void BufferEvent::dispatch_on_event(
    struct bufferevent*, short what, void* ctx) {
  reinterpret_cast<BufferEvent*>(ctx)->on_event(what);
}

void BufferEvent::on_read() {
  // Default behavior: drain the buffer and ignore the contents
  auto buf = this->get_input();
  buf.drain(buf.get_length());
}

void BufferEvent::on_write() {
  // Default behavior: do nothing
}

void BufferEvent::on_event(short) {
  // Default behavior: do nothing
}
