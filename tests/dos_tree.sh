#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
test "${MHVTL_ALLOW_OVERWRITE:-}" = OA2101L8 || exit 1
OPENTAPE_TAPE_TEST=tree-write bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=tree-read bash "$ROOT/tests/dos_status.sh"
if test "${OPENTAPE_TREE_LARGE:-0}" = 1; then
    OPENTAPE_TAPE_TEST=tree-limited bash "$ROOT/tests/dos_status.sh"
fi
OPENTAPE_TAPE_TEST=tree-verify bash "$ROOT/tests/dos_status.sh"
echo 'OpenTape DOS tree backup/readback passed'
