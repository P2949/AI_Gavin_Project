#!/usr/bin/env bash
# B2b-6a: committed mechanical validation. No temporary native TU or asset writes.
set -euo pipefail
PROJECT=${PROJECT:-$HOME/Development/Unreal/Projects/AI_Gavin_Project}
ENGINE=${ENGINE:-$HOME/Development/Unreal/Engines/UE_5.8.2}
MODE=${1:-all}
case "$MODE" in all|unit|world|integration|rendered|unity|probe) ;; *) echo "Usage: $0 [all|unit|world|integration|rendered|unity|probe]" >&2; exit 2;; esac
[[ $# -le 1 ]] || exit 2
cd "$PROJECT"
PROJECT=$PWD
ENGINE=$(realpath "$ENGINE")
NIX=(nix develop '/etc/nixos#default' --command)
UE="$ENGINE/Engine/Binaries/Linux/UnrealEditor"
BUILD="$ENGINE/Engine/Build/BatchFiles/Linux/Build.sh"
DESCRIPTOR="$PROJECT/AI_Gavin_Project.uproject"
MAP='/Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest'
TOOLS='Tools/PedroValidation'
AUDIT="$TOOLS/b2b6a_audit.py"
VERIFY="$TOOLS/verify_awareness_report.py"
export PYTHONDONTWRITEBYTECODE=1
[[ -x "$UE" && -f "$BUILD" && -f "$DESCRIPTOR" ]] || { echo 'STOP: Engine or descriptor missing.' >&2; exit 1; }
mkdir -p Saved/PedroValidation
# Shared advisory lock for all invocations of this kit in this checkout.
exec 9>Saved/PedroValidation/b2b6a.lock
flock -n 9 || { echo 'STOP: Another B2b-6a runner is active.' >&2; exit 1; }
if pgrep -u "$(id -u)" -f '/Engine/Binaries/Linux/UnrealEditor(-Cmd)?([[:space:]]|$)' >/dev/null; then
    echo 'STOP: Close all Unreal Editor/game instances.' >&2; exit 1
fi
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b6a-$MODE-XXXXXXXX")
printf '%s\n' "$RUN" > Saved/PedroValidation/b2b6a-latest.txt
printf '%s\n' "$RUN" > "Saved/PedroValidation/b2b6a-$MODE-latest.txt"
: > "$RUN/completed.txt"
echo "Reports: $RUN"
complete() { printf '%s\n' "$1" >> "$RUN/completed.txt"; echo "PASS: $1"; }

# A failed preflight is not a successful run. Always leave diagnostics.
PREPARED=0
finalize() {
    local rc=$?
    trap - EXIT
    set +e
    if (( PREPARED )); then
        "${NIX[@]}" python3 -B "$AUDIT" --snapshot "$RUN" after > "$RUN/snapshot-after.log" 2>&1 || rc=1
        cmp -s "$RUN/status-before.txt" "$RUN/status-after.txt" || { echo 'FAIL: Git status changed.' >&2; rc=1; }
        if ! cmp -s "$RUN/files-before.sha256" "$RUN/files-after.sha256"; then
            echo 'FAIL: Working-tree file hashes changed. No rollback attempted.' >&2
            diff -u "$RUN/files-before.sha256" "$RUN/files-after.sha256" > "$RUN/file-differences.txt" || true
            rc=1
        fi
        "${NIX[@]}" python3 -B "$AUDIT" > "$RUN/audit-after.log" 2>&1 || rc=1
        git lfs fsck > "$RUN/lfs-fsck.log" 2>&1 || rc=1
        if (( rc == 0 )); then complete repository-audit; fi
    fi
    "${NIX[@]}" python3 -B - "$RUN" "$MODE" "$rc" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);mode=sys.argv[2];rc=int(sys.argv[3]);stages=(p/'completed.txt').read_text().splitlines()
common=['preflight','engine-inspection','offline-tests','snapshot-before']
expected={'probe':common,'unit':common+['ordinary-build','native'],
          'world':common+['ordinary-build','world'],
          'integration':common+['ordinary-build','integration'],
          'rendered':common+['ordinary-build','rendered-world','rendered-integration'],
          'unity':common+['forced-unity-build','native-unity'],
          'all':common+['ordinary-build','native','world','integration','rendered-world','rendered-integration','forced-unity-build','native-unity']}[mode]+['repository-audit']
passed=(rc==0 and stages==expected)
reports={}
for q in sorted(p.glob('*/results.json')):
    reports[q.parent.name]=str(q.relative_to(p))
summary={'status':'PASS' if passed else 'FAIL','mode':mode,'exit_status':0 if passed else max(rc,1),
         'completed_stages':stages,'expected_stages':expected,'verified_reports':reports,
         'full_acceptance':passed and mode=='all'}
(p/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
if not passed: raise SystemExit(1)
PY
    local summary_rc=$?
    if (( summary_rc != 0 )); then rc=1; fi
    if (( rc == 0 )); then
        printf 'PASS: B2b-6a %s validation.\n' "$MODE" | tee "$RUN/verification.txt"
    else
        printf 'FAIL: B2b-6a %s validation; inspect the retained logs.\n' "$MODE" | tee "$RUN/verification.txt"
    fi
    echo "Reports: $RUN"
    exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo '=== Repository preflight ==='
"${NIX[@]}" python3 -B "$AUDIT" | tee "$RUN/preflight.log"
git lfs fsck | tee "$RUN/lfs-before.log"
complete preflight

echo '=== Inspect local UE API and source (read-only) ==='
"${NIX[@]}" python3 -B "$AUDIT" --engine-only "$ENGINE" > "$RUN/engine-inspection.log" 2>&1 || { tail -n 50 "$RUN/engine-inspection.log"; exit 1; }
complete engine-inspection

echo '=== Offline source and report verifier tests ==='
bash -n "$TOOLS/b2b6a_run.sh"
"${NIX[@]}" python3 -B - "$TOOLS" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1])
for name in ['b2b6a_audit.py','verify_awareness_report.py','tests/test_awareness_report.py']:
    p=root/name;compile(p.read_bytes(),str(p),'exec')
print('PASS: Python source syntax')
PY
"${NIX[@]}" python3 -B -m unittest discover -s "$TOOLS/tests" -p test_awareness_report.py -v 2>&1 | tee "$RUN/offline-tests.log"
complete offline-tests
"${NIX[@]}" python3 -B "$AUDIT" --snapshot "$RUN" before > "$RUN/snapshot-before.log"
PREPARED=1
complete snapshot-before
if [[ "$MODE" == probe ]]; then exit 0; fi

build() {
    local label=$1; shift
    local out="$RUN/$label.log"
    echo "=== Build $label ==="
    if ! "${NIX[@]}" bash "$BUILD" AI_Gavin_ProjectEditor Linux Development "-Project=$DESCRIPTOR" -WaitMutex "$@" > "$out" 2>&1; then
        tail -n 100 "$out"; return 1
    fi
    cat "$out"
    grep -q 'Result: Succeeded' "$out" || { echo 'FAIL: missing UBT success marker' >&2; return 1; }
    if [[ "$label" == forced-unity-build ]]; then
        mkdir -p "$RUN/unity"
        "${NIX[@]}" python3 -B "$VERIFY" unity "$out" 0 "$RUN/unity/results.json" --project "$PROJECT" | tee "$RUN/unity/verification.txt"
    fi
    complete "$label"
}
check_fatal() {
    local file=$1
    if grep -Ein 'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault' "$file"; then
        echo "FAIL: fatal/assert/ensure in $file" >&2; return 1
    fi
}
native() {
    local label=$1 dir="$RUN/$1" rc=0
    mkdir -p "$dir"
    echo "=== Native Pedro tests ($label) ==="
    "${NIX[@]}" timeout --kill-after=15s 420s steam-run "$UE" "$DESCRIPTOR" \
        -nullrhi -unattended -nosound -nop4 -nosplash -log "-abslog=$dir/editor.log" \
        '-ExecCmds=Automation RunTest AI_Gavin_Project.Pedro;Quit' \
        "-ReportExportPath=$dir" \
        > "$dir/terminal.log" 2>&1 || rc=$?
    printf '%s\n' "$rc" > "$dir/exit-status.txt"
    [[ -s "$dir/editor.log" ]] || { tail -n 60 "$dir/terminal.log"; return 1; }
    check_fatal "$dir/editor.log"
    if ! "${NIX[@]}" python3 -B "$VERIFY" native "$dir/index.json" "$rc" "$dir/results.json" 2>&1 | tee "$dir/verification.txt"; then
        echo "=== Native process diagnostics (exit=$rc) ==="
        grep -Ein -m 80 'Result=\{Fail|: Error:|Fatal error:|Assertion failed:|Ensure condition failed:|RequestExit|Automation Test Queue Empty' "$dir/editor.log" || true
        tail -n 60 "$dir/editor.log"; return 1
    fi
    complete "$label"
}
runtime() {
    local suite=$1 label=$2 render=$3 dir="$RUN/$2" rc=0 flag
    local rhi=(-nullrhi)
    [[ "$render" == yes ]] && rhi=()
    if [[ "$suite" == world ]]; then flag=-PedroAwarenessWorld; else flag=-PedroAwarenessIntegration; fi
    mkdir -p "$dir"
    echo "=== Runtime suite $label ==="
    "${NIX[@]}" timeout --kill-after=15s 420s steam-run "$UE" "$DESCRIPTOR" "$MAP" \
        -game "${rhi[@]}" -unattended -nosound -nop4 -nosplash -log "$flag" \
        "-abslog=$dir/game.log" > "$dir/terminal.log" 2>&1 || rc=$?
    printf '%s\n' "$rc" > "$dir/exit-status.txt"
    if [[ ! -s "$dir/game.log" ]]; then tail -n 80 "$dir/terminal.log"; return 1; fi
    grep -E 'PEDRO_AWARENESS_TEST|PEDRO_AWARENESS_FIXTURE|PEDRO_DIRECT|PEDRO_SHADOW|PEDRO_OUTCOME|Engine exit requested' "$dir/game.log" > "$dir/events.txt" || true
    cat "$dir/events.txt"
    check_fatal "$dir/game.log"
    if ! "${NIX[@]}" python3 -B "$VERIFY" "$suite" "$dir/game.log" "$rc" "$dir/results.json" 2>&1 | tee "$dir/verification.txt"; then
        tail -n 80 "$dir/game.log"; return 1
    fi
    complete "$label"
}

forced_unity() {
    # Invalidate dependencies without altering source bytes or build configuration.
    touch -- Source/AI_Gavin_Project/Encounters/Pedro/PedroObserverAIController.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessStateTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroObserverAIControllerTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessWorldTests.cpp
    build forced-unity-build -ForceUnity -DisableAdaptiveUnity -gather
}
if [[ "$MODE" == unity ]]; then
    forced_unity
    native native-unity
    exit 0
fi
build ordinary-build
case "$MODE" in
    unit) native native ;;
    world) runtime world world no ;;
    integration) runtime integration integration no ;;
    rendered) runtime world rendered-world yes; runtime integration rendered-integration yes ;;
    all)
        native native
        runtime world world no
        runtime integration integration no
        runtime world rendered-world yes
        runtime integration rendered-integration yes
        forced_unity
        native native-unity
        ;;
esac
