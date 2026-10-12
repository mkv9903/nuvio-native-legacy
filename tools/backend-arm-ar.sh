#!/usr/bin/env bash
set -euo pipefail
exec "${NUVIO_BACKEND_RUNTIME:-docker}" exec "${NUVIO_BACKEND_SDK:?SDK container is not running}" arm-webos-linux-gnueabi-ar "$@"
