#include "EventBase.hh"

#include <phosg/Time.hh>

#include "Event.hh"

using namespace std;

EventBase::EventBase()
    : base(event_base_new()),
      owned(true) {
  if (!this->base) {
    throw runtime_error("event_base_new");
  }
}

EventBase::EventBase(EventConfig& config)
    : base(event_base_new_with_config(config.get())),
      owned(true) {
  if (!this->base) {
    throw runtime_error("event_base_new_with_config");
  }
}

EventBase::EventBase(struct event_base* base)
    : base(base),
      owned(false) {}

EventBase::EventBase(const EventBase& other)
    : base(other.base),
      owned(false) {}

EventBase::EventBase(EventBase&& other)
    : base(other.base),
      owned(other.owned) {
  other.owned = false;
}

EventBase& EventBase::operator=(const EventBase& other) {
  this->base = other.base;
  this->owned = false;
  return *this;
}

EventBase& EventBase::operator=(EventBase&& other) {
  this->base = other.base;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

EventBase::~EventBase() {
  if (this->owned && this->base) {
    event_base_free(this->base);
  }
}

bool EventBase::dispatch() {
  int ret = event_base_dispatch(this->get());
  if (ret < 0) {
    throw runtime_error("event_base_dispatch");
  }
  return (ret == 0);
}

bool EventBase::loop(int flags) {
  int ret = event_base_loop(this->base, flags);
  if (ret < 0) {
    throw runtime_error("event_base_loop");
  }
  return (ret == 0);
}

void EventBase::loopexit(uint64_t usecs) {
  int ret;
  if (usecs) {
    auto tv = usecs_to_timeval(usecs);
    ret = event_base_loopexit(this->base, &tv);
  } else {
    ret = event_base_loopexit(this->base, nullptr);
  }
  if (ret) {
    throw runtime_error("event_base_loopexit");
  }
}

void EventBase::loopexit(const struct timeval* tv) {
  if (event_base_loopexit(this->base, tv)) {
    throw runtime_error("event_base_loopexit");
  }
}

void EventBase::loopbreak() {
  if (event_base_loopbreak(this->base)) {
    throw runtime_error("event_base_loopbreak");
  }
}

void EventBase::loopcontinue() {
  if (event_base_loopcontinue(this->base)) {
    throw runtime_error("event_base_loopcontinue");
  }
}

bool EventBase::got_exit() const {
  return event_base_got_exit(this->base);
}

bool EventBase::got_break() const {
  return event_base_got_break(this->base);
}

void EventBase::dispatch_once_cb(evutil_socket_t fd, short what, void* ctx) {
  auto* fn = reinterpret_cast<function<void(evutil_socket_t, short)>*>(ctx);
  (*fn)(fd, what);
  delete fn;
}

void EventBase::dispatch_once_timeout_cb(evutil_socket_t, short, void* ctx) {
  auto* fn = reinterpret_cast<function<void()>*>(ctx);
  (*fn)();
  delete fn;
}

void EventBase::once(
    evutil_socket_t fd,
    short what,
    function<void(evutil_socket_t, short)> cb,
    const struct timeval* timeout) {
  // TODO: can we do this without an extra allocation?
  auto fn = new function<void(evutil_socket_t, short)>(cb);
  if (event_base_once(this->base, fd, what, &EventBase::dispatch_once_cb, fn,
          timeout)) {
    delete fn;
    throw runtime_error("event_base_once");
  }
}

void EventBase::once(
    evutil_socket_t fd,
    short what,
    function<void(evutil_socket_t, short)> cb,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  this->once(fd, what, cb, &tv);
}

void EventBase::once(
    evutil_socket_t fd,
    short what,
    void (*cb)(evutil_socket_t, short, void*),
    void* cbarg,
    const struct timeval* timeout) {
  if (event_base_once(this->base, fd, what, cb, cbarg, timeout)) {
    throw runtime_error("event_base_once");
  }
}

void EventBase::once(
    evutil_socket_t fd,
    short what,
    void (*cb)(evutil_socket_t, short, void*),
    void* cbarg,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  this->once(fd, what, cb, cbarg, &tv);
}

void EventBase::once(
    function<void()> cb,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  this->once(cb, &tv);
}

void EventBase::once(
    function<void()> cb,
    const struct timeval* timeout) {
  // TODO: can we do this without an extra allocation?
  auto fn = new function<void()>(cb);
  if (event_base_once(this->base, -1, EV_TIMEOUT, &EventBase::dispatch_once_timeout_cb, fn, timeout)) {
    delete fn;
    throw runtime_error("event_base_once");
  }
}

CallbackEvent EventBase::call_next(std::function<void()> cb) {
  return this->call_later(move(cb), 0);
}

CallbackEvent EventBase::call_later(std::function<void()> cb, uint64_t usecs) {
  return CallbackEvent(*this, move(cb), usecs);
}

Event EventBase::get_running_event() {
  return Event(event_base_get_running_event(this->base));
}

struct event* EventBase::get_running_event_raw() {
  return event_base_get_running_event(this->base);
}

void EventBase::gettimeofday_cached(struct timeval* tv_out) {
  if (event_base_gettimeofday_cached(this->base, tv_out)) {
    throw runtime_error("event_base_gettimeofday_cached");
  }
}

uint64_t EventBase::gettimeofday_cached64() {
  struct timeval tv;
  if (event_base_gettimeofday_cached(this->base, &tv)) {
    throw runtime_error("event_base_gettimeofday_cached");
  }
  return timeval_to_usecs(tv);
}

void EventBase::update_cache_time() {
  if (!event_base_update_cache_time(this->base)) {
    throw runtime_error("event_base_update_cache_time");
  }
}

void EventBase::priority_init(int num_priorities) {
  if (event_base_priority_init(this->base, num_priorities)) {
    throw runtime_error("event_base_priority_init");
  }
}

int EventBase::get_npriorities() const {
  return event_base_get_npriorities(this->base);
}

const struct timeval* EventBase::init_common_timeout(
    const struct timeval* duration) {
  return event_base_init_common_timeout(this->base, duration);
}

void EventBase::reinit() {
  if (event_reinit(this->base)) {
    throw runtime_error("event_reinit");
  }
}

int EventBase::dispatch_foreach_event_raw_cb(const struct event_base*,
    const struct event* event, void* ctx) {
  return (*reinterpret_cast<function<int(const struct event*)>*>(ctx))(event);
}

int EventBase::foreach_event_raw(function<int(const struct event*)> cb) {
  return event_base_foreach_event(this->base,
      &EventBase::dispatch_foreach_event_raw_cb, &cb);
}

void EventBase::dump_events(FILE* f) {
  event_base_dump_events(this->base, f);
}

const char* EventBase::get_method() const {
  return event_base_get_method(this->base);
}

enum event_method_feature EventBase::get_features() const {
  return static_cast<enum event_method_feature>(event_base_get_features(this->base));
}

vector<const char*> EventBase::get_supported_methods() {
  vector<const char*> ret;
  for (const char** methods = event_get_supported_methods();
       *methods != nullptr;
       methods++) {
    ret.push_back(*methods);
  }
  return ret;
}

struct event_base* EventBase::get() {
  return this->base;
}
