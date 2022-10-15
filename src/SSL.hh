#pragma once

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <string>
#include <memory>



void openssl_global_init();

SSL_CTX* openssl_create_default_context(
    const std::string& key_filename,
    const std::string& cert_filename,
    const std::string& ca_cert_filename);
std::unique_ptr<SSL_CTX, void(*)(SSL_CTX*)> openssl_create_default_context_unique(
    const std::string& key_filename,
    const std::string& cert_filename,
    const std::string& ca_cert_filename);
std::shared_ptr<SSL_CTX> openssl_create_default_context_shared(
    const std::string& key_filename,
    const std::string& cert_filename,
    const std::string& ca_cert_filename);
