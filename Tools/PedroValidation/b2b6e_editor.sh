#!/usr/bin/env bash
# Pedro-only mechanical Unreal Editor operations with strict integrity checks.
set -euo pipefail
MODE=${1:-inventory}
case "$MODE" in
  inventory|probe|verify-test|verify-production|verify|prepare-test|author-test|build-test-nav|author-production|build-production-nav) ;;
  *) echo 'Usage: b2b6e_editor.sh inventory|probe|verify-test|verify-production|prepare-test|build-test-nav|author-test|build-production-nav|author-production' >&2; exit 2 ;;
esac
[[ $# -eq 1 ]] || { echo 'Unexpected editor arguments' >&2; exit 2; }
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init
pedro_lock
WRITE=no
case "$MODE" in prepare-test|author-test|build-test-nav|author-production|build-production-nav) WRITE=yes ;; esac
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b6e-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" >"Saved/PedroValidation/b2b6e-$MODE-latest.txt"
echo "Reports: $RUN"
AUDIT="$PROJECT/Tools/PedroValidation/b2b6e_saved_map_audit.py"
AUTHOR="$PROJECT/Tools/PedroValidation/b2b6e_author_level.py"
SCRIPT="$AUDIT"
[[ "$WRITE" == no ]] || SCRIPT="$AUTHOR"
# Redirect caches even if the embedded Python interpreter does not honor
# PYTHONDONTWRITEBYTECODE. Entry points also disable bytecode explicitly.
export B2B6E_AUDIT_MODE="$MODE" B2B6E_AUDIT_REPORT="$RUN/results.json" B2B6E_SCRIPT_DIR="$PROJECT/Tools/PedroValidation" PYTHONDONTWRITEBYTECODE=1 PYTHONPYCACHEPREFIX="$RUN/python-cache" B2B6E_PYCACHE_PREFIX="$RUN/python-cache"
cp -p "$DESCRIPTOR" "$RUN/original.uproject"
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" before
finalize() {
  local rc=$?
  trap - EXIT
  set +e
  cp -p "$RUN/original.uproject" "$DESCRIPTOR" || rc=1
  cmp -s "$RUN/original.uproject" "$DESCRIPTOR" || { echo 'FAIL: descriptor restoration' >&2; rc=1; }
  run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after || rc=1
  # Always record path-level differences, including generated/untracked files.
  # Do not silently remove, ignore or stage any new file.
  if [[ -f "$RUN/files-after.sha256" ]]; then
    run_nix python3 -B Tools/PedroValidation/b2b6e_integrity.py snapshots "$RUN" \
      >"$RUN/snapshot-diff.log" 2>&1 || rc=1
  fi
  if [[ "$WRITE" == no ]]; then
    verify_readonly_snapshot "$RUN" || {
      [[ ! -f "$RUN/snapshot-diff.log" ]] || cat "$RUN/snapshot-diff.log" >&2
      rc=1
    }
  else
    run_nix python3 -B Tools/PedroValidation/b2b6e_write_scope.py \
      "$MODE" "$RUN/files-before.sha256" "$RUN/files-after.sha256" "$RUN/write-scope.json" "$RUN/results.json" || rc=1
  fi
  git lfs fsck >"$RUN/lfs-fsck.log" 2>&1 || rc=1
  if ((rc == 0)); then echo "PASS: B2b6e Editor $MODE"; else echo "FAIL: B2b6e Editor $MODE" >&2; fi
  echo "Reports: $RUN"
  exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
enable_editor_plugins
rc=0
run_nix timeout --kill-after=15s 600s steam-run \
  "$UE" "$DESCRIPTOR" -nop4 "-ExecutePythonScript=$SCRIPT" "-abslog=$RUN/editor.log" \
  >"$RUN/terminal.log" 2>&1 || rc=$?
printf '%s\n' "$rc" > "$RUN/editor-exit.txt"
if ((rc != 0)); then tail -n 80 "$RUN/terminal.log"; exit "$rc"; fi
[[ -s "$RUN/editor.log" && -s "$RUN/results.json" ]] || {
  echo 'FAIL: missing Editor log or results'; tail -n 40 "$RUN/terminal.log"; exit 1;
}
check_fatal "$RUN/editor.log"
run_nix python3 -B - "$RUN/results.json" "$MODE" <<'PY'
import json,sys
from pathlib import Path
p=json.loads(Path(sys.argv[1]).read_text())
if p.get('status')!='PASS' or p.get('mode')!=sys.argv[2]:
    raise SystemExit('STOP: Unreal process did not report PASS for requested mode: '+str(p))
if sys.argv[2].startswith('verify') and not p.get('maps'):
    raise SystemExit('STOP: read-only verification contains no map details')
print('PASS: actual Unreal process emitted a verified result')
PY
