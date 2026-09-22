# AI Gavin Project

This is a project for the CS4096 (artificial intelligence for games) module.

## Engine Version

**Unreal Engine 5.8.2**

All contributors should use Unreal Engine **5.8.2**.

Do not open and save the project using a newer Unreal Engine version. Unreal assets may be upgraded when saved and are not necessarily backwards-compatible.

The `.uproject` file currently records the engine association as `5.8`, so this README is the authoritative source for the required patch version.

## Repository Requirements

Before cloning the project, install:

* Git
* Git LFS
* Unreal Engine 5.8.2
* A C++ development toolchain supported by Unreal Engine 5.8.2 for your operating system

Git LFS is required because Unreal binary assets such as `.uasset` and `.umap` files are stored through Git LFS.

## Clone and Initial Setup

Clone the repository:

```bash
git clone https://github.com/P2949/AI_Gavin_Project.git
cd AI_Gavin_Project
```

Enable Git LFS and retrieve the project assets:

```bash
git lfs install
git lfs pull
git lfs fsck
```

The repository should be clean after cloning:

```bash
git status
```

Open:

```text
AI_Gavin_Project.uproject
```

using **Unreal Engine 5.8.2**.

Because this is a C++ project, Unreal may need to compile the project modules the first time it is opened.

If the Editor cannot build the modules automatically, build the `AI_Gavin_ProjectEditor` target using the build tools supplied with your Unreal Engine 5.8.2 installation.

Do not commit generated build products after compiling.

## Template Content

The existing `ThirdPerson` and `Variant_*` content/code comes from Unreal Engine template and example content.

It is currently retained as reference material and a working baseline. It should not be treated as the agreed architecture or design of the final project; the team will decide what to keep, replace, or remove.

## Platform Status

The project currently targets desktop development.

| Platform         | Current status                                                     |
| ---------------- | ------------------------------------------------------------------ |
| Linux            | Known-working development environment                              |
| Windows          | Expected development platform; team clean-clone validation pending |

This table describes the current development state rather than final shipping targets.

## Source Control

Development uses Git and GitHub, with Git LFS for Unreal binary assets.

The intended workflow is:

```text
main
  ↓
short-lived branch
  ↓
changes
  ↓
pull request
  ↓
review
  ↓
main
```

Avoid developing directly on `main`.

Before contributing, read:

```text
CONTRIBUTING.md
```

It contains the repository's branch, pull-request, Unreal asset, binary-file, and source-control rules.

## Generated Files

Generated Unreal and IDE output is intentionally excluded from version control.

Examples include:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
```

A clean clone should regenerate these locally when required.

Do not copy generated directories from another contributor's machine as part of project setup.

## Getting Help

If the project does not build or open after a clean clone:

1. confirm that Unreal Engine 5.8.2 is being used;
2. confirm Git LFS is installed;
3. run `git lfs pull`;
4. run `git lfs fsck`;
5. check `git status`;
6. rebuild the `AI_Gavin_ProjectEditor` target;
7. share the exact error and relevant log output with the team.

Avoid solving setup problems by committing generated files or replacing project files with local copies.
