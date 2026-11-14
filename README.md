# Lost Soul of Yor — PSP Homebrew Prototype

A native C/PSPSDK project for the PlayStation Portable. This is PSP homebrew, not a browser game.

## Current project scope

The repository currently contains a native PSP 3D action prototype with a ruined approach, two roaming hostile guards with patrol behavior, melee pursuit AI and visible attack wind-ups, three collectible memory shards and a sealed boss gate that opens after both guards are defeated and at least two shards are recovered, plus an on-screen controls and objectives tutorial, a rest altar and automatic PSP memory-stick checkpoint saves for shards, guard progress and boss damage, Yor's concealed black-robed appearance before the gate opens and her two-phase white spirit encounter afterward, lock-on, camera controls, quick/heavy attacks, dodge, stamina, directional guard, three healing flasks, pause/resume, defeat/retry, and a human-form reunion ending scene with Yor's final thank-you dialogue. Character and environment visuals are still built from primitive geometry in code; the soundtrack is synthesized in code with the PSP audio API, and there are no external model, texture, audio, or font asset files. A larger multi-area campaign, finished character models and full character animation, combat sound effects, and Arabic text rendering remain unfinished.

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
- Start: move from the title screen through the controls tutorial and story intro; pause or resume during play
- Cross: continue from the tutorial; rest at the entrance altar to refill health, stamina and flasks; retry after defeat or restart after victory

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
