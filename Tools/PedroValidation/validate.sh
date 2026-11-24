#!/usr/bin/env bash
# Stable entry point for existing validation suites; authoring stays explicit.
set -euo pipefail
DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SUITE=${1:-}
if (( $# )); then shift; fi
case "$SUITE" in
    native) [[ $# -gt 0 ]] || set -- unit; exec bash "$DIR/b2b6a_run.sh" "$@" ;;
    world) [[ $# -gt 0 ]] || set -- all; exec bash "$DIR/b2b6a_run.sh" "$@" ;;
    flow) exec bash "$DIR/b2b6f_flow_run.sh" "$@" ;;
    interaction) exec bash "$DIR/b2b6e_interaction_run.sh" "$@" ;;
    maps)
        case "${1:-}" in inventory|probe|verify-test|verify-production|verify) ;;
            *) echo 'Use an explicit read-only map mode; authoring is separate.' >&2; exit 2;; esac
        exec bash "$DIR/b2b6e_editor.sh" "$@" ;;
    legacy-map) exec bash "$DIR/b2b5b2_run.sh" "$@" ;;
    *) echo "Usage: $0 native [mode]|world [mode]|flow MODE|interaction MODE|maps MODE|legacy-map [verify]" >&2; exit 2;;
esac
