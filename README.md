# Lost Soul of Yor — PSP Homebrew Prototype

A native C/PSPSDK project for the PlayStation Portable. This is PSP homebrew, not a browser game.

## Current project scope

The repository contains an early, playable 3D boss-arena prototype: analog movement, quick and heavy attacks, dodge, stamina, a hold-to-guard action, a two-phase Yor boss, a health HUD, and defeat/retry flow. The current visuals are drawn from primitive geometry in code; there are no external art, texture, audio, or font asset files to download. A full campaign, finished character art, audio, and Arabic font rendering are still future work.

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
- Square: quick attack
- Triangle: heavy attack
- Circle: dodge
- R: hold to guard; guarding drains stamina and a successful block costs additional stamina
- Cross: retry the fight after defeat or restart after victory
- Start: exit

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
