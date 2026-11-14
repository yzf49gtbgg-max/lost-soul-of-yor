# Lost Soul of Yor — PSP Homebrew Prototype

A native C/PSPSDK project for the PlayStation Portable. This is PSP homebrew, not a browser game.

## Current project scope

The repository currently contains a native PSP 3D action prototype with a ruined approach, two roaming hostile guards with melee AI, three collectible memory shards and a sealed boss gate that opens after both guards are defeated and at least two shards are recovered, a rest altar, a two-phase Yor encounter, lock-on, camera controls, quick/heavy attacks, dodge, stamina, directional guard, three healing flasks, pause/resume, defeat/retry, and a human-form reunion ending scene. Character and environment visuals are still built from primitive geometry in code; there are no external model, texture, audio, or font asset files. A multi-area campaign, finished character models and animation, sound, and Arabic text rendering remain unfinished.

## Requirements

- PSPSDK / PSPDEV installed for your platform
- psp-gcc, psp-config, GNU Make, and the PSP SDK libraries
- PPSSPP for emulator play

Make sure the PSPSDK bin directory is on PATH. A typical PSPDEV setup exports PSPDEV and adds its bin directory to PATH.

## Build the PSP executable

From the repository root, run:

```sh
git clone https://github.com/yzf49gtbgg-max/lost-soul-of-yor.git
cd lost-soul-of-yor
make clean
make
```

A successful PSPSDK build creates EBOOT.PBP in the repository root. make clean is optional on the first build.

The Makefile is the canonical build path because it uses the PSPSDK-provided lib/build.mak rules to compile the PSP executable and package it as EBOOT.PBP. CMakeLists.txt is retained for PSP toolchains that configure create_pbp_file; without that helper, CMake builds the executable only and prints a note to use make for the PBP package.

## Run in PPSSPP

You can open EBOOT.PBP directly in PPSSPP. Alternatively, copy it to:

```text
PSP/GAME/LOSTSOUL/EBOOT.PBP
```

Then launch it from PPSSPP Games. This is an unsigned homebrew package, not a commercial PSP game image.

## Controls

- Analog stick: move
- D-pad: adjust camera direction and pitch
- L: lock on to the nearest nearby ruin guard or Yor
- Square: quick attack
- Triangle: heavy attack
- Circle: dodge
- R: hold to guard; guarding drains stamina and a successful block costs additional stamina
- Select: consume one of three healing flasks to restore up to 35 HP
- Cross: rest at the entrance altar to refill health, stamina and flasks; retry after defeat or restart after victory
- Start: open the controls tutorial from the title screen, then pause or resume during play
- Cross: continue from the controls tutorial, rest at the entrance altar, retry after defeat or restart after victory

## Automated build

GitHub Actions builds the PBP with the PSPSDK container on pushes to main and pull requests, and uploads EBOOT.PBP as a workflow artifact. If a build fails, open the failed workflow run and inspect its log before distributing the artifact.

## Repository contents

- main.c — game source
- Makefile — PSPSDK compile and PBP packaging rules
- CMakeLists.txt — optional CMake configuration
- .github/workflows/psp-build.yml — automated PSP build
- .gitignore — generated build output exclusions
- src — existing one-byte placeholder retained from the original repository

No original files are removed by this setup. The src placeholder is preserved; the source file used by both build paths remains main.c at the repository root.
