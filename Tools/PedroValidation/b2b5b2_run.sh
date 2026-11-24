#!/usr/bin/env bash
# Read-only saved-map verification. Original authoring workflow is in Git history.
set -euo pipefail
MODE=${1:-verify}
[[ $# -le 1 && "$MODE" == verify ]] || { echo "Usage: $0 [verify]" >&2; exit 2; }
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init
SCRIPT="$PROJECT/Tools/PedroValidation/b2b5b2_editor.py"
AUDIT=Tools/PedroValidation/b2b6a_audit.py
[[ -f "$SCRIPT" ]]
pedro_lock
run_nix python3 -B "$AUDIT"
git diff --quiet HEAD -- "$DESCRIPTOR" || { echo 'STOP: Descriptor has existing edits.' >&2; exit 1; }
git lfs fsck
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b5b2-verify-XXXXXXXX")
echo "Reports: $RUN"
printf '%s\n' "$RUN" > Saved/PedroValidation/b2b5b2-verify-latest.txt
run_nix python3 -B "$AUDIT" --snapshot "$RUN" before
BACKUP="$RUN/original.uproject"
cp -p "$DESCRIPTOR" "$BACKUP"
finalize() {
    local rc=$?
    trap - EXIT
    set +e
    cp -p "$BACKUP" "$DESCRIPTOR" || rc=1
    cmp -s "$BACKUP" "$DESCRIPTOR" || { echo 'FAIL: Descriptor restoration' >&2; rc=1; }
    run_nix python3 -B "$AUDIT" --snapshot "$RUN" after || rc=1
    verify_readonly_snapshot "$RUN" || rc=1
    run_nix python3 -B "$AUDIT" || rc=1
    git lfs fsck || rc=1
    if (( rc == 0 )); then echo 'PASS: Saved-map verification and repository integrity'; else echo 'FAIL: Saved-map verification; inspect reports' >&2; fi
    echo "Reports: $RUN"
    exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
enable_editor_plugins
export B2B5B2_MODE=verify
export B2B5B2_RUN_ID="$(basename "$RUN")"
export B2B5B2_RESULT_PATH="$RUN/result.json"
export B2B5B2_PROJECT="$PROJECT"
rc=0
run_nix timeout --kill-after=15s 420s steam-run \
    "$ENGINE/Engine/Binaries/Linux/UnrealEditor" "$DESCRIPTOR" -nop4 \
    "-ExecutePythonScript=$SCRIPT" "-abslog=$RUN/editor.log" \
    > "$RUN/terminal.log" 2>&1 || rc=$?
if (( rc != 0 )); then tail -n 60 "$RUN/terminal.log"; exit 1; fi
check_fatal "$RUN/editor.log"
[[ -s "$RUN/editor.log" ]]
run_nix python3 -B - "$B2B5B2_RESULT_PATH" "$B2B5B2_RUN_ID" <<'PY'
import json
import sys
from pathlib import Path
r = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
if (r.get('status'), r.get('mode'), r.get('run_id')) != ('PASS', 'verify', sys.argv[2]):
    raise SystemExit('FAIL: ' + str(r))
print('PASS: Saved configuration verified in this Editor process')
PY
