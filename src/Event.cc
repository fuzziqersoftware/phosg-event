#include "Event.hh"

#include <phosg/Time.hh>

using namespace std;

Event::Event(EventBase& base, evutil_socket_t fd, short what)
    : event(event_new(base.get(), fd, what, &Event::dispatch_on_trigger, this)),
      owned(true) {
  if (!this->event) {
    throw runtime_error("event_new");
  }
}

Event::Event(struct event* ev)
    : event(ev),
      owned(false) {}

Event::Event(const Event& other)
    : event(other.event),
      owned(false) {}

Event::Event(Event&& other)
    : event(other.event),
      owned(other.owned) {
  other.owned = false;
}

Event& Event::operator=(const Event& other) {
  this->event = other.event;
  this->owned = false;
  return *this;
}

Event& Event::operator=(Event&& other) {
  this->event = other.event;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

Event::~Event() {
  if (this->owned && this->event) {
    event_free(this->event);
  }
}

void Event::add(const struct timeval* tv) {
  if (event_add(this->event, tv)) {
    throw runtime_error("event_add");
  }
}

void Event::add(uint64_t usecs) {
  auto tv = usecs_to_timeval(usecs);
  if (event_add(this->event, &tv)) {
    throw runtime_error("event_add");
  }
}

void Event::del() {
  if (event_del(this->event)) {
    throw runtime_error("event_del");
  }
}

void Event::remove_timer() {
  if (event_remove_timer(this->event)) {
    throw runtime_error("event_remove_timer");
  }
}

void Event::priority_set(int priority) {
  if (event_priority_set(this->event, priority)) {
    throw runtime_error("event_priority_set");
  }
}

void Event::active(int what, short ncalls) {
  event_active(this->event, what, ncalls);
}

int Event::pending(short what, struct timeval* tv) const {
  return event_pending(this->event, what, tv);
}

evutil_socket_t Event::get_fd() const {
  return event_get_fd(this->event);
}

EventBase Event::get_base() const {
  return EventBase(event_get_base(this->event));
}

short Event::get_events() const {
  return event_get_events(this->event);
}

int Event::get_priority() const {
  return event_get_priority(this->event);
}

bool Event::initialized() const {
  return event_initialized(this->event);
}

struct event* Event::get() {
  return this->event;
}

void Event::on_trigger(evutil_socket_t, short) {
  // Default behavior: do nothing
}

void Event::dispatch_on_trigger(evutil_socket_t fd, short what, void* ctx) {
  Event* ev = reinterpret_cast<Event*>(ctx);
  ev->on_trigger(fd, what);
}

TimeoutEvent::TimeoutEvent(EventBase& base, const struct timeval* tv, bool persist)
    : Event(base, -1, EV_TIMEOUT | (persist ? EV_PERSIST : 0)),
      tv(*tv) {}

TimeoutEvent::TimeoutEvent(EventBase& base, uint64_t usecs, bool persist)
    : Event(base, -1, EV_TIMEOUT | (persist ? EV_PERSIST : 0)),
      tv(usecs_to_timeval(usecs)) {}

void TimeoutEvent::add() {
  if (event_add(this->event, &this->tv)) {
    throw runtime_error("event_add");
  }
}

SignalEvent::SignalEvent(EventBase& base, int signum)
    : Event(base, signum, EV_SIGNAL | EV_PERSIST) {}
