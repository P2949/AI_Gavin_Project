#!/usr/bin/env bash
# Run in a CHILD shell. Never `set -e` in the interactive shell.
# Every UE process and audit result persists in Saved/PedroValidation.
set -euo pipefail
MODE=${1:-}
case "$MODE" in
  probe|test-completion|test-completion-rendered|test-early|test-early-rendered|production-completion|production-completion-rendered|production-early|production-early-rendered|all) ;;
  *) echo 'Usage: b2b6f_flow_run.sh probe|test-completion[-rendered]|test-early[-rendered]|production-completion[-rendered]|production-early[-rendered]|all' >&2; exit 2;;
esac
[[ $# == 1 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init build
UPROJECT="$DESCRIPTOR"
TOOLS=Tools/PedroValidation
pedro_lock
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b6f-flow-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" > "Saved/PedroValidation/b2b6f-flow-$MODE-latest.txt"
printf '%s\n' "$RUN" > "Saved/PedroValidation/b2b6f-flow-latest.txt"
echo "Reports: $RUN"
printf '%s\n' "$MODE" >"$RUN/mode.txt"
: >"$RUN/completed.txt"
PREPARED=0
finish() {
  local rc=$? audit=0
  trap - EXIT
  set +e
  if (( PREPARED )); then
    run_nix python3 -B "$TOOLS/b2b6a_audit.py" --snapshot "$RUN" after >"$RUN/snapshot-after.log" 2>&1 || audit=1
    verify_readonly_snapshot "$RUN" || audit=1
    run_nix python3 -B "$TOOLS/b2b6a_audit.py" >"$RUN/audit-after.log" 2>&1 || audit=1
  fi
  git lfs fsck >"$RUN/lfs-after.log" 2>&1 || audit=1
  if (( audit )); then rc=1; fi
  run_nix python3 -B - "$RUN" "$MODE" "$rc" "$audit" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);mode=sys.argv[2];rc=int(sys.argv[3]);audit=int(sys.argv[4])
stages=(p/'completed.txt').read_text().splitlines()
required=0 if mode=='probe' else 8 if mode=='all' else 1
runs=sorted(p.glob('*/result.json'))
passed=rc==0 and len(runs)==required and all(json.loads(x.read_text()).get('status')=='PASS' for x in runs)
if mode=='probe':passed=passed and 'preflight' in stages
else:passed=passed and 'ordinary-build' in stages
result={'status':'PASS' if passed else 'FAIL','mode':mode,'exit_status':0 if passed else max(rc,1),
        'repository_unchanged':audit==0,'completed_stages':stages,
        'verified_game_processes':len(runs),'expected_game_processes':required,
        'game_process_reports':{x.parent.name:str(x.relative_to(p)) for x in runs},
        'mapped_physical_input':'UNVERIFIED','shipping_cook_and_launch':'UNVERIFIED'}
(p/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
if not passed:raise SystemExit(1)
PY
  local summary_rc=$?
  if (( summary_rc )); then rc=1; fi
  if (( rc )); then echo "FAIL: B2b-6f $MODE; logs preserved at $RUN" >&2
  else echo "PASS: B2b-6f $MODE; logs preserved at $RUN"; fi
  exit "$rc"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo '=== Fully staged branch and LFS preflight ==='
run_nix python3 -B "$TOOLS/b2b6a_audit.py" | tee "$RUN/preflight.log"
git lfs fsck >"$RUN/lfs-before.log"
bash -n "$TOOLS/b2b6f_flow_run.sh"
run_nix python3 -B -m unittest discover -s "$TOOLS/tests" -p test_b2b6f_flow.py -v >"$RUN/offline-tests.log" 2>&1 || {
  tail -n 80 "$RUN/offline-tests.log";exit 1;
}
printf '%s\n' preflight >>"$RUN/completed.txt"
run_nix python3 -B "$TOOLS/b2b6a_audit.py" --snapshot "$RUN" before >"$RUN/snapshot-before.log"
PREPARED=1
if [[ "$MODE" == probe ]]; then exit 0; fi

echo '=== Ordinary UE Editor compilation ==='
if ! run_nix bash "$BUILD" AI_Gavin_ProjectEditor Linux Development "-Project=$UPROJECT" -WaitMutex -NoUBTMakefiles >"$RUN/build.log" 2>&1;then
  tail -n 100 "$RUN/build.log";exit 1
fi
grep -q 'Result: Succeeded' "$RUN/build.log" || { tail -n 100 "$RUN/build.log";exit 1; }
printf '%s\n' ordinary-build >>"$RUN/completed.txt"
echo 'PASS: ordinary-build'

run_game() {
  local name=$1 map_kind=$2 scenario=$3 rendered=$4 dir="$RUN/$1" rc=0 hub pedro
  mkdir -p "$dir"
  if [[ "$map_kind" == production ]]; then
    hub=/Game/AI_Gavin/World/Hub/Maps/L_Hub_Prototype
    pedro=/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype
  else
    hub=/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Hub_Pedro_AutonomousTest
    pedro=/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest
  fi
  for package in "$hub" "$pedro"; do
    [[ -f "$PROJECT/Content/${package#/Game/}.umap" ]] || { echo "STOP: missing exact saved map $package" >&2;return 1; }
  done
  local rhi=(-nullrhi)
  [[ "$rendered" == yes ]] && rhi=()
  echo "=== Real game world $name ==="
  run_nix timeout --kill-after=20s 240s steam-run "$UE" "$UPROJECT" "$hub" \
      -game "${rhi[@]}" -unattended -nosound -nop4 -nosplash -log \
      -PedroFlowSmoke "-PedroFlowScenario=$scenario" \
      "-PedroFlowExpectedHub=$hub" "-PedroFlowExpectedPedro=$pedro" \
      '-ExecCmds=t.MaxFPS 120' "-abslog=$dir/game.log" \
      >"$dir/terminal.log" 2>&1 || rc=$?
  printf '%s\n' "$rc" >"$dir/exit-status.txt"
  [[ -s "$dir/game.log" ]] || { tail -n 100 "$dir/terminal.log";return 1; }
  grep -E 'PEDRO_FLOW_TEST|PEDRO_FLOW_DIAG|PEDRO_FLOW_SEARCH|PEDRO_DIRECT|PEDRO_OUTCOME|Engine exit requested' "$dir/game.log" >"$dir/events.log" || true
  if ! run_nix python3 -B "$TOOLS/verify_pedro_flow.py" "$dir/game.log" "$rc" "$scenario" "$hub" "$pedro" "$dir/result.json" \
       >"$dir/verifier.log" 2>&1;then
    echo '=== Exact verifier failure ==='
    tail -n 80 "$dir/verifier.log"
    echo '=== Relevant chronological game events ==='
    tail -n 120 "$dir/events.log"
    echo '=== Last engine log lines ==='
    tail -n 25 "$dir/game.log"
    return 1
  fi
  cat "$dir/verifier.log"
  printf '%s\n' "$name" >>"$RUN/completed.txt"
}

if [[ "$MODE" == all ]]; then
  for map_kind in test production; do
    for scenario in completion early; do
      run_game "$map_kind-$scenario" "$map_kind" "$scenario" no
      run_game "$map_kind-$scenario-rendered" "$map_kind" "$scenario" yes
    done
  done
else
  map_kind=${MODE%%-*}
  suffix=${MODE#*-}
  scenario=${suffix%%-*}
  rendered=no
  [[ "$suffix" == *-rendered ]] && rendered=yes
  run_game "$MODE" "$map_kind" "$scenario" "$rendered"
fi
