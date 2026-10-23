#!/bin/bash
# One destructive write followed by a separate boot and read-only verification.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
test "${MHVTL_ALLOW_OVERWRITE:-}" = OA2101L8 || {
    echo 'Set MHVTL_ALLOW_OVERWRITE=OA2101L8 to overwrite only this virtual scratch tape' >&2
    exit 1
}
OPENTAPE_TAPE_TEST=write bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=verify bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=cli-verify bash "$ROOT/tests/dos_status.sh"
echo 'OpenTape DOS native write/readback roundtrip passed'
