/*
 * Host-test shim implementations: the mbedTLS surface sinricpro_signature.c
 * uses, backed by OpenSSL, so the real source compiles unmodified off-target.
 */

#include "mbedtls/md.h"
#include "mbedtls/base64.h"

#include <openssl/hmac.h>
#include <stdlib.h>
#include <string.h>

static const mbedtls_md_info_t sha256_info = { MBEDTLS_MD_SHA256 };

const mbedtls_md_info_t *mbedtls_md_info_from_type(mbedtls_md_type_t type)
{
    return type == MBEDTLS_MD_SHA256 ? &sha256_info : NULL;
}

void mbedtls_md_init(mbedtls_md_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

int mbedtls_md_setup(mbedtls_md_context_t *ctx, const mbedtls_md_info_t *info, int hmac)
{
    (void)hmac;
    if (info == NULL) {
        return -1;
    }
    ctx->info = info;
    return 0;
}

int mbedtls_md_hmac_starts(mbedtls_md_context_t *ctx, const unsigned char *key, size_t keylen)
{
    free(ctx->key);
    ctx->key = malloc(keylen ? keylen : 1);
    if (ctx->key == NULL) {
        return -1;
    }
    memcpy(ctx->key, key, keylen);
    ctx->key_len = keylen;
    ctx->data_len = 0;
    return 0;
}

int mbedtls_md_hmac_update(mbedtls_md_context_t *ctx, const unsigned char *input, size_t ilen)
{
    if (ctx->data_len + ilen > ctx->data_cap) {
        size_t cap = ctx->data_cap ? ctx->data_cap : 256;
        while (cap < ctx->data_len + ilen) {
            cap *= 2;
        }
        unsigned char *grown = realloc(ctx->data, cap);
        if (grown == NULL) {
            return -1;
        }
        ctx->data = grown;
        ctx->data_cap = cap;
    }
    memcpy(ctx->data + ctx->data_len, input, ilen);
    ctx->data_len += ilen;
    return 0;
}

int mbedtls_md_hmac_finish(mbedtls_md_context_t *ctx, unsigned char *output)
{
    unsigned int len = 0;
    if (HMAC(EVP_sha256(), ctx->key, (int)ctx->key_len,
             ctx->data, ctx->data_len, output, &len) == NULL || len != 32) {
        return -1;
    }
    return 0;
}

void mbedtls_md_free(mbedtls_md_context_t *ctx)
{
    free(ctx->key);
    free(ctx->data);
    memset(ctx, 0, sizeof(*ctx));
}

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t need = ((slen + 2) / 3) * 4;

    if (dst == NULL || dlen < need + 1) {
        *olen = need + 1;
        return -1;
    }

    size_t o = 0;
    size_t i = 0;

    for (; i + 2 < slen; i += 3) {
        unsigned v = ((unsigned)src[i] << 16) | ((unsigned)src[i + 1] << 8) | src[i + 2];
        dst[o++] = b64[(v >> 18) & 0x3F];
        dst[o++] = b64[(v >> 12) & 0x3F];
        dst[o++] = b64[(v >> 6) & 0x3F];
        dst[o++] = b64[v & 0x3F];
    }

    if (i < slen) {
        unsigned v = (unsigned)src[i] << 16;
        int rem = (int)(slen - i);
        if (rem == 2) {
            v |= (unsigned)src[i + 1] << 8;
        }
        dst[o++] = b64[(v >> 18) & 0x3F];
        dst[o++] = b64[(v >> 12) & 0x3F];
        dst[o++] = rem == 2 ? b64[(v >> 6) & 0x3F] : '=';
        dst[o++] = '=';
    }

    dst[o] = '\0';
    *olen = o;

    return 0;
}

static int base64_value(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    return c == '/' ? 63 : -1;
}

/* Mirrors mbedTLS: an empty input decodes to nothing, and a NULL or short
 * buffer reports the size needed through olen with BUFFER_TOO_SMALL. */
int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen)
{
    if (slen == 0) {
        *olen = 0;
        return 0;
    }
    if (slen % 4 != 0) {
        return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
    }

    size_t pad = 0;
    while (pad < 2 && src[slen - 1 - pad] == '=') {
        pad++;
    }
    for (size_t i = 0; i < slen - pad; i++) {
        if (base64_value(src[i]) < 0) {
            return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
        }
    }

    size_t need = slen / 4 * 3 - pad;
    if (dst == NULL || dlen < need) {
        *olen = need;
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    size_t o = 0;
    for (size_t i = 0; i < slen; i += 4) {
        unsigned v = 0;
        for (int k = 0; k < 4; k++) {
            unsigned char c = src[i + k];
            v = (v << 6) | (c == '=' ? 0u : (unsigned)base64_value(c));
        }
        if (o < need) {
            dst[o++] = (unsigned char)(v >> 16);
        }
        if (o < need) {
            dst[o++] = (unsigned char)(v >> 8);
        }
        if (o < need) {
            dst[o++] = (unsigned char)v;
        }
    }

    *olen = need;
    return 0;
}
