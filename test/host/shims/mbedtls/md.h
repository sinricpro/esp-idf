/* Host-test shim: mbedTLS HMAC surface used by sinricpro_signature.c, over OpenSSL. */
#ifndef SINRICPRO_HOST_SHIM_MBEDTLS_MD_H
#define SINRICPRO_HOST_SHIM_MBEDTLS_MD_H

#include <stddef.h>

typedef enum { MBEDTLS_MD_SHA256 = 6 } mbedtls_md_type_t;

typedef struct { mbedtls_md_type_t type; } mbedtls_md_info_t;

typedef struct {
    const mbedtls_md_info_t *info;
    unsigned char *key;
    size_t key_len;
    unsigned char *data;
    size_t data_len;
    size_t data_cap;
} mbedtls_md_context_t;

const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t type);
void mbedtls_md_init(mbedtls_md_context_t *ctx);
int  mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *info, int hmac);
int  mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key, size_t keylen);
int  mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen);
int  mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output);
void mbedtls_md_free(mbedtls_md_context_t *ctx);

#endif
