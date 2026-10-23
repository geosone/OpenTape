#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
test "${MHVTL_ALLOW_OVERWRITE:-}" = OA2101L8 || {
    echo 'Set MHVTL_ALLOW_OVERWRITE=OA2101L8 for the disposable virtual scratch tape' >&2
    exit 1
}
OPENTAPE_TAPE_TEST=file-write bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=file-read bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=file-verify bash "$ROOT/tests/dos_status.sh"
echo 'OpenTape DOS real-file backup/readback passed'
