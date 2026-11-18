#!/usr/bin/env bash
# Run B2b-5b2 Editor Python using the user's Nix/Steam runtime.
# Explicitly and TEMPORARILY enables Python Editor Script and Editor Scripting
# Utilities in the project descriptor; the original byte-for-byte descriptor
# is restored on success or failure. Never runs alongside another Editor.
set -euo pipefail

MODE="${1:-}"
case "$MODE" in
    inspect|build|verify|resume|probe) ;;
    *) echo "Usage: bash Tools/PedroValidation/b2b5b2_run.sh {inspect|build|verify|resume|probe}" >&2; exit 2 ;;
esac

PROJECT="${PROJECT:-$HOME/Development/Unreal/Projects/AI_Gavin_Project}"
ENGINE="${ENGINE:-$HOME/Development/Unreal/Engines/UE_5.8.2}"
BRANCH='feature/pedro-physical-observer-playable'
BASE='8a6f31356ebd9972279d37cb9fd6777d7df05b60'
DESCRIPTOR="$PROJECT/AI_Gavin_Project.uproject"
SCRIPT="$PROJECT/Tools/PedroValidation/b2b5b2_editor.py"

cd "$PROJECT"

test "$(git branch --show-current)" = "$BRANCH" || {
    echo "STOP: Wrong branch. Expected $BRANCH" >&2; exit 1;
}
git merge-base --is-ancestor "$BASE" HEAD || {
    echo "STOP: B2b-5b2 baseline $BASE not an ancestor of HEAD" >&2; exit 1;
}
test -f "$DESCRIPTOR"
test -f "$SCRIPT"
test -x "$ENGINE/Engine/Binaries/Linux/UnrealEditor" || {
    echo "STOP: UnrealEditor not found at $ENGINE" >&2; exit 1;
}
test "$(git status --porcelain -- "$DESCRIPTOR")" = '' || {
    echo "STOP: .uproject has existing edits. Save/resolve them first." >&2; exit 1;
}
if [[ "$MODE" == build ]]; then
    for asset in \
        L_Pedro_ObserverActorTest.umap \
        L_Hub_Pedro_ObserverActorTest.umap \
        DA_Encounter_Pedro_ObserverActorTest.uasset \
        BP_PedroObserver_Validation.uasset; do
        test ! -e "$PROJECT/Content/AI_Gavin/Encounters/Pedro/Validation/$asset" || {
            echo "STOP: $asset already exists. Build is intentionally one-shot." >&2
            exit 1
        }
    done
fi

# Resume is deliberately limited to the precise partial state observed in
# the first UE 5.8.2 run. Do NOT use this as a general idempotent build mode.
if [[ "$MODE" == resume || "$MODE" == probe ]]; then
    ASSET_DIR="$PROJECT/Content/AI_Gavin/Encounters/Pedro/Validation"
    for asset in L_Pedro_ObserverActorTest.umap L_Hub_Pedro_ObserverActorTest.umap; do
        test -s "$ASSET_DIR/$asset" || {
            echo "STOP: Resume requires saved partial map: $asset" >&2; exit 1;
        }
    done
    for asset in DA_Encounter_Pedro_ObserverActorTest.uasset BP_PedroObserver_Validation.uasset; do
        test ! -e "$ASSET_DIR/$asset" || {
            echo "STOP: Resume refuses existing BP/Data asset: $asset" >&2; exit 1;
        }
    done
    # The previously confirmed failed run has exactly six new/untracked
    # project paths: four tools/document files and the two map copies.
    # Refuse to run if tracked project files changed or unknown untracked
    # content was created since the failed run.
    git diff --quiet HEAD -- || {
        echo "STOP: Tracked files changed; resume preflight must be reviewed." >&2; exit 1;
    }
    EXPECTED_PATHS="$(printf '%s\n' \
        'Content/AI_Gavin/Encounters/Pedro/Validation/L_Hub_Pedro_ObserverActorTest.umap' \
        'Content/AI_Gavin/Encounters/Pedro/Validation/L_Pedro_ObserverActorTest.umap' \
        'Tools/PedroValidation/b2b5b2_audit.py' \
        'Tools/PedroValidation/b2b5b2_editor.py' \
        'Tools/PedroValidation/b2b5b2_run.sh' \
        'docs/encounters/pedro-observer-playable-validation.md' | LC_ALL=C sort)"
    ACTUAL_PATHS="$(git ls-files --others --exclude-standard | LC_ALL=C sort)"
    if [[ "$ACTUAL_PATHS" != "$EXPECTED_PATHS" ]]; then
        echo "STOP: Resume requires EXACTLY the 2 partial maps and 4 kit files." >&2
        echo "Expected:" >&2; printf '%s\n' "$EXPECTED_PATHS" >&2
        echo "Actual:" >&2; printf '%s\n' "$ACTUAL_PATHS" >&2
        exit 1
    fi
    test -z "$(git diff --cached --name-only)" || {
        echo "STOP: Staged changes present; review before resume." >&2; exit 1;
    }
fi

# Do not permit an existing Editor instance to hold/open/save the same maps.
if pgrep -u "$(id -u)" -f '/Engine/Binaries/Linux/UnrealEditor([[:space:]]|$)' >/dev/null; then
    echo "STOP: Close all UnrealEditor instances before running the scripted editor." >&2
    exit 1
fi

RUN="$PROJECT/Saved/PedroValidation/b2b5b2-${MODE}-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$RUN"
BACKUP="$RUN/original.uproject"
cp -p "$DESCRIPTOR" "$BACKUP"

restore_descriptor() {
    local old_status=$?
    trap - EXIT
    if [[ -f "$BACKUP" ]]; then
        cp "$BACKUP" "$DESCRIPTOR"
        if ! cmp -s "$BACKUP" "$DESCRIPTOR"; then
            echo "CRITICAL: Could not restore original .uproject descriptor!" >&2
            old_status=1
        fi
    fi
    exit "$old_status"
}
trap restore_descriptor EXIT

# Updating the descriptor is only a local, temporary execution detail.
# Both plugins are editor-only and the original descriptor is restored exactly.
nix develop '/etc/nixos#default' --command python3 - "$DESCRIPTOR" <<'PY'
import json
import sys
from pathlib import Path
path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding='utf-8'))
plugins = data.setdefault('Plugins', [])
for name in ('PythonScriptPlugin', 'EditorScriptingUtilities'):
    matching = [p for p in plugins if p.get('Name') == name]
    if len(matching) > 1:
        raise SystemExit('STOP: Duplicate plugin descriptor for ' + name)
    if matching:
        matching[0]['Enabled'] = True
    else:
        plugins.append({'Name': name, 'Enabled': True,
                        'TargetAllowList': ['Editor']})
path.write_text(json.dumps(data, indent=4) + '\n', encoding='utf-8')
print('Temporary Editor Python plugins enabled.')
PY

export B2B5B2_MODE="$MODE"
export B2B5B2_RUN_ID="$(basename "$RUN")"
export B2B5B2_RESULT_PATH="$RUN/result.json"
export B2B5B2_PROJECT="$PROJECT"

printf '%s\n' "$RUN" > "$PROJECT/Saved/PedroValidation/b2b5b2-${MODE}-latest.txt"
echo "=== B2b-5b2 Editor automation: $MODE ==="
echo "Engine: $ENGINE"
echo "Project: $PROJECT"
echo "Report: $RUN"

# Full Editor mode is deliberate: new_level_from_template relies on Editor
# world APIs. -ExecutePythonScript automatically exits after script finishes.
set +e
nix develop '/etc/nixos#default' --command steam-run \
    "$ENGINE/Engine/Binaries/Linux/UnrealEditor" \
    "$DESCRIPTOR" -nop4 \
    "-ExecutePythonScript=$SCRIPT" \
    "-abslog=$RUN/editor.log" \
    > "$RUN/terminal.log" 2>&1
ENGINE_STATUS=$?
set -e

# Validate the dedicated JSON result even when Unreal returns exit code 0.
set +e
nix develop '/etc/nixos#default' --command python3 - \
    "$B2B5B2_RESULT_PATH" "$MODE" "$B2B5B2_RUN_ID" <<'PY'
import json
import sys
from pathlib import Path
p = Path(sys.argv[1])
if not p.is_file():
    raise SystemExit('FAIL: Editor did not write a report (Python plugin/startup issue?): ' + str(p))
r = json.loads(p.read_text(encoding='utf-8'))
print('B2b-5b2 Editor result: ' + r.get('status', '<missing>'))
if r.get('status') != 'PASS' or r.get('mode') != sys.argv[2] or r.get('run_id') != sys.argv[3]:
    print(r.get('error', '<no error provided>'))
    raise SystemExit(1)
print('PASS: Result belongs to this run.')
PY
REPORT_STATUS=$?
set -e

if (( ENGINE_STATUS != 0 || REPORT_STATUS != 0 )); then
    echo "FAIL: UE Editor status=$ENGINE_STATUS; report status=$REPORT_STATUS" >&2
    echo "Logs: $RUN/editor.log  $RUN/terminal.log  $RUN/result.json" >&2
    echo "If build/resume failed, do NOT rerun blindly: new assets may already exist." >&2
    exit 1
fi

echo "PASS: Unreal Editor $MODE completed."
echo "Saved result: $RUN/result.json"
echo "The original .uproject will be restored now."
