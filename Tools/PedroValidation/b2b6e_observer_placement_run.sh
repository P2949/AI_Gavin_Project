#!/usr/bin/env bash
# Two distinct Editor invocations. No map mutation without a current, genuine probe.
set -euo pipefail
MODE=${1:-}
case "$MODE" in probe-test|apply-test|probe-production|apply-production) ;; *)
  echo 'Usage: b2b6e_observer_placement_run.sh probe-test|apply-test|probe-production|apply-production' >&2;exit 2;;
esac
[[ $# -eq 1 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init
pedro_lock
KIND=${MODE#*-}
if [[ "$KIND" == test ]];then
  MAPFILE=Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap
else
  MAPFILE=Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype.umap
fi
[[ -f "$MAPFILE" ]] || { echo "STOP: missing map $MAPFILE" >&2;exit 1; }
PROBE_PATH=''
if [[ "$MODE" == apply-* ]];then
  POINTER="Saved/PedroValidation/b2b6e-place-probe-$KIND-latest.txt"
  [[ -s "$POINTER" ]] || { echo 'STOP: no successful placement probe' >&2;exit 1; }
  PROBE_RUN=$(cat "$POINTER")
  [[ "$PROBE_RUN" == "$PROJECT"/Saved/PedroValidation/b2b6e-place-probe-"$KIND"-* &&
     -d "$PROBE_RUN" && ! -L "$PROBE_RUN" && -s "$PROBE_RUN/results.json" &&
     ! -L "$PROBE_RUN/results.json" ]] || { echo 'STOP: invalid saved probe path' >&2;exit 1; }
  PROBE_PATH="$PROBE_RUN/results.json"
  run_nix python3 -B - "$PROBE_PATH" "$KIND" <<'PY'
import json,sys
from pathlib import Path
s=json.loads(Path(sys.argv[1]).read_text()); c=s.get('candidate')
if not(s.get('status')=='PASS' and s.get('mode')=='probe-'+sys.argv[2] and
       s.get('authoring')=='NONE_READ_ONLY' and isinstance(c,dict) and
       isinstance(c.get('sight'),dict) and c['sight'].get('visible',0)>=1 and
       c['sight'].get('hidden',0)>=1 and c.get('movement_path_cm',0)>=150.):
    raise SystemExit('STOP: previous location probe is not approved for map authoring')
PY
fi
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b6e-place-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" >"Saved/PedroValidation/b2b6e-place-$MODE-latest.txt"
echo "Reports: $RUN"
AUDIT=Tools/PedroValidation/b2b6e_observer_placement_audit.py
cp -p "$DESCRIPTOR" "$RUN/original.uproject"
RESTORED=0; SNAPSHOTTED=0
finish() {
  code=$?
  trap - EXIT
  set +e
  if (( ! RESTORED ));then cp -p "$RUN/original.uproject" "$DESCRIPTOR" || code=1;fi
  cmp -s "$RUN/original.uproject" "$DESCRIPTOR" || code=1
  if (( SNAPSHOTTED )) && [[ ! -s "$RUN/files-after.sha256" ]];then
    run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after >"$RUN/snapshot-after.log" 2>&1 || code=1
  fi
  if ((code!=0));then
    echo "STOP: placement $MODE failed. Retained: $RUN" >&2
    echo 'Do not manually edit or restage any Unreal asset; inspect results.json and snapshots.' >&2
  fi
  exit "$code"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py > "$RUN/preflight.log"
git lfs fsck > "$RUN/lfs-before.log"
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" before > "$RUN/snapshot-before.log"
SNAPSHOTTED=1
enable_editor_plugins
export B2B6E_PROJECT="$PROJECT" B2B6E_SCRIPT_DIR="$PROJECT/Tools/PedroValidation"
export B2B6E_PLACE_MODE="$MODE" B2B6E_PLACE_REPORT="$RUN/results.json" B2B6E_PLACE_PROBE="$PROBE_PATH"
export B2B6E_VIS_MODE="probe-$KIND" B2B6E_VIS_REPORT="$RUN/unused-original-probe.json"
export B2B6E_PYCACHE_PREFIX="$RUN/python-cache" PYTHONPYCACHEPREFIX="$RUN/python-cache" PYTHONDONTWRITEBYTECODE=1
RC=0
run_nix timeout --kill-after=15s 600s steam-run "$UE" "$DESCRIPTOR" -nop4 \
  "-ExecutePythonScript=$PROJECT/Tools/PedroValidation/b2b6e_observer_placement_editor.py" \
  "-abslog=$RUN/editor.log" > "$RUN/terminal.log" 2>&1 || RC=$?
printf '%s\n' "$RC" >"$RUN/editor-exit.txt"
cp -p "$RUN/original.uproject" "$DESCRIPTOR"; RESTORED=1
cmp -s "$RUN/original.uproject" "$DESCRIPTOR" || { echo 'STOP: uproject restoration failed';exit 1; }
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after >"$RUN/snapshot-after.log"
if [[ -s "$RUN/results.json" ]];then
  run_nix python3 -B - "$RUN/results.json" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]));print('PEDRO_PLACEMENT_EDITOR_RESULT',json.dumps({k:p.get(k) for k in ('status','mode','reason','search','candidate')},sort_keys=True))
PY
fi
if ((RC!=0)) || [[ ! -s "$RUN/results.json" || ! -s "$RUN/editor.log" ]];then
  tail -n 70 "$RUN/terminal.log" || true
  exit 1
fi
check_fatal "$RUN/editor.log"
run_nix python3 -B "$AUDIT" compare "$RUN" "$MODE"
if [[ "$MODE" == apply-* ]];then
  # Only a proof-backed single map can be staged; all other entries unchanged.
  git add -- "$MAPFILE"
  run_nix python3 -B "$AUDIT" staged "$PROJECT" "$KIND"
  run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py > "$RUN/stage-audit.log"
  echo "PASS: only approved Pedro $KIND map was changed through Unreal Editor and staged"
else
  echo 'PASS: read-only location and sightline plan; no maps modified'
fi
echo "Reports: $RUN"
