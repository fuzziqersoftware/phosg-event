#include "EvDNSBase.hh"

using namespace std;

EvDNSBase::EvDNSBase(EventBase& base, int initialize, int fail_requests_on_destroy)
    : base(evdns_base_new(base.get(), initialize)),
      owned(true),
      fail_requests_on_destroy(fail_requests_on_destroy) {}

EvDNSBase::EvDNSBase(struct evdns_base* base)
    : base(base),
      owned(false),
      fail_requests_on_destroy(false) {}

EvDNSBase::EvDNSBase(const EvDNSBase& other)
    : base(other.base),
      owned(false) {}

EvDNSBase::EvDNSBase(EvDNSBase&& other)
    : base(other.base),
      owned(other.owned) {
  other.owned = false;
}

EvDNSBase& EvDNSBase::operator=(const EvDNSBase& other) {
  this->base = other.base;
  this->owned = false;
  return *this;
}

EvDNSBase& EvDNSBase::operator=(EvDNSBase&& other) {
  this->base = other.base;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

EvDNSBase::~EvDNSBase() {
  if (this->owned && this->base) {
    evdns_base_free(this->base, this->fail_requests_on_destroy);
  }
}

void EvDNSBase::resolv_conf_parse(int flags, const char* filename) {
  if (evdns_base_resolv_conf_parse(this->base, flags, filename)) {
    throw runtime_error("evdns_base_resolv_conf_parse");
  }
}

#ifdef WIN32
void EvDNSBase::config_windows_nameservers() {
  if (evdns_base_config_windows_nameservers(this->base)) {
    throw runtime_error("evdns_base_config_windows_nameservers");
  }
}
#endif

void EvDNSBase::nameserver_sockaddr_add(const struct sockaddr* sa, socklen_t len,
    unsigned flags) {
  if (evdns_base_nameserver_sockaddr_add(this->base, sa, len, flags)) {
    throw runtime_error("evdns_base_nameserver_sockaddr_add");
  }
}

void EvDNSBase::nameserver_ip_add(const char* ip_as_string) {
  if (evdns_base_nameserver_ip_add(this->base, ip_as_string)) {
    throw runtime_error("evdns_base_nameserver_ip_add");
  }
}

void EvDNSBase::load_hosts(const char* hosts_fname) {
  if (evdns_base_load_hosts(this->base, hosts_fname)) {
    throw runtime_error("evdns_base_load_hosts");
  }
}

void EvDNSBase::clear_nameservers_and_suspend() {
  if (evdns_base_clear_nameservers_and_suspend(this->base)) {
    throw runtime_error("evdns_base_clear_nameservers_and_suspend");
  }
}

void EvDNSBase::resume() {
  if (evdns_base_resume(this->base)) {
    throw runtime_error("evdns_base_resume");
  }
}

void EvDNSBase::search_clear() {
  return evdns_base_search_clear(this->base);
}

void EvDNSBase::search_add(const char* domain) {
  return evdns_base_search_add(this->base, domain);
}

void EvDNSBase::search_ndots_set(int ndots) {
  return evdns_base_search_ndots_set(this->base, ndots);
}

void EvDNSBase::set_option(const char* option, const char* val) {
  if (evdns_base_set_option(this->base, option, val)) {
    throw runtime_error("evdns_base_set_option");
  }
}

int EvDNSBase::count_nameservers() {
  return evdns_base_count_nameservers(this->base);
}

struct evdns_getaddrinfo_request* EvDNSBase::getaddrinfo(
    const char* nodename,
    const char* servname,
    const struct evutil_addrinfo* hints_in,
    void (*cb)(int result, struct evutil_addrinfo* res, void* arg),
    void* cbarg) {
  return evdns_getaddrinfo(this->base, nodename, servname, hints_in, cb, cbarg);
}

void EvDNSBase::getaddrinfo_cancel(struct evdns_getaddrinfo_request* req) {
  return evdns_getaddrinfo_cancel(req);
}

struct evdns_request* EvDNSBase::resolve_ipv4(
    const char* name, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_ipv4(this->base, name, flags, cb, ctx);
}

struct evdns_request* EvDNSBase::resolve_ipv6(
    const char* name, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_ipv6(this->base, name, flags, cb, ctx);
}

struct evdns_request* EvDNSBase::resolve_reverse(
    const struct in_addr* in, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_reverse(this->base, in, flags, cb, ctx);
}

struct evdns_request* EvDNSBase::resolve_reverse_ipv6(
    const struct in6_addr* in, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_reverse_ipv6(this->base, in, flags, cb, ctx);
}

void EvDNSBase::cancel_request(struct evdns_request* req) {
  return evdns_cancel_request(this->base, req);
}

const char* err_to_string(int err) {
  return evdns_err_to_string(err);
}

struct evdns_base* EvDNSBase::get() {
  return this->base;
}
