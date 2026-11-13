# Lost Soul of Yor — PSP prototype

This is an early source-level PSP homebrew prototype, not the complete planned game.

## Current prototype
- A simple PSP GU/GUM 3D arena made from primitive geometry.
- Analog-stick movement.
- Square: quick sword attack; Triangle: heavy attack.
- Circle: dodge; Cross: stamina action placeholder; Start: exit.
- A spectral Yor boss prototype with two phases and health/stamina bars.

## Build on GitHub Actions
1. Create a new GitHub repository.
2. Extract this ZIP and upload **the contents** (including the hidden `.github` folder) to the repository root.
3. Open the repository's **Actions** tab. If prompted, enable workflows.
4. Run **Build Lost Soul of Yor (PSP)**, or push a commit.
5. When it succeeds, open the workflow run and download the `Lost-Soul-of-Yor-EBOOT` artifact. It contains `EBOOT.PBP`.

## Important
The source has not been compiled or tested in this environment. The first remote build may expose issues that need fixing. The current visuals are primitive geometry; finished character models, textures, audio, Arabic font rendering, and the full story campaign are not included yet.
