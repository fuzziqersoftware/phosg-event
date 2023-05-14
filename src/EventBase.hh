#pragma once

#include <event2/event.h>

#include <functional>
#include <vector>

#include "EventConfig.hh"

class Event;
class CallbackEvent;

class EventBase {
public:
  EventBase();
  EventBase(EventConfig& config);
  EventBase(struct event_base* base);
  EventBase(const EventBase& base);
  EventBase(EventBase&& base);
  EventBase& operator=(const EventBase& base);
  EventBase& operator=(EventBase&& base);
  ~EventBase();

  bool dispatch();
  bool loop(int flags);
  void loopexit(uint64_t usecs);
  void loopexit(const struct timeval* tv = nullptr);
  void loopbreak();
  void loopcontinue();
  bool got_exit() const;
  bool got_break() const;

  void once(
      evutil_socket_t fd,
      short what,
      std::function<void(evutil_socket_t, short)> cb,
      const struct timeval* timeout);
  void once(
      evutil_socket_t fd,
      short what,
      std::function<void(evutil_socket_t, short)> cb,
      uint64_t timeout_usecs);
  void once(
      evutil_socket_t fd,
      short what,
      void (*cb)(evutil_socket_t, short, void*),
      void* cbarg,
      const struct timeval* timeout);
  void once(
      evutil_socket_t fd,
      short what,
      void (*cb)(evutil_socket_t, short, void*),
      void* cbarg,
      uint64_t timeout_usecs);

  void once(std::function<void()> cb, const struct timeval* timeout);
  void once(std::function<void()> cb, uint64_t timeout_usecs = 0);

  CallbackEvent call_next(std::function<void()> cb);
  CallbackEvent call_later(std::function<void()> cb, uint64_t usecs);

  Event get_running_event();
  struct event* get_running_event_raw();

  void gettimeofday_cached(struct timeval* tv_out);
  uint64_t gettimeofday_cached64();
  void update_cache_time();

  void priority_init(int num_priorities);
  int get_npriorities() const;

  const struct timeval* init_common_timeout(const struct timeval* duration);

  void reinit();

  int foreach_event_raw(std::function<int(const struct event*)> cb);

  void dump_events(FILE* f);

  const char* get_method() const;
  enum event_method_feature get_features() const;
  std::vector<const char*> get_supported_methods();

  struct event_base* get();

protected:
  static void dispatch_once_cb(evutil_socket_t fd, short what, void* ctx);
  static void dispatch_once_timeout_cb(evutil_socket_t fd, short what, void* ctx);
  static int dispatch_foreach_event_raw_cb(const struct event_base* base,
      const struct event* event, void* ctx);

  struct event_base* base;
  bool owned;
};
