#include "EvBuffer.hh"

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

using namespace std;

EvBuffer::BoundedReader::BoundedReader(EvBuffer* buf, size_t size)
    : buf(buf),
      bytes_remaining(size) {}

void EvBuffer::BoundedReader::consume(size_t size) {
  if (size < this->bytes_remaining) {
    throw EvBuffer::insufficient_data();
  }
  this->bytes_remaining -= size;
}

void EvBuffer::BoundedReader::skip(size_t size) {
  this->consume(size);
  this->buf->drain(size);
}

void EvBuffer::BoundedReader::read(void* data, size_t size) {
  this->consume(size);
  this->buf->remove(data, size);
}

std::string EvBuffer::BoundedReader::read(size_t size) {
  this->consume(size);
  return this->buf->remove(size);
}

EvBuffer::LockGuard::LockGuard(EvBuffer* buf) : buf(buf) {
  this->buf->lock();
}

EvBuffer::LockGuard::LockGuard(LockGuard&& other) : buf(other.buf) {
  other.buf = nullptr;
}

EvBuffer::LockGuard& EvBuffer::LockGuard::operator=(LockGuard&& other) {
  this->buf = other.buf;
  other.buf = nullptr;
  return *this;
}

EvBuffer::LockGuard::~LockGuard() {
  if (this->buf) {
    this->buf->unlock();
  }
}

EvBuffer::EvBuffer()
    : buf(evbuffer_new()),
      owned(true) {}

EvBuffer::EvBuffer(struct evbuffer* buf)
    : buf(buf),
      owned(false) {}

EvBuffer::EvBuffer(const EvBuffer& other)
    : buf(other.buf),
      owned(false) {}

EvBuffer::EvBuffer(EvBuffer&& other)
    : buf(other.buf),
      owned(other.owned) {
  other.owned = false;
}

EvBuffer& EvBuffer::operator=(const EvBuffer& other) {
  if (this->owned) {
    evbuffer_free(this->buf);
  }
  this->buf = other.buf;
  this->owned = false;
  return *this;
}

EvBuffer& EvBuffer::operator=(EvBuffer&& other) {
  if (this->owned) {
    evbuffer_free(this->buf);
  }
  this->buf = other.buf;
  this->owned = other.owned;
  other.owned = false;
  return *this;
}

EvBuffer::~EvBuffer() {
  if (this->owned) {
    evbuffer_free(this->buf);
  }
}

void EvBuffer::set_owned(bool owned) {
  this->owned = owned;
}

struct evbuffer* EvBuffer::get() {
  return this->buf;
}

const struct evbuffer* EvBuffer::get() const {
  return this->buf;
}

void EvBuffer::enable_locking(void* lock) {
  if (evbuffer_enable_locking(this->buf, lock)) {
    throw runtime_error("evbuffer_enable_locking");
  }
}

void EvBuffer::lock() {
  evbuffer_lock(this->buf);
}

void EvBuffer::unlock() {
  evbuffer_unlock(this->buf);
}

EvBuffer::LockGuard EvBuffer::lock_guard() {
  return LockGuard(this);
}

size_t EvBuffer::get_length() const {
  return evbuffer_get_length(this->buf);
}

size_t EvBuffer::get_contiguous_space() const {
  return evbuffer_get_contiguous_space(this->buf);
}

void EvBuffer::expand(size_t size) {
  if (evbuffer_expand(this->buf, size)) {
    throw runtime_error("evbuffer_expand");
  }
}

void EvBuffer::add(const void* data, size_t size) {
  if (evbuffer_add(this->buf, data, size)) {
    throw runtime_error("evbuffer_add");
  }
}

void EvBuffer::add(const string& data) {
  this->add(data.data(), data.size());
}

size_t EvBuffer::add_printf(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  va_end(va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

size_t EvBuffer::add_vprintf(const char* fmt, va_list va) {
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

void EvBuffer::add_buffer(struct evbuffer* src) {
  if (evbuffer_add_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_add_buffer");
  }
}

void EvBuffer::add_buffer(EvBuffer& src) {
  this->add_buffer(src.get());
}

size_t EvBuffer::remove_buffer_atmost(struct evbuffer* src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  return ret;
}

size_t EvBuffer::remove_buffer_atmost(EvBuffer& src, size_t size) {
  return this->remove_buffer_atmost(src.get(), size);
}

void EvBuffer::remove_buffer(struct evbuffer* src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  if (static_cast<size_t>(ret) != size) {
    throw insufficient_data();
  }
}

void EvBuffer::remove_buffer(EvBuffer& src, size_t size) {
  this->remove_buffer(src.get(), size);
}

void EvBuffer::prepend(const void* data, size_t size) {
  if (evbuffer_prepend(this->buf, data, size)) {
    throw runtime_error("evbuffer_prepend");
  }
}

void EvBuffer::prepend_buffer(struct evbuffer* src) {
  if (evbuffer_prepend_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_prepend_buffer");
  }
}

void EvBuffer::prepend_buffer(EvBuffer& src) {
  return this->prepend_buffer(src.get());
}

uint8_t* EvBuffer::pullup(ssize_t size) {
  uint8_t* ret = evbuffer_pullup(this->buf, size);
  if (!ret) {
    throw runtime_error("evbuffer_pullup");
  }
  return ret;
}

void EvBuffer::drain(size_t size) {
  if (evbuffer_drain(this->buf, size)) {
    throw runtime_error("evbuffer_drain");
  }
}

void EvBuffer::drain_all() {
  this->drain(this->get_length());
}

size_t EvBuffer::remove_atmost(void* data, size_t size) {
  int ret = evbuffer_remove(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove");
  }
  return ret;
}

string EvBuffer::remove_atmost(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->remove_atmost(const_cast<char*>(data.data()), size));
  return data;
}

void EvBuffer::remove(void* data, size_t size) {
  int ret = evbuffer_remove(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove");
  } else if (static_cast<size_t>(ret) < size) {
    throw insufficient_data();
  }
}

string EvBuffer::remove(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->remove(const_cast<char*>(data.data()), size);
  return data;
}

size_t EvBuffer::copyout_atmost(void* data, size_t size) {
  ssize_t ret = evbuffer_copyout(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout");
  }
  return ret;
}

string EvBuffer::copyout_atmost(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->copyout_atmost(const_cast<char*>(data.data()), size));
  return data;
}

void EvBuffer::copyout(void* data, size_t size) {
  int ret = evbuffer_copyout(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout");
  } else if (static_cast<size_t>(ret) < size) {
    throw insufficient_data();
  }
}

string EvBuffer::copyout(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->copyout(const_cast<char*>(data.data()), size);
  return data;
}

size_t EvBuffer::copyout_from_atmost(const struct evbuffer_ptr* pos, void* data, size_t size) {
  ssize_t ret = evbuffer_copyout_from(this->buf, pos, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout_from");
  }
  return ret;
}

string EvBuffer::copyout_from_atmost(const struct evbuffer_ptr* pos, size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->copyout_from_atmost(pos, const_cast<char*>(data.data()), size));
  return data;
}

void EvBuffer::copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size) {
  ssize_t ret = evbuffer_copyout_from(this->buf, pos, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout_from");
  } else if (static_cast<size_t>(ret) < size) {
    throw insufficient_data();
  }
}

string EvBuffer::copyout_from(const struct evbuffer_ptr* pos, size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->copyout_from(pos, const_cast<char*>(data.data()), size);
  return data;
}

string EvBuffer::readln(enum evbuffer_eol_style eol_style) {
  size_t bytes_read;
  char* ret = evbuffer_readln(this->buf, &bytes_read, eol_style);
  if (ret) {
    // TODO: find a way to keep the API clean without having to copy the string
    // again here
    string str(ret, bytes_read);
    free(ret);
    return str;
  } else {
    throw insufficient_data();
  }
}

struct evbuffer_ptr EvBuffer::search(const char* what, size_t size,
    const struct evbuffer_ptr* start) {
  return evbuffer_search(this->buf, what, size, start);
}
struct evbuffer_ptr EvBuffer::search_range(const char* what, size_t size,
    const struct evbuffer_ptr* start, const struct evbuffer_ptr* end) {
  return evbuffer_search_range(this->buf, what, size, start, end);
}
struct evbuffer_ptr EvBuffer::search_eol(struct evbuffer_ptr* start,
    size_t* bytes_found, enum evbuffer_eol_style eol_style) {
  return evbuffer_search_eol(this->buf, start, bytes_found, eol_style);
}

void EvBuffer::ptr_set(struct evbuffer_ptr* pos, size_t position,
    enum evbuffer_ptr_how how) {
  if (evbuffer_ptr_set(this->buf, pos, position, how)) {
    throw runtime_error("evbuffer_ptr_set");
  }
}

int EvBuffer::peek(ssize_t size, struct evbuffer_ptr* start_at,
    struct evbuffer_iovec* vec_out, int n_vec) {
  return evbuffer_peek(this->buf, size, start_at, vec_out, n_vec);
}

int EvBuffer::reserve_space(ev_ssize_t size, struct evbuffer_iovec* vec, int n_vecs) {
  return evbuffer_reserve_space(this->buf, size, vec, n_vecs);
}

void EvBuffer::commit_space(struct evbuffer_iovec* vec, int n_vecs) {
  if (evbuffer_commit_space(this->buf, vec, n_vecs)) {
    throw runtime_error("evbuffer_commit_space");
  }
}

void EvBuffer::add_reference(const void* data, size_t size,
    void (*cleanup_fn)(const void* data, size_t size, void* ctx), void* ctx) {
  if (evbuffer_add_reference(this->buf, data, size, cleanup_fn, ctx)) {
    throw runtime_error("evbuffer_add_reference");
  }
}

void EvBuffer::add_reference(const string& data) {
  this->add_reference(data.data(), data.size(), nullptr, nullptr);
}

static void dispatch_cxx_cleanup_fn(const void* data, size_t size, void* ctx) {
  auto* fn = reinterpret_cast<function<void(const void* data, size_t size)>*>(ctx);
  (*fn)(data, size);
  delete fn;
}

void EvBuffer::add_reference(const void* data, size_t size,
    function<void(const void* data, size_t size)> cleanup_fn) {
  // TODO: can we do this without an extra allocation?
  auto* fn_copy = new function<void(const void* data, size_t size)>(cleanup_fn);
  int ret = evbuffer_add_reference(this->buf, data, size, dispatch_cxx_cleanup_fn, fn_copy);
  if (ret < 0) {
    delete fn_copy;
    throw runtime_error("evbuffer_add_reference");
  }
}

static void dispatch_delete_string(const void*, size_t, void* ctx) {
  delete reinterpret_cast<string*>(ctx);
}

void EvBuffer::add_reference(string&& data) {
  string* s = new string(std::move(data));
  int ret = evbuffer_add_reference(this->buf, s->data(), s->size(),
      dispatch_delete_string, s);
  if (ret < 0) {
    data = std::move(*s);
    delete s;
    throw runtime_error("evbuffer_add_reference");
  }
}

void EvBuffer::add_file(int fd, off_t offset, size_t size) {
  if (evbuffer_add_file(this->buf, fd, offset, size)) {
    throw runtime_error("evbuffer_add_file");
  }
}

void EvBuffer::add_buffer_reference(struct evbuffer* other_buf) {
  if (evbuffer_add_buffer_reference(this->buf, other_buf)) {
    throw runtime_error("evbuffer_add_buffer_reference");
  }
}

void EvBuffer::add_buffer_reference(EvBuffer& other_buf) {
  this->add_buffer_reference(other_buf.get());
}

void EvBuffer::freeze(int at_front) {
  if (evbuffer_freeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_freeze");
  }
}

void EvBuffer::unfreeze(int at_front) {
  if (evbuffer_unfreeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_unfreeze");
  }
}

void EvBuffer::debug_print_contents(FILE* stream) {
  size_t size = this->get_length();
  if (size) {
    print_data(stream, this->copyout(size));
  }
}
