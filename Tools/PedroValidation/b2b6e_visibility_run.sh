#!/usr/bin/env bash
# Safe UE Editor-only read-only sight probe / one-observer-rotation authoring.
set -euo pipefail
MODE=${1:-}
case "$MODE" in probe-test|align-test|probe-production|align-production) ;; *)
  echo 'Usage: b2b6e_visibility_run.sh probe-test|align-test|probe-production|align-production' >&2;exit 2;;
esac
[[ $# -eq 1 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init
pedro_lock
map_kind=${MODE#*-}
if [[ "$map_kind" == test ]];then
  MAPFILE=Content/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest.umap
else
  MAPFILE=Content/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype.umap
fi
[[ -f "$MAPFILE" ]] || { echo "STOP: expected map $MAPFILE missing" >&2;exit 1; }
PROBE_PATH=''
if [[ "$MODE" == align-* ]];then
  pointer="Saved/PedroValidation/b2b6e-visibility-probe-$map_kind-latest.txt"
  [[ -s "$pointer" ]] || { echo 'STOP: run read-only sightline probe first';exit 1; }
  PROBE_RUN=$(cat "$pointer")
  [[ "$PROBE_RUN" == "$PROJECT"/Saved/PedroValidation/b2b6e-visibility-probe-"$map_kind"-* &&
     -d "$PROBE_RUN" && ! -L "$PROBE_RUN" && ! -L "$PROBE_RUN/results.json" &&
     -s "$PROBE_RUN/results.json" ]] || { echo 'STOP: missing previous probe report';exit 1; }
  PROBE_PATH="$PROBE_RUN/results.json"
  run_nix python3 -B - "$PROBE_PATH" "$map_kind" <<'VIS_PROBE_PY'
import json,sys
from pathlib import Path
report=Path(sys.argv[1])
result=json.loads(report.read_text(encoding='utf-8'))
expected='probe-'+sys.argv[2]
choice=result.get('candidate')
if not (result.get('status')=='PASS' and result.get('mode')==expected and
        result.get('authoring')=='NONE_READ_ONLY' and
        result.get('rotation_needed') is True and isinstance(choice,dict) and
        0<choice.get('visible',0)<4 and choice.get('hidden')==4-choice.get('visible')):
    print('STOP: previous sightline probe is not approved for map authoring. '
          'status='+str(result.get('status'))+' reason='+str(result.get('reason')),
          file=sys.stderr)
    raise SystemExit(1)
VIS_PROBE_PY
fi
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b6e-visibility-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" >"Saved/PedroValidation/b2b6e-visibility-$MODE-latest.txt"
echo "Reports: $RUN"
cp -p "$DESCRIPTOR" "$RUN/original.uproject"
REPORT="$RUN/results.json"
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py > "$RUN/preflight.log"
git lfs fsck > "$RUN/lfs-before.log"
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" before > "$RUN/snapshot-before.log"
SNAPSHOTTED=1
RESTORED=0
finish() {
  code=$?
  trap - EXIT
  set +e
  if (( ! RESTORED ));then
    cp -p "$RUN/original.uproject" "$DESCRIPTOR" || code=1
  fi
  cmp -s "$RUN/original.uproject" "$DESCRIPTOR" || code=1
  if (( SNAPSHOTTED ));then
    if [[ ! -s "$RUN/files-after.sha256" ]];then
      run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after \
        > "$RUN/snapshot-after.log" 2>&1 || code=1
    fi
  fi
  if ((code!=0));then
    echo "STOP: sightline $MODE failed. Retained: $RUN" >&2
    echo 'Do not rerun authoring or modify maps manually.' >&2
  fi
  exit "$code"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
enable_editor_plugins
export B2B6E_PROJECT="$PROJECT" B2B6E_SCRIPT_DIR="$PROJECT/Tools/PedroValidation"
export B2B6E_VIS_MODE="$MODE" B2B6E_VIS_REPORT="$REPORT" B2B6E_VIS_PROBE="$PROBE_PATH"
export B2B6E_PYCACHE_PREFIX="$RUN/python-cache" PYTHONPYCACHEPREFIX="$RUN/python-cache" PYTHONDONTWRITEBYTECODE=1
RC=0
run_nix timeout --kill-after=15s 600s steam-run "$UE" "$DESCRIPTOR" -nop4 \
  "-ExecutePythonScript=$PROJECT/Tools/PedroValidation/b2b6e_visibility_editor.py" \
  "-abslog=$RUN/editor.log" > "$RUN/terminal.log" 2>&1 || RC=$?
printf '%s\n' "$RC" >"$RUN/editor-exit.txt"
cp -p "$RUN/original.uproject" "$DESCRIPTOR"
RESTORED=1
cmp -s "$RUN/original.uproject" "$DESCRIPTOR" || { echo 'STOP: descriptor not restored';exit 1; }
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after > "$RUN/snapshot-after.log"
if [[ -s "$REPORT" ]];then
  run_nix python3 -B Tools/PedroValidation/b2b6e_visibility_diagnose.py \
    --project "$PROJECT" --run "$RUN" | tee "$RUN/diagnostics.txt"
fi
if ((RC!=0)) || [[ ! -s "$REPORT" || ! -s "$RUN/editor.log" ]];then
  tail -80 "$RUN/terminal.log" || true
  exit 1
fi
check_fatal "$RUN/editor.log"
run_nix python3 -B Tools/PedroValidation/b2b6e_visibility_audit.py compare "$RUN" "$MODE"
if [[ "$MODE" == align-* ]];then
  git add -- "$MAPFILE"
  run_nix python3 -B Tools/PedroValidation/b2b6e_visibility_audit.py staged "$PROJECT" "$map_kind"
  run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py | tee "$RUN/stage-audit.log"
  echo "PASS: verified one-map rotation was authored through Unreal Editor and staged: $MAPFILE"
else
  echo 'PASS: read-only saved-map visibility and grounded rotation plan'
fi
echo "Reports: $RUN"
