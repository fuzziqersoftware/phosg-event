#include "SSL.hh"

#include <openssl/err.h>
#include <openssl/ssl.h>

using namespace std;

void openssl_global_init() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

SSL_CTX* openssl_create_default_context(
    const string& key_filename,
    const string& cert_filename,
    const string& ca_cert_filename) {
  SSL_CTX* ctx = SSL_CTX_new(TLS_method());
  if (!ctx) {
    throw runtime_error("cannot allocate SSL context");
  }
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  SSL_CTX_set_cipher_list(ctx, "ECDH+AESGCM:ECDH+CHACHA20:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS:!AESCCM:!RSA");
  SSL_CTX_set_ecdh_auto(ctx, 1);
  if (!ca_cert_filename.empty()) {
    SSL_CTX_load_verify_locations(ctx, ca_cert_filename.c_str(), nullptr);
  }
  if (SSL_CTX_use_certificate_file(ctx, cert_filename.c_str(), SSL_FILETYPE_PEM) <= 0) {
    throw runtime_error("cannot open SSL certificate file " + cert_filename);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, key_filename.c_str(), SSL_FILETYPE_PEM) <= 0) {
    throw runtime_error("cannot open SSL key file " + key_filename);
  }

  return ctx;
}

unique_ptr<SSL_CTX, void (*)(SSL_CTX*)> openssl_create_default_context_unique(
    const string& key_filename,
    const string& cert_filename,
    const string& ca_cert_filename) {
  auto* ctx = openssl_create_default_context(
      key_filename, cert_filename, ca_cert_filename);
  return unique_ptr<SSL_CTX, void (*)(SSL_CTX*)>(ctx, SSL_CTX_free);
}

shared_ptr<SSL_CTX> openssl_create_default_context_shared(
    const string& key_filename,
    const string& cert_filename,
    const string& ca_cert_filename) {
  auto* ctx = openssl_create_default_context(
      key_filename, cert_filename, ca_cert_filename);
  return shared_ptr<SSL_CTX>(ctx, SSL_CTX_free);
}
