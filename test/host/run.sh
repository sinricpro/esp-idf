#!/usr/bin/env bash
# Build and run the host tests for the signing / verification wire contract.
#
# Compiles the real src/core/sinricpro_signature.c off-target against shim
# headers (OpenSSL stands in for mbedTLS), so what is tested is the shipped
# source rather than a re-implementation of it.
#
#   test/host/run.sh
#
# cJSON is taken from the example's managed_components if present; override
# with CJSON_DIR=/path/to/cJSON.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"

cjson_dir="${CJSON_DIR:-$root/examples/switch/managed_components/espressif__cjson/cJSON}"

if [ ! -f "$cjson_dir/cJSON.c" ]; then
    echo "cJSON sources not found at $cjson_dir" >&2
    echo "Build an example once (idf.py reconfigure) or set CJSON_DIR." >&2
    exit 1
fi

out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -O1 -g \
    -I "$here/shims" \
    -I "$root/src/core" \
    -I "$root/include" \
    -I "$cjson_dir" \
    "$here/test_signature.c" \
    "$here/shims/shims.c" \
    "$root/src/core/sinricpro_signature.c" \
    "$cjson_dir/cJSON.c" \
    -lcrypto -lm \
    -o "$out/test_signature"

"$out/test_signature"
