/* Host-test shim: mbedTLS base64 surface used by sinricpro_signature.c and camera_controller.c. */
#ifndef SINRICPRO_HOST_SHIM_MBEDTLS_BASE64_H
#define SINRICPRO_HOST_SHIM_MBEDTLS_BASE64_H

#include <stddef.h>

#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL  -0x002A
#define MBEDTLS_ERR_BASE64_INVALID_CHARACTER -0x002C

int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

int mbedtls_base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                          const unsigned char *src, size_t slen);

#endif
