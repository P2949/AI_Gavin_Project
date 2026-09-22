# Contributing to AI Gavin Project

This document defines the basic collaboration and source-control rules for the project.

The goal is to keep `main` usable while allowing multiple people to work on Unreal Engine code, maps, Blueprints, StateTrees, and other binary assets without unnecessary conflicts or lost work.

## Before Starting

Use **Unreal Engine 5.8.2**.

Before beginning new work:

```bash
git switch main
git pull --ff-only origin main
```

Confirm the repository is clean:

```bash
git status
```

Then create a branch for the work.

Do not develop directly on `main`.

## Updating a Branch

Before creating a new branch, update `main`.

If `main` changes while you are already working, update your branch carefully.

Avoid rewriting shared branch history after other people have based work on it.

Do not force-push shared work without coordinating with the people affected.

Binary Unreal conflicts cannot usually be resolved meaningfully with normal text merge tools.

## Git LFS

Git LFS is required because Unreal assets such as `.uasset` and `.umap` files are binary files.

After cloning:

```bash
git lfs install
git lfs pull
git lfs fsck
```

Before pushing changes involving Unreal assets, checking LFS state can be useful:

```bash
git lfs status
```

Files such as `.uasset` and `.umap` are binary Unreal assets and are tracked through Git LFS.

Do not remove Unreal assets from LFS merely to make Git diffs easier.

GitHub cannot provide a meaningful text diff for most Unreal binary assets, so important binary changes should be reviewed inside Unreal Editor when necessary.

## Coordinate Binary Asset Editing

Unreal binary assets generally cannot be merged safely in the same way as C++ or configuration files.

Before making substantial changes to a shared asset, especially:

```text
.umap
Blueprints
StateTrees
Animation Blueprints
Data Assets
Materials
complex shared assets
```

coordinate with the team so that two people do not unknowingly make incompatible changes to the same file.

Maps and central gameplay Blueprints deserve particular care.

If two branches modify the same binary asset, do not blindly resolve the conflict by selecting `ours` or `theirs`.

Determine which work needs to survive and coordinate the resolution with the other contributor.

## Moving and Renaming Unreal Assets

Move, rename, duplicate, and delete Unreal assets **inside Unreal Editor using the Content Browser**.

Do not move `.uasset` or `.umap` files using:

* a terminal;
* `mv`;
* Windows Explorer;
* Finder;
* a graphical file manager;
* direct Git filesystem operations.

Unreal tracks references between assets. Moving files outside the Editor can leave references broken.

### Redirectors

Moving or renaming assets in Unreal can create redirectors at their previous locations.

After a deliberate asset reorganization, inspect and fix redirectors using the Content Browser when appropriate.

Be aware that fixing redirectors can resave other assets that reference the moved object.

For that reason:

* do not casually reorganize large parts of `Content/`;
* keep large asset moves separate from unrelated feature work;
* inspect `git status` after fixing redirectors;
* review every file that Unreal changed before committing.

## External Actors and External Objects

Directories such as:

```text
Content/__ExternalActors__/
Content/__ExternalObjects__/
```

are not disposable build output.

They can contain source data associated with Unreal levels and One File Per Actor workflows.

When a level change legitimately modifies external actor/object files, those changes may need to be committed together with the related level work.

## Saving Unreal Work

Before committing Unreal content changes:

1. finish or stop Play In Editor;
2. save the intended assets;
3. check which files Unreal modified;
4. make sure unrelated maps or assets were not accidentally saved;
5. inspect `git status`;
6. inspect Git LFS status if binary assets changed.

Do not use "Save All" without checking the resulting Git changes when several unrelated assets have been opened or modified.

## C++ Changes

C++ source belongs under `Source/`.

Commit source files and Unreal build configuration required to build the project.

Do not commit generated compiler output.

When introducing a new Unreal module or plugin, include its source and configuration, but not its generated `Binaries` or `Intermediate` output.

Avoid introducing new project-wide architectural abstractions without discussing them with the team first.

This is particularly important for shared AI systems such as:

* base AI controllers;
* enemy hierarchies;
* companion hierarchies;
* perception architecture;
* StateTree frameworks;
* Behavior Tree frameworks;
* shared blackboards;
* gameplay ability architecture;
* faction systems;
* squad systems.

These choices affect everyone and should not become project architecture accidentally.

## Files That Belong in Source Control

Examples of files that normally belong in the repository include:

```text
AI_Gavin_Project.uproject
Config/
Content/
Source/
README.md
CONTRIBUTING.md
.gitignore
.gitattributes
```

Project-owned plugins added later should also have their required source, resources, descriptors, configuration, and content committed.

## Files That Must Not Be Committed

Do not commit generated Unreal directories such as:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
```

Do not commit generated IDE/project metadata such as:

```text
.vs/
.vscode/
*.sln
*.suo
*.code-workspace
compile_commands.json
```

Do not commit compiler artifacts such as:

```text
*.o
*.obj
*.pch
```

Do not commit local profiling output such as:

```text
*.profraw
*.profdata
```

Do not commit:

* crash dumps;
* temporary files;
* local logs that are not intentionally being preserved as project evidence;
* engine installation files;
* credentials;
* API keys;
* authentication tokens;
* passwords;
* machine-specific secrets.

If a credential or token is accidentally committed, treat it as exposed and replace it rather than assuming deleting it in a later commit makes it secret again.

## Branches and Pull Requests

Use short-lived branches rather than developing directly on `main`.

Suggested prefixes are `feature/`, `fix/`, `chore/`, `docs/`, and `refactor/`.

Keep each pull request focused on one coherent change. If Unreal binary assets are changed, mention the relevant assets in the pull request so they can be reviewed in the Editor when necessary.

## Before Committing

Check the working tree:

```bash
git status --short
```

Review text changes:

```bash
git --no-pager diff
```

Check for whitespace errors:

```bash
git diff --check
```

For staged changes:

```bash
git --no-pager diff --cached
```

If binary Unreal assets changed, verify that every changed asset is intentional.

A commit should represent a coherent change that can be explained in a short sentence.


## Before Pushing

Confirm:

```bash
git status
```

Run any tests relevant to your change.

For C++ changes, make sure the appropriate Editor target builds.

For Unreal asset or gameplay changes, open the project and verify the affected content in the Editor.

Push the branch:

```bash
git push -u origin <branch-name>
```

Then create a pull request into `main`.

## General Rule

If an operation will affect a large number of shared assets, project-wide architecture, engine version, plugins, build settings, or source-control configuration, discuss it with the team before committing to the change.

It is much easier to coordinate a structural decision before several branches depend on it than to undo it afterwards.
