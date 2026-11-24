#!/usr/bin/env bash
# Shared Linux validation setup. Source from a runner, then initialize and lock.
# Keep the parent lock held; child Nix/UE processes must not inherit fd 9.
pedro_init() {
    PROJECT=$(realpath "${PROJECT:-$HOME/Development/Unreal/Projects/AI_Gavin_Project}") || return 1
    ENGINE=$(realpath "${ENGINE:-$HOME/Development/Unreal/Engines/UE_5.8.2}") || return 1
    cd "$PROJECT" || return 1
    UE="$ENGINE/Engine/Binaries/Linux/UnrealEditor"
    BUILD="$ENGINE/Engine/Build/BatchFiles/Linux/Build.sh"
    DESCRIPTOR="$PROJECT/AI_Gavin_Project.uproject"
    [[ -x "$UE" && -f "$DESCRIPTOR" ]] || { echo 'STOP: missing UnrealEditor or project descriptor' >&2; return 1; }
    if [[ "${1:-}" == build && ! -f "$BUILD" ]]; then
        echo 'STOP: missing Unreal build script' >&2; return 1
    fi
    NIX=(nix develop '/etc/nixos#default' --command)
    export PYTHONDONTWRITEBYTECODE=1
}

run_nix() { "${NIX[@]}" "$@" 9>&-; }

pedro_lock() {
    mkdir -p Saved/PedroValidation || return 1
    exec 9>Saved/PedroValidation/b2b6a.lock || return 1
    flock -n 9 || { echo 'STOP: another Pedro validation runner holds the lock' >&2; return 1; }
    local rc=0
    pgrep -u "$(id -u)" -f '/Engine/Binaries/Linux/UnrealEditor(-Cmd)?([[:space:]]|$)' >/dev/null || rc=$?
    if (( rc != 1 )); then
        echo 'STOP: Unreal Editor/game is running or process inspection failed' >&2; return 1
    fi
}

# Caller saves the original descriptor and installs its restoration trap first.
enable_editor_plugins() {
    run_nix python3 -B - "$DESCRIPTOR" <<'PYTHON'
import json, sys
from pathlib import Path
path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding='utf-8'))
plugins = data.setdefault('Plugins', [])
for name in ('PythonScriptPlugin', 'EditorScriptingUtilities'):
    matches = [plugin for plugin in plugins if plugin.get('Name') == name]
    if len(matches) > 1:
        raise SystemExit('STOP: duplicate plugin descriptor: ' + name)
    if matches:
        matches[0]['Enabled'] = True
    else:
        plugins.append({'Name': name, 'Enabled': True, 'TargetAllowList': ['Editor']})
path.write_text(json.dumps(data, indent=4) + '\n', encoding='utf-8')
PYTHON
}

check_fatal() {
    local file=$1 rc=0
    grep -Ein 'Fatal error:|Assertion failed:|Ensure condition failed:|Unhandled Exception:|Signal 11 caught|Segmentation fault' "$file" || rc=$?
    if (( rc != 1 )); then
        echo "FAIL: fatal/assert/ensure or unreadable log: $file" >&2; return 1
    fi
}

# Read-only runners must have identical before/after worktree bytes and status.
# Do not restore or delete files on a mismatch; retain diagnostics.
verify_readonly_snapshot() {
    local dir=$1 rc=0
    cmp -s "$dir/status-before.txt" "$dir/status-after.txt" || {
        echo 'FAIL: read-only Git status changed or missing' >&2; rc=1;
    }
    cmp -s "$dir/files-before.sha256" "$dir/files-after.sha256" || {
        echo 'FAIL: read-only project file manifest changed or missing' >&2
        diff -u "$dir/files-before.sha256" "$dir/files-after.sha256" > "$dir/file-differences.txt" || true
        rc=1
    }
    return "$rc"
}
