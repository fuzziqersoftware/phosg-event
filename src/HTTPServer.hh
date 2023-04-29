#define _STDC_FORMAT_MACROS

#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <inttypes.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "EvBuffer.hh"
#include "EvHTTPRequest.hh"
#include "EventBase.hh"

class HTTPServer {
public:
  HTTPServer(EventBase& base, std::shared_ptr<SSL_CTX> ssl_ctx = nullptr);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer();

  void add_socket(int fd);

  void set_server_name(const char* server_name);

protected:
  EventBase base;
  struct evhttp* http;
  std::shared_ptr<SSL_CTX> ssl_ctx;
  std::string server_name;

  static struct bufferevent* dispatch_on_ssl_connection(struct event_base* base,
      void* ctx);
  static void dispatch_handle_request(struct evhttp_request* req, void* ctx);

  void send_response(EvHTTPRequest& req, int code, const char* content_type,
      EvBuffer& b);
  void send_response(EvHTTPRequest& req, int code, const char* content_type,
      const char* fmt, ...);
  void send_response(EvHTTPRequest& req, int code, const char* content_type = nullptr);

  virtual void handle_request(EvHTTPRequest& req) = 0;

  static const std::unordered_map<int, const char*> explanation_for_response_code;
};
