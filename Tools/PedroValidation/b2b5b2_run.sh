#!/usr/bin/env bash
# Read-only saved-map verification. Original authoring workflow is in Git history.
set -euo pipefail
MODE=${1:-verify}
[[ $# -le 1 && "$MODE" == verify ]] || { echo "Usage: $0 [verify]" >&2; exit 2; }
PROJECT=${PROJECT:-$HOME/Development/Unreal/Projects/AI_Gavin_Project}
ENGINE=${ENGINE:-$HOME/Development/Unreal/Engines/UE_5.8.2}
cd "$PROJECT"
PROJECT=$PWD
ENGINE=$(realpath "$ENGINE")
NIX=(nix develop '/etc/nixos#default' --command)
DESCRIPTOR="$PROJECT/AI_Gavin_Project.uproject"
SCRIPT="$PROJECT/Tools/PedroValidation/b2b5b2_editor.py"
AUDIT=Tools/PedroValidation/b2b6a_audit.py
export PYTHONDONTWRITEBYTECODE=1
[[ -f "$DESCRIPTOR" && -f "$SCRIPT" && -x "$ENGINE/Engine/Binaries/Linux/UnrealEditor" ]]
mkdir -p Saved/PedroValidation
exec 9>Saved/PedroValidation/b2b6a.lock
flock -n 9 || { echo 'STOP: Pedro validation runner already active.' >&2; exit 1; }
if pgrep -u "$(id -u)" -f '/Engine/Binaries/Linux/UnrealEditor(-Cmd)?([[:space:]]|$)' >/dev/null; then
    echo 'STOP: Close all Unreal Editor/game instances.' >&2; exit 1
fi
"${NIX[@]}" python3 -B "$AUDIT"
git diff --quiet HEAD -- "$DESCRIPTOR" || { echo 'STOP: Descriptor has existing edits.' >&2; exit 1; }
git lfs fsck
RUN=$(mktemp -d "$PROJECT/Saved/PedroValidation/b2b5b2-verify-XXXXXXXX")
echo "Reports: $RUN"
printf '%s\n' "$RUN" > Saved/PedroValidation/b2b5b2-verify-latest.txt
"${NIX[@]}" python3 -B "$AUDIT" --snapshot "$RUN" before
BACKUP="$RUN/original.uproject"
cp -p "$DESCRIPTOR" "$BACKUP"
finalize() {
    local rc=$?
    trap - EXIT
    set +e
    cp -p "$BACKUP" "$DESCRIPTOR" || rc=1
    cmp -s "$BACKUP" "$DESCRIPTOR" || { echo 'FAIL: Descriptor restoration' >&2; rc=1; }
    "${NIX[@]}" python3 -B "$AUDIT" --snapshot "$RUN" after || rc=1
    cmp -s "$RUN/status-before.txt" "$RUN/status-after.txt" || { echo 'FAIL: Git status changed' >&2; rc=1; }
    cmp -s "$RUN/files-before.sha256" "$RUN/files-after.sha256" || { echo 'FAIL: Repository bytes changed; inspect reports' >&2; rc=1; }
    "${NIX[@]}" python3 -B "$AUDIT" || rc=1
    git lfs fsck || rc=1
    if (( rc == 0 )); then echo 'PASS: Saved-map verification and repository integrity'; else echo 'FAIL: Saved-map verification; inspect reports' >&2; fi
    echo "Reports: $RUN"
    exit "$rc"
}
trap finalize EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"${NIX[@]}" python3 -B - "$DESCRIPTOR" <<'PY'
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
        plugins.append({'Name': name, 'Enabled': True, 'TargetAllowList': ['Editor']})
path.write_text(json.dumps(data, indent=4) + '\n', encoding='utf-8')
PY
export B2B5B2_MODE=verify
export B2B5B2_RUN_ID="$(basename "$RUN")"
export B2B5B2_RESULT_PATH="$RUN/result.json"
export B2B5B2_PROJECT="$PROJECT"
rc=0
"${NIX[@]}" timeout --kill-after=15s 420s steam-run \
    "$ENGINE/Engine/Binaries/Linux/UnrealEditor" "$DESCRIPTOR" -nop4 \
    "-ExecutePythonScript=$SCRIPT" "-abslog=$RUN/editor.log" \
    > "$RUN/terminal.log" 2>&1 || rc=$?
if (( rc != 0 )); then tail -n 60 "$RUN/terminal.log"; exit 1; fi
if grep -Ein 'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault' "$RUN/editor.log"; then exit 1; fi
[[ -s "$RUN/editor.log" ]]
"${NIX[@]}" python3 -B - "$B2B5B2_RESULT_PATH" "$B2B5B2_RUN_ID" <<'PY'
import json
import sys
from pathlib import Path
r = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
if (r.get('status'), r.get('mode'), r.get('run_id')) != ('PASS', 'verify', sys.argv[2]):
    raise SystemExit('FAIL: ' + str(r))
print('PASS: Saved configuration verified in this Editor process')
PY
