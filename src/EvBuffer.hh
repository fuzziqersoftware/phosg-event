#pragma once

#include <event2/buffer.h>

#include <functional>
#include <memory>
#include <string>
#include <phosg/Encoding.hh>



struct EvBuffer {
  class LockGuard {
  public:
    LockGuard(EvBuffer* buf);
    LockGuard(const LockGuard&) = delete;
    LockGuard(LockGuard&& other);
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard& operator=(LockGuard&& other);
    ~LockGuard();
  private:
    EvBuffer* buf;
  };

  EvBuffer();
  explicit EvBuffer(struct evbuffer* buf);
  EvBuffer(const EvBuffer& other);
  EvBuffer(EvBuffer&& other);
  EvBuffer& operator=(const EvBuffer& other);
  EvBuffer& operator=(EvBuffer&& other);
  ~EvBuffer();

  void set_owned(bool owned);
  struct evbuffer* get();
  const struct evbuffer* get() const;

  void enable_locking(void* lock = nullptr);
  void lock();
  void unlock();
  LockGuard lock_guard();

  size_t get_length() const;
  size_t get_contiguous_space() const;
  void expand(size_t size);

  void add(const void* data, size_t size);
  void add(const std::string& data);

  size_t add_printf(const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));
  size_t add_vprintf(const char* fmt, va_list va);

  void add_buffer(struct evbuffer* src);
  void add_buffer(EvBuffer& src);
  void remove_buffer(struct evbuffer* src, size_t size);
  void remove_buffer(EvBuffer& src, size_t size);
  size_t remove_buffer_atmost(struct evbuffer* src, size_t size);
  size_t remove_buffer_atmost(EvBuffer& src, size_t size);

  void prepend(const void* data, size_t size);
  void prepend_buffer(struct evbuffer* src);
  void prepend_buffer(EvBuffer& src);

  uint8_t* pullup(ssize_t size);

  void drain(size_t size);
  void drain_all();

  size_t remove_atmost(void* data, size_t size);
  std::string remove_atmost(size_t size);
  void remove(void* data, size_t size);
  std::string remove(size_t size);

  size_t copyout_atmost(void* data, size_t size);
  std::string copyout_atmost(size_t size);
  void copyout(void* data, size_t size);
  std::string copyout(size_t size);
  size_t copyout_from_atmost(const struct evbuffer_ptr* pos, void* data, size_t size);
  std::string copyout_from_atmost(const struct evbuffer_ptr* pos, size_t size);
  void copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size);
  std::string copyout_from(const struct evbuffer_ptr* pos, size_t size);

  template <typename T, std::enable_if_t<std::is_pod_v<T>, bool> = true>
  void add(const T& t) {
    this->add(&t, sizeof(T));
  }
  template <typename T, std::enable_if_t<std::is_pod_v<T>, bool> = true>
  T remove() {
    T ret;
    this->remove(&ret, sizeof(T));
    return ret;
  }
  template <typename T, std::enable_if_t<std::is_pod_v<T>, bool> = true>
  T copyout() {
    T ret;
    this->copyout(&ret, sizeof(T));
    return ret;
  }

  inline void add_u8(uint8_t v)    { this->add<uint8_t>(v); }
  inline void add_s8(int8_t v)     { this->add<int8_t>(v); }
  inline void add_u16b(uint16_t v) { this->add<be_uint16_t>(v); }
  inline void add_s16b(int16_t v)  { this->add<be_int16_t>(v); }
  inline void add_u16l(uint16_t v) { this->add<le_uint16_t>(v); }
  inline void add_s16l(int16_t v)  { this->add<le_int16_t>(v); }
  inline void add_u32b(uint32_t v) { this->add<be_uint32_t>(v); }
  inline void add_s32b(int32_t v)  { this->add<be_int32_t>(v); }
  inline void add_u32l(uint32_t v) { this->add<le_uint32_t>(v); }
  inline void add_s32l(int32_t v)  { this->add<le_int32_t>(v); }
  inline void add_u64b(uint64_t v) { this->add<be_uint64_t>(v); }
  inline void add_s64b(int64_t v)  { this->add<be_int64_t>(v); }
  inline void add_u64l(uint64_t v) { this->add<le_uint64_t>(v); }
  inline void add_s64l(int64_t v)  { this->add<le_int64_t>(v); }

  inline uint8_t remove_u8()    { return this->remove<uint8_t>(); }
  inline int8_t remove_s8()     { return this->remove<int8_t>(); }
  inline uint16_t remove_u16b() { return this->remove<be_uint16_t>(); }
  inline int16_t remove_s16b()  { return this->remove<be_int16_t>(); }
  inline uint16_t remove_u16l() { return this->remove<le_uint16_t>(); }
  inline int16_t remove_s16l()  { return this->remove<le_int16_t>(); }
  inline uint32_t remove_u32b() { return this->remove<be_uint32_t>(); }
  inline int32_t remove_s32b()  { return this->remove<be_int32_t>(); }
  inline uint32_t remove_u32l() { return this->remove<le_uint32_t>(); }
  inline int32_t remove_s32l()  { return this->remove<le_int32_t>(); }
  inline uint64_t remove_u64b() { return this->remove<be_uint64_t>(); }
  inline int64_t remove_s64b()  { return this->remove<be_int64_t>(); }
  inline uint64_t remove_u64l() { return this->remove<le_uint64_t>(); }
  inline int64_t remove_s64l()  { return this->remove<le_int64_t>(); }

  inline uint8_t copyout_u8()    { return this->copyout<uint8_t>(); }
  inline int8_t copyout_s8()     { return this->copyout<int8_t>(); }
  inline uint16_t copyout_u16b() { return this->copyout<be_uint16_t>(); }
  inline int16_t copyout_s16b()  { return this->copyout<be_int16_t>(); }
  inline uint16_t copyout_u16l() { return this->copyout<le_uint16_t>(); }
  inline int16_t copyout_s16l()  { return this->copyout<le_int16_t>(); }
  inline uint32_t copyout_u32b() { return this->copyout<be_uint32_t>(); }
  inline int32_t copyout_s32b()  { return this->copyout<be_int32_t>(); }
  inline uint32_t copyout_u32l() { return this->copyout<le_uint32_t>(); }
  inline int32_t copyout_s32l()  { return this->copyout<le_int32_t>(); }
  inline uint64_t copyout_u64b() { return this->copyout<be_uint64_t>(); }
  inline int64_t copyout_s64b()  { return this->copyout<be_int64_t>(); }
  inline uint64_t copyout_u64l() { return this->copyout<le_uint64_t>(); }
  inline int64_t copyout_s64l()  { return this->copyout<le_int64_t>(); }

  std::string readln(enum evbuffer_eol_style eol_style);

  struct evbuffer_ptr search(const char* what, size_t size,
      const struct evbuffer_ptr* start);
  struct evbuffer_ptr search_range(const char* what, size_t size,
      const struct evbuffer_ptr *start, const struct evbuffer_ptr *end);
  struct evbuffer_ptr search_eol(struct evbuffer_ptr* start,
      size_t* bytes_found, enum evbuffer_eol_style eol_style);
  void ptr_set(struct evbuffer_ptr* pos, size_t position,
      enum evbuffer_ptr_how how);

  int peek(ssize_t size, struct evbuffer_ptr* start_at,
      struct evbuffer_iovec* vec_out, int n_vec);
  int reserve_space(ev_ssize_t size, struct evbuffer_iovec* vec, int n_vecs);
  void commit_space(struct evbuffer_iovec* vec, int n_vecs);

  void add_reference(const void* data, size_t size,
      void (*cleanup_fn)(const void* data, size_t size, void* ctx), void* ctx);
  void add_reference(const std::string& data);
  void add_reference(std::string&& data);
  void add_reference(const void* data, size_t size,
      std::function<void(const void* data, size_t size)> cleanup_fn);

  void add_file(int fd, off_t offset, size_t size);

  void add_buffer_reference(struct evbuffer* other_buf);
  void add_buffer_reference(EvBuffer& other_buf);

  void freeze(int at_front);
  void unfreeze(int at_front);

  // This copies all of the data out, so it is slow and should only be used for
  // debugging
  void debug_print_contents(FILE* stream);

protected:
  struct evbuffer* buf;
  bool owned;
};
