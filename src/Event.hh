#pragma once

#include <event2/event.h>

#include <memory>

#include "EventBase.hh"

// TODO: should we support unallocated event structs (e.g. via event_assign)?

class Event {
public:
  Event();
  Event(EventBase& base, evutil_socket_t fd, short what);
  explicit Event(struct event* ev);
  Event(const Event& ev);
  Event(Event&& ev);
  Event& operator=(const Event& ev);
  Event& operator=(Event&& ev);
  virtual ~Event();

  void add(const struct timeval* tv);
  void add(uint64_t usecs);
  void del();
  void remove_timer();
  void priority_set(int priority);
  void active(int what, short ncalls);

  int pending(short what, struct timeval* tv) const;
  evutil_socket_t get_fd() const;
  EventBase get_base() const;
  short get_events() const;
  int get_priority() const;
  bool initialized() const;

  struct event* get();

protected:
  virtual void on_trigger(evutil_socket_t fd, short what);
  static void dispatch_on_trigger(evutil_socket_t fd, short what, void* ctx);

  struct event* event;
  bool owned;
};

class TimeoutEvent : public Event {
public:
  TimeoutEvent();
  TimeoutEvent(EventBase& base, const struct timeval* tv, bool persist = false);
  TimeoutEvent(EventBase& base, uint64_t usecs, bool persist = false);
  TimeoutEvent(const TimeoutEvent& ev);
  TimeoutEvent(TimeoutEvent&& ev);
  TimeoutEvent& operator=(const TimeoutEvent& ev);
  TimeoutEvent& operator=(TimeoutEvent&& ev);
  virtual ~TimeoutEvent() = default;

  void add();

protected:
  struct timeval tv;
};

class CallbackEvent : public Event {
public:
  CallbackEvent();
  CallbackEvent(EventBase& base, std::function<void()> fn, bool persist = false);
  CallbackEvent(const CallbackEvent& ev);
  CallbackEvent(CallbackEvent&& ev);
  CallbackEvent& operator=(const CallbackEvent& ev);
  CallbackEvent& operator=(CallbackEvent&& ev);
  virtual ~CallbackEvent() = default;

  inline void call_next() {
    this->call_after_usecs(0);
  }

  void call_after_usecs(uint64_t usecs);

protected:
  virtual void on_trigger(evutil_socket_t fd, short what);
  std::function<void()> fn;
};

class SignalEvent : public Event {
public:
  SignalEvent();
  SignalEvent(EventBase& base, int signum);
  SignalEvent(const SignalEvent& ev);
  SignalEvent(SignalEvent&& ev);
  SignalEvent& operator=(const SignalEvent& ev);
  SignalEvent& operator=(SignalEvent&& ev);
  virtual ~SignalEvent() = default;
};
