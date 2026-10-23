#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
test "${MHVTL_ALLOW_OVERWRITE:-}" = OA2101L8 || exit 1
export OPENTAPE_TREE_LARGE=${OPENTAPE_TREE_LARGE:-1}
OPENTAPE_TAPE_TEST=tree-write bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=tree-restore bash "$ROOT/tests/dos_status.sh"
OPENTAPE_TAPE_TEST=tree-replace bash "$ROOT/tests/dos_status.sh"
echo 'OpenTape DOS per-file replacement passed'
