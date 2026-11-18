#!/usr/bin/env python3
"""Read-only diff/audit gate for B2b-5b2, run outside Unreal Editor.

Usage: nix develop '/etc/nixos#default' --command python3 \
       Tools/PedroValidation/b2b5b2_audit.py

All output is diagnostic. Do not stage/commit until Editor verification and
actual player PIE validation succeed. External Actor files require inspection.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXPECTED_BRANCH = "feature/pedro-physical-observer-playable"
BASELINE = "8a6f31356ebd9972279d37cb9fd6777d7df05b60"
ASSET_ROOT = "Content/AI_Gavin/Encounters/Pedro/Validation/"
ASSETS = {
    ASSET_ROOT + "L_Pedro_ObserverActorTest.umap",
    ASSET_ROOT + "L_Hub_Pedro_ObserverActorTest.umap",
    ASSET_ROOT + "DA_Encounter_Pedro_ObserverActorTest.uasset",
    ASSET_ROOT + "BP_PedroObserver_Validation.uasset",
}
CODE = {
    "Tools/PedroValidation/b2b5b2_editor.py",
    "Tools/PedroValidation/b2b5b2_run.sh",
    "Tools/PedroValidation/b2b5b2_audit.py",
    "docs/encounters/pedro-observer-playable-validation.md",
}
ALLOWED = ASSETS | CODE


def git(*args: str) -> str:
    result = subprocess.run(["git", *args], cwd=ROOT, check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    return result.stdout.strip()


def check(condition: bool, message: str):
    if not condition:
        raise ValueError(message)
    print("PASS:", message)


def gather_changed() -> set[str]:
    # Includes staged changes, unstaged changes, and untracked files.
    # Use NUL separators so spaces in filenames remain unambiguous.
    tracked = subprocess.check_output(
        ["git", "diff", "--name-only", "-z", "HEAD"], cwd=ROOT)
    untracked = subprocess.check_output(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        cwd=ROOT)
    # Both -z outputs are NUL-delimited.
    return {
        part.decode("utf-8", errors="surrogateescape")
        for part in (tracked + untracked).split(b"\0") if part
    }


def main():
    check(git("branch", "--show-current") == EXPECTED_BRANCH,
          "Expected B2b-5b2 feature branch")
    subprocess.run(["git", "merge-base", "--is-ancestor", BASELINE, "HEAD"],
                   cwd=ROOT, check=True)
    check(True, "Branch descends from merged B2b-5b1 baseline")
    subprocess.run(["git", "diff", "--check", "HEAD"], cwd=ROOT, check=True)

    changed = gather_changed()
    print("\nChanged/untracked files:")
    for name in sorted(changed):
        print("  " + name)
    check(bool(changed), "There are proposed B2b-5b2 changes")

    other = changed - ALLOWED
    # External actor files can be created by World Partition / One File Per
    # Actor. Do not auto-accept these based on a hashed basename. Require a
    # manual review rather than a guessed location/reference inference.
    external = sorted(p for p in other if p.startswith(
        ("Content/__ExternalActors__/", "Content/__ExternalObjects__/")))
    other -= set(external)
    if other:
        raise ValueError("OUT-OF-SCOPE FILES changed: " + ", ".join(sorted(other)))
    if external:
        raise ValueError("External packages need explicit Editor/reference "
                         "review before committing: " + ", ".join(external))
    check(True, "No unrelated assets, production maps, or build config changed")

    missing = ASSETS - changed
    check(not missing, "All four new Unreal binary assets are present "
          "(missing: %s)" % sorted(missing))
    missing_code = CODE - changed
    check(not missing_code, "Automation source and validation documentation "
          "are present (missing: %s)" % sorted(missing_code))

    for file in sorted(ASSETS):
        check((ROOT / file).is_file(), file + " exists on disk")
        attr = git("check-attr", "filter", "--", file)
        check(attr.rsplit(":", 1)[-1].strip() == "lfs",
              file + " is configured for Git LFS")

    print("\nPASS: B2b-5b2 file-scope and LFS attribute audit")
    print("NOTE: This does NOT establish PIE success or saved map correctness.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, subprocess.CalledProcessError, OSError) as exc:
        print("FAIL:", exc, file=sys.stderr)
        sys.exit(1)
