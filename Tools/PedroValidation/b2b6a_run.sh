#!/usr/bin/env bash
# Pedro awareness, attention and navigation: committed mechanical validation. No temporary native TU or asset writes.
set -euo pipefail
MODE=${1:-all}
case "$MODE" in all|unit|world|integration|attention|attention-integration|navigation-probe|navigation|navigation-integration|context-probe|context|context-integration|playable|playable-rendered|playable-test|playable-test-rendered|rendered|unity|probe) ;; *) echo "Usage: $0 [all|unit|world|integration|attention|attention-integration|navigation-probe|navigation|navigation-integration|context-probe|context|context-integration|rendered|unity|probe]" >&2; exit 2;; esac
[[ $# -le 1 ]] || exit 2
source "$(dirname -- "${BASH_SOURCE[0]}")/runner_common.sh"
pedro_init build
MAP='/Game/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest'
PLAYABLE_PRODUCTION='/Game/AI_Gavin/Encounters/Pedro/Maps/L_Pedro_Prototype'
PLAYABLE_TEST='/Game/AI_Gavin/Encounters/Pedro/Validation/B2b6e/L_Pedro_AutonomousTest'
case "$MODE" in
    playable-test|playable-test-rendered) PLAYABLE_MAP="$PLAYABLE_TEST" ;;
    *) PLAYABLE_MAP="$PLAYABLE_PRODUCTION" ;;
esac
if [[ -n "${PEDRO_PLAYABLE_MAP:-}" && "$PEDRO_PLAYABLE_MAP" != "$PLAYABLE_MAP" ]]; then
    echo "STOP: $MODE requires exact map $PLAYABLE_MAP (not $PEDRO_PLAYABLE_MAP)" >&2; exit 2
fi
case "$MODE" in
    playable|playable-rendered|playable-test|playable-test-rendered|all|rendered)
        playable_disk="Content/${PLAYABLE_MAP#/Game/}.umap"
        [[ -f "$playable_disk" ]] || { echo "STOP: missing saved playable map: $playable_disk" >&2; exit 1; }
        ;;
esac
TOOLS='Tools/PedroValidation'
AUDIT="$TOOLS/b2b6a_audit.py"
VERIFY="$TOOLS/verify_awareness_report.py"
pedro_lock
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
        run_nix python3 -B "$AUDIT" --snapshot "$RUN" after > "$RUN/snapshot-after.log" 2>&1 || rc=1
        verify_readonly_snapshot "$RUN" || rc=1
        run_nix python3 -B "$AUDIT" > "$RUN/audit-after.log" 2>&1 || rc=1
        git lfs fsck > "$RUN/lfs-fsck.log" 2>&1 || rc=1
        if (( rc == 0 )); then complete repository-audit; fi
    fi
    run_nix python3 -B - "$RUN" "$MODE" "$rc" <<'PY'
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);mode=sys.argv[2];rc=int(sys.argv[3]);stages=(p/'completed.txt').read_text().splitlines()
sys.path.insert(0, str(Path.cwd()/'Tools/PedroValidation'))
from b2b6e_contract import expected_stages, check_full_acceptance
expected=expected_stages(mode)
reports={}
for q in sorted(p.glob('*/results.json')):
    reports[q.parent.name]=str(q.relative_to(p))
passed=check_full_acceptance(mode,rc,stages,set(reports))
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
        printf 'PASS: Pedro %s validation.\n' "$MODE" | tee "$RUN/verification.txt"
    else
        printf 'FAIL: Pedro %s validation; inspect the retained logs.\n' "$MODE" | tee "$RUN/verification.txt"
    fi
    echo "Reports: $RUN"
    exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo '=== Repository preflight ==='
run_nix python3 -B "$AUDIT" | tee "$RUN/preflight.log"
git lfs fsck | tee "$RUN/lfs-before.log"
complete preflight

echo '=== Inspect local UE API and source (read-only) ==='
run_nix python3 -B "$AUDIT" --engine-only "$ENGINE" > "$RUN/engine-inspection.log" 2>&1 || { tail -n 50 "$RUN/engine-inspection.log"; exit 1; }
complete engine-inspection

echo '=== Offline source and report verifier tests ==='
bash -n "$TOOLS/b2b6a_run.sh"
run_nix python3 -B - "$TOOLS" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1])
for name in ['b2b6a_audit.py','verify_awareness_report.py','tests/test_awareness_report.py',
             'verify_playable_smoke.py','tests/test_playable_smoke.py',
             'tests/test_playable_protocol.py','b2b6e_saved_map_audit.py',
             'b2b6e_contract.py','b2b6e_editor_common.py','b2b6e_author_level.py',
             'b2b6e_write_scope.py','tests/test_b2b6e_mechanical.py']:
    p=root/name;compile(p.read_bytes(),str(p),'exec')
print('PASS: Python source syntax')
PY
run_nix python3 -B -m unittest discover -s "$TOOLS/tests" -p "test*.py" -v 2>&1 | tee "$RUN/offline-tests.log"
complete offline-tests
run_nix python3 -B "$AUDIT" --snapshot "$RUN" before > "$RUN/snapshot-before.log"
PREPARED=1
complete snapshot-before
if [[ "$MODE" == probe ]]; then exit 0; fi

build() {
    local label=$1; shift
    local out="$RUN/$label.log"
    echo "=== Build $label ==="
    if ! run_nix bash "$BUILD" AI_Gavin_ProjectEditor Linux Development "-Project=$DESCRIPTOR" -WaitMutex "$@" > "$out" 2>&1; then
        tail -n 100 "$out"; return 1
    fi
    cat "$out"
    grep -q 'Result: Succeeded' "$out" || { echo 'FAIL: missing UBT success marker' >&2; return 1; }
    if [[ "$label" == non-unity-build ]]; then
        mkdir -p "$RUN/non-unity"
        run_nix python3 -B "$VERIFY" non-unity "$out" 0 "$RUN/non-unity/results.json" | tee "$RUN/non-unity/verification.txt"
    elif [[ "$label" == forced-unity-build ]]; then
        mkdir -p "$RUN/unity"
        run_nix python3 -B "$VERIFY" unity "$out" 0 "$RUN/unity/results.json" --project "$PROJECT" | tee "$RUN/unity/verification.txt"
    fi
    complete "$label"
}
native() {
    local label=$1 dir="$RUN/$1" rc=0
    mkdir -p "$dir"
    echo "=== Native Pedro tests ($label) ==="
    run_nix timeout --kill-after=15s 420s steam-run "$UE" "$DESCRIPTOR" \
        -nullrhi -unattended -nosound -nop4 -nosplash -log "-abslog=$dir/editor.log" \
        '-ExecCmds=Automation RunTest AI_Gavin_Project.Pedro;Quit' \
        "-ReportExportPath=$dir" \
        > "$dir/terminal.log" 2>&1 || rc=$?
    printf '%s\n' "$rc" > "$dir/exit-status.txt"
    [[ -s "$dir/editor.log" ]] || { tail -n 60 "$dir/terminal.log"; return 1; }
    check_fatal "$dir/editor.log"
    if ! run_nix python3 -B "$VERIFY" native "$dir/index.json" "$rc" "$dir/results.json" 2>&1 | tee "$dir/verification.txt"; then
        echo "=== Native process diagnostics (exit=$rc) ==="
        grep -Ein -m 80 'Result=\{Fail|: Error:|Fatal error:|Assertion failed:|Ensure condition failed:|RequestExit|Automation Test Queue Empty' "$dir/editor.log" || true
        tail -n 60 "$dir/editor.log"; return 1
    fi
    complete "$label"
}
runtime() {
    local suite=$1 label=$2 render=$3 dir="$RUN/$2" rc=0 flag
    local rhi=(-nullrhi) map="$MAP"
    [[ "$render" == yes ]] && rhi=()
    local pacing=() navconfig=() playable_arg=()
    case "$suite" in
        world) flag=-PedroAwarenessWorld ;;
        integration) flag=-PedroAwarenessIntegration ;;
        attention) flag=-PedroAttentionWorld; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        attention-integration) flag=-PedroAttentionIntegration; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        navigation-probe) flag=-PedroNavigationProbe; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        navigation) flag=-PedroNavigationWorld; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        navigation-integration) flag=-PedroNavigationIntegration; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        context-probe) flag=-PedroContextProbe; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        playable|playable-test) flag=-PedroPlayableSmoke; map="$PLAYABLE_MAP"; pacing=('-ExecCmds=t.MaxFPS 120'); playable_arg=("-PedroPlayableExpectedMap=$PLAYABLE_MAP") ;;
        context) flag=-PedroContextWorld; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        context-integration) flag=-PedroContextIntegration; pacing=('-ExecCmds=t.MaxFPS 120') ;;
        *) return 1 ;;
    esac
    if [[ "$suite" == navigation* || "$suite" == context* ]]; then
        # Process-only override; no project or engine ini changes. The runtime
        # fixture independently demands Dynamic Recast and real path queries.
        navconfig=('-ini:Engine:[/Script/NavigationSystem.RecastNavMesh]:RuntimeGeneration=Dynamic')
    fi
    mkdir -p "$dir"
    echo "=== Runtime suite $label ==="
    run_nix timeout --kill-after=15s 420s steam-run "$UE" "$DESCRIPTOR" "$map" \
        -game "${rhi[@]}" -unattended -nosound -nop4 -nosplash -log "$flag" "${pacing[@]}" "${navconfig[@]}" "${playable_arg[@]}" \
        "-abslog=$dir/game.log" > "$dir/terminal.log" 2>&1 || rc=$?
    printf '%s\n' "$rc" > "$dir/exit-status.txt"
    if [[ ! -s "$dir/game.log" ]]; then tail -n 80 "$dir/terminal.log"; return 1; fi
    grep -E 'PEDRO_CONTEXT_|PEDRO_NAV_TEST|PEDRO_NAV_FIXTURE|PEDRO_ATTENTION_TEST|PEDRO_AWARENESS_TEST|PEDRO_AWARENESS_FIXTURE|PEDRO_PLAYABLE_TEST|PEDRO_PLAYABLE_CONFIGURATION|PEDRO_DIRECT|PEDRO_SHADOW|PEDRO_OUTCOME|Engine exit requested' "$dir/game.log" > "$dir/events.txt" || true
    grep -Ev 'PEDRO_(ATTENTION|NAV|CONTEXT)_TEST status=FRAME ' "$dir/events.txt" || true
    check_fatal "$dir/game.log"
    local verifier=("$VERIFY" "$suite")
    local playable_verify_args=()
    if [[ "$suite" == playable || "$suite" == playable-test ]]; then
        verifier=("$TOOLS/verify_playable_smoke.py")
        playable_verify_args=("$map")
    fi
    if ! run_nix python3 -B "${verifier[@]}" "$dir/game.log" "$rc" "$dir/results.json" "${playable_verify_args[@]}" 2>&1 | tee "$dir/verification.txt"; then
        tail -n 80 "$dir/game.log"; return 1
    fi
    complete "$label"
}

touch_sources() {
    # Invalidate dependencies without altering source bytes or build configuration.
    touch -- Source/AI_Gavin_Project/Encounters/Pedro/PedroObserverAIController.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessStateTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroObserverAIControllerTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAwarenessWorldTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroAttentionPolicy.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroAttentionTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroNavigationPolicy.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroNavigationTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroContextPolicy.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroObserverWatchPoint.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroEncounterDirector.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroContextTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/PedroPlayableIntegrationActor.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableIntegrationTests.cpp \
        Source/AI_Gavin_Project/Encounters/Pedro/Tests/PedroPlayableSmoke.cpp
}
# Do not let an existing UBT action graph survive an explicit build-mode switch.
# Keep generated project build state recoverable, outside the report archive.
# Never clean engine caches, project binaries, assets, or tracked files.
prepare_explicit_build() {
    local label=$1 relative=Intermediate/Build cache="$PROJECT/Intermediate/Build" backup
    if [[ -L "$PROJECT/Intermediate" || -L "$cache" ]]; then
        echo 'STOP: unexpected symlink in generated-build/cache path.' >&2; return 1
    fi
    if [[ -n "$(git ls-files --cached -- "$relative")" ]] || ! git check-ignore -q -- "$relative/"; then
        echo 'STOP: Intermediate/Build is not exclusively ignored generated state.' >&2; return 1
    fi
    if [[ -e "$cache" ]]; then
        [[ -d "$cache" ]] || { echo 'STOP: generated build path is not a directory.' >&2; return 1; }
        # Use the same parent as Build, not Saved (which may be on another filesystem).
        # A real rename is the authority, not st_dev: subvolumes may report different
        # device IDs. os.rename never falls back to recursively copying the cache.
        backup=$(mktemp -d "$PROJECT/Intermediate/.pedro-build-cache-$label-XXXXXXXX")
        if ! git check-ignore -q -- "${backup#"$PROJECT/"}/"; then
            rmdir -- "$backup"
            echo 'STOP: generated backup directory is not ignored by Git.' >&2; return 1
        fi
        if ! run_nix python3 -B - "$cache" "$backup/Build" <<'PYRENAME'
import os, sys
source, destination = sys.argv[1:]
if os.path.lexists(destination):
    raise SystemExit('STOP: generated backup destination already exists')
try:
    os.rename(source, destination)
except OSError as exc:
    print(f'STOP: atomic cache rename failed: {exc}', file=sys.stderr)
    raise SystemExit(1)
PYRENAME
        then
            # Never delete source data, even if the rename fails (e.g. Build is a mountpoint).
            rmdir -- "$backup" 2>/dev/null || true
            printf 'Cache path: %s\n' "$cache" >&2
            stat -c 'device=%d type=%F path=%n' -- "$cache" "$PROJECT/Intermediate" >&2 || true
            if command -v findmnt >/dev/null; then
                findmnt -T "$cache" >&2 || true
                findmnt -T "$PROJECT/Intermediate" >&2 || true
            fi
            return 1
        fi
        printf 'Preserved generated build cache: %s\n' "$backup/Build" | tee -a "$RUN/build-cache.log"
    fi
    [[ ! -e "$cache" && ! -L "$cache" ]] || { echo 'STOP: project build cache still exists.' >&2; return 1; }
    touch_sources
}
# Older UBT exposes an additional makefile-cache switch. Use it only when
# declared by this engine, not based on an assumed engine version.
CACHE_FLAGS=()
if grep -RIn --include='*.cs' -E '\[CommandLine\([[:space:]]*"-NoUBTMakefiles"([[:space:]]*[,)]|[[:space:]]*\])' "$ENGINE/Engine/Source/Programs/UnrealBuildTool" > "$RUN/no-ubt-makefiles-source.log"; then
    CACHE_FLAGS=(-NoUBTMakefiles)
    echo 'Local UBT declares -NoUBTMakefiles; explicit builds will disable makefile reuse.'
else
    echo 'No local -NoUBTMakefiles declaration found; use fresh project build tree.'
fi
non_unity() {
    prepare_explicit_build non-unity-build
    build non-unity-build -DisableUnity -DisableAdaptiveUnity -gather "${CACHE_FLAGS[@]}"
}
forced_unity() {
    prepare_explicit_build forced-unity-build
    build forced-unity-build -ForceUnity -DisableAdaptiveUnity -gather "${CACHE_FLAGS[@]}"
}
ordinary_build() {
    # A recovery archive may preserve an older source mtime. Ensure the
    # twelve relevant translation units are not skipped on an incremental build.
    touch_sources
    build ordinary-build "${CACHE_FLAGS[@]}"
}
if [[ "$MODE" == unity ]]; then
    forced_unity
    native native-unity
    exit 0
fi
if [[ "$MODE" == all || "$MODE" == unit || "$MODE" == navigation-probe || "$MODE" == context-probe ]]; then non_unity; else ordinary_build; fi
case "$MODE" in
    unit) native native ;;
    world) runtime world world no ;;
    integration) runtime integration integration no ;;
    attention) runtime attention attention no ;;
    attention-integration) runtime attention-integration attention-integration no ;;
    navigation-probe) runtime navigation-probe navigation-probe no ;;
    navigation) runtime navigation navigation no ;;
    navigation-integration) runtime navigation-integration navigation-integration no ;;
    context-probe) runtime context-probe context-probe no ;;
    context) runtime context context no ;;
    context-integration) runtime context-integration context-integration no ;;
    playable) runtime playable playable no ;;
    playable-rendered) runtime playable rendered-playable yes ;;
    playable-test) runtime playable-test playable-test no ;;
    playable-test-rendered) runtime playable-test rendered-playable-test yes ;;
    rendered) runtime world rendered-world yes; runtime integration rendered-integration yes; runtime attention rendered-attention yes; runtime attention-integration rendered-attention-integration yes; runtime navigation rendered-navigation yes; runtime navigation-integration rendered-navigation-integration yes; runtime context rendered-context yes; runtime context-integration rendered-context-integration yes; runtime playable rendered-playable yes ;;
    all)
        native native
        runtime world world no
        runtime integration integration no
        runtime attention attention no
        runtime attention-integration attention-integration no
        runtime navigation navigation no
        runtime navigation-integration navigation-integration no
        runtime context context no
        runtime context-integration context-integration no
        runtime playable playable no
        runtime world rendered-world yes
        runtime integration rendered-integration yes
        runtime attention rendered-attention yes
        runtime attention-integration rendered-attention-integration yes
        runtime navigation rendered-navigation yes
        runtime navigation-integration rendered-navigation-integration yes
        runtime context rendered-context yes
        runtime context-integration rendered-context-integration yes
        runtime playable rendered-playable yes
        forced_unity
        native native-unity
        ;;
esac
