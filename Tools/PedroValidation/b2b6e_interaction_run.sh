#!/usr/bin/env bash
# Saved-world, source-only, component-level real-interaction regression.
# Keeps the source project's existing all suite and binary maps untouched.
set -euo pipefail
MODE=${1:-}
case "$MODE" in test|test-rendered|production|production-rendered) ;; *)
  printf 'Usage: %s test|test-rendered|production|production-rendered\n' "$0" >&2; exit 2;;
esac
[[ $# == 1 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init build
case "$MODE" in
  production*) MAP=/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype ;;
  test*) MAP=/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest ;;
esac
RHI=(-nullrhi)
[[ "$MODE" == *-rendered ]] && RHI=()
pedro_lock
RUN=$(mktemp -d "$PWD/Saved/PedroValidation/b2b6e-interaction-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" > "Saved/PedroValidation/b2b6e-interaction-$MODE-latest.txt"
echo "Reports: $RUN"
STATUS=1
FINALIZE=0
finish() {
  local original=$? audit=1
  set +e
  trap - EXIT
  if (( FINALIZE )); then
    audit=0
    run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" after > "$RUN/snapshot-after.log" 2>&1 || audit=1
    verify_readonly_snapshot "$RUN" || audit=1
    run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py > "$RUN/audit-after.log" 2>&1 || audit=1
    git lfs fsck > "$RUN/lfs-after.log" 2>&1 || audit=1
  fi
  if (( original || audit )); then STATUS=1; else STATUS=0; fi
  run_nix python3 -B - "$RUN" "$MODE" "$STATUS" "$audit" <<'PY'
import json,sys
from pathlib import Path
run=Path(sys.argv[1]);mode=sys.argv[2];status=int(sys.argv[3]);audit=int(sys.argv[4]);
result={'status':'PASS' if status==0 else 'FAIL','mode':mode,'exit_status':status,
        'repository_audit':'PASS' if audit==0 else 'FAIL',
        'interaction_smoke':'PASS' if status==0 else 'UNVERIFIED_OR_FAIL',
        'full_encounter_completion':'UNVERIFIED','hub_travel':'UNVERIFIED',
        'manual_input':'UNVERIFIED','shipping':'UNVERIFIED'}
(run/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
PY
  if (( $? != 0 )); then STATUS=1; fi
  echo "Reports: $RUN"
  exit "$STATUS"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py | tee "$RUN/preflight.log"
git lfs fsck > "$RUN/lfs-before.log"
run_nix python3 -B Tools/PedroValidation/b2b6a_audit.py --snapshot "$RUN" before > "$RUN/snapshot-before.log"
FINALIZE=1
echo '=== Source verification ==='
run_nix python3 -B -m unittest discover -s Tools/PedroValidation/tests -p test_b2b6e_interaction_smoke.py -v | tee "$RUN/source-tests.log"
echo '=== Ordinary UE 5.8.2 compile ==='
if ! run_nix bash "$BUILD" AI_Gavin_ProjectEditor Linux Development "-Project=$DESCRIPTOR" -WaitMutex -NoUBTMakefiles > "$RUN/build.log" 2>&1; then
  tail -n 90 "$RUN/build.log";exit 1
fi
grep -q 'Result: Succeeded' "$RUN/build.log" || { tail -n 90 "$RUN/build.log";exit 1; }
echo 'PASS: ordinary-build'
echo '=== Native game-process real interaction ==='
RC=0
run_nix timeout --kill-after=15s 420s steam-run "$UE" "$DESCRIPTOR" "$MAP" \
  -game "${RHI[@]}" -unattended -nosound -nop4 -nosplash -log \
  -PedroInteractionSmoke "-PedroInteractionExpectedMap=$MAP" \
  '-ExecCmds=t.MaxFPS 120' "-abslog=$RUN/game.log" > "$RUN/terminal.log" 2>&1 || RC=$?
printf '%d\n' "$RC" > "$RUN/exit-status.txt"
[[ -s "$RUN/game.log" ]] || { tail -n 70 "$RUN/terminal.log";exit 1; }
grep -E 'PEDRO_INTERACTION_|Engine exit requested' "$RUN/game.log" || true
run_nix python3 -B Tools/PedroValidation/verify_interaction_smoke.py \
  "$RUN/game.log" "$RC" "$MAP" "$RUN/results.json"
echo 'PASS: real component-level interaction; manual input, completion and hub remain unverified'
