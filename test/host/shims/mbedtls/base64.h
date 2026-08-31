/* Host-test shim: mbedTLS base64 surface used by sinricpro_signature.c. */
#ifndef SINRICPRO_HOST_SHIM_MBEDTLS_BASE64_H
#define SINRICPRO_HOST_SHIM_MBEDTLS_BASE64_H

#include <stddef.h>

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

#endif
