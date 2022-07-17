#pragma once

#include <event2/event.h>
#include <event2/http.h>

#include <unordered_map>
#include <string>

#include "EvBuffer.hh"



class EvHTTPRequest {
public:
  EvHTTPRequest();
  EvHTTPRequest(struct evhttp_request* req);
  EvHTTPRequest(const EvHTTPRequest& req);
  EvHTTPRequest(EvHTTPRequest&& req);
  EvHTTPRequest& operator=(const EvHTTPRequest& req);
  EvHTTPRequest& operator=(EvHTTPRequest&& req);
  ~EvHTTPRequest() = default;

  std::unordered_multimap<std::string, std::string> parse_url_params(
      const char* query = nullptr);
  std::unordered_map<std::string, std::string> parse_url_params_unique(
      const char* query = nullptr);

  enum evhttp_cmd_type get_command() const;
  struct evhttp_connection* get_connection();
  const struct evhttp_uri* get_evhttp_uri() const;
  const char* get_host() const;

  EvBuffer get_input_buffer();
  EvBuffer get_output_buffer();

  struct evkeyvalq* get_input_headers();
  struct evkeyvalq* get_output_headers();
  const char* get_input_header(const char* header_name);
  void add_output_header(const char* header_name, const char* value);

  int get_response_code();
  const char* get_uri();
  int is_owned();

  void own();
  void set_chunked_cb(void(*cb)(struct evhttp_request *, void *));

  struct evhttp_request* get();

protected:
  static void dispatch_on_response(struct evhttp_request* req, void* ctx);
  virtual void on_response();

  struct evhttp_request* req;
  bool owned;
};
