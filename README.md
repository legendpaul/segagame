# MEGA DODGEBALL

A complete Sega Mega Drive (Genesis) dodgeball game, built with [SGDK](https://github.com/Stephane-D/SGDK).
Pick your team, throw and catch the ball against the CPU, and win the match.

Design brief: [docs/planning.md](docs/planning.md).

## What's implemented

- **Boot**: standard Mega Drive/SGDK boot sequence (TMSS "SEGA" check) into the game.
- **Menu**: title screen with team select (4 teams, cycle with Left/Right) and a
  "PRESS START" prompt. The CPU is automatically assigned a rival team.
- **Match**: full dodgeball gameplay -
  - Left/Right to move along your baseline.
  - Press **A** to throw when you're holding the ball (you aim by where you're standing).
  - Press and hold **A** while lined up with an incoming ball to catch it (a catch turns
    the ball over to you); miss the timing/position and you take a hit and lose a life.
  - 3 lives per side per round; lose all 3 and the other side scores a point.
  - First to 3 points wins the match.
  - A basic CPU opponent moves to intercept, has a random catch chance, and throws back
    at your position with some inaccuracy.
- **Scoring/HUD**: team names, live score and lives shown on screen throughout the match.
- **Sound**: PSG sound effects for menu moves, throws, catches, hits and scoring.
- **Game over**: winner announcement with final score, returns to the menu.

All graphics are hand-authored 8x8 tiles defined directly in C (no image pipeline/resource
compiler needed), so the whole game builds from source with nothing external to fetch.

## Project layout

```
segagame/
├── src/            All game source (see below)
│   └── boot/       Cartridge header (rom_head.c) + SGDK's standard boot code
├── docs/           Design notes
├── out/            Build output (rom.bin) - not committed
└── build.bat       One-click build (Windows, requires SGDK)
```

Source modules: `main.c` (scene dispatcher), `scene_menu/match/gameover.c`, `player.c`,
`ball.c`, `ai_mgr.c` (CPU behaviour), `input_mgr.c`, `sound_mgr.c`, `sprites_data.c`
(tile art + palettes), `teams.c`, `game_state.h` (shared constants).

## Building the ROM

Requires [SGDK](https://github.com/Stephane-D/SGDK) installed locally (this project was
built and tested against an SGDK install at `C:\SGDK`).

```
build.bat
```

This produces `out\rom.bin` - a 128KB, checksummed, real Mega Drive ROM image.

## Running it

**Emulator**: open `out\rom.bin` in Fusion (or any accurate Mega Drive emulator, e.g.
BlastEm, Regen, Genesis Plus GX).

**Real hardware**: flash `out\rom.bin` onto a Mega Drive flash cart (e.g. Mega EverDrive,
Krikzz EverDrive-MD, Mega SD) exactly as you would any homebrew ROM, then boot it on a
real console/PAL or NTSC Genesis. The ROM header is region-flexible (`JUE`) so it will
boot on JP/US/EU hardware. See the flash cart's own instructions for how to copy the
`.bin` onto its SD card / USB and select it from the cart's menu.

<!-- MOTIVATIONAPP:START -->
## MotivationApp Engineering Summary

segagame remains a documentation-only planning stub for a proposed Sega Mega Drive dodgeball game styled after FIFA 94; no ROM, source code, build artifacts, or runnable output exists. The only content is a one-line README.md ('a sample game that can be used as a template') and docs/planning.md, which specifies a scene-driven architecture, TEST/PLAY startup modes, and a 12-row status table where every component (game loop, 3 scenes, input manager, player/ball entities, collision manager, AI manager, graphics, music/SFX, test framework) is marked 'Not started'. Since the prior review on this same date, no new commits or file additions have occurred; the repository is unchanged and remains pre-implementation with zero verifiable functionality.

- **Current priority:** Run 'git add docs/planning.md README.md && git commit' to bring the untracked docs/planning.md and modified README.md into version control, exactly as recommended in the prior review pass on this same date; verify with 'git status' showing a clean working tree — this action still has not been taken.
- **Larger goal:** Stand up the first executable slice: create /src with a minimal TEST-mode entry point and one isolated test per docs/planning.md's TDD philosophy, first resolving the open question of how tests are stored/selected in TEST mode; done when a TEST-mode build compiles, runs, and reports at least one passing modular test, closing the current zero-source, zero-test gap that has persisted unchanged across this and the prior review.
- **Subprojects/components analysed:** 0
- **Related projects:** No local project relationships detected.
- **Full reviewed summary:** [docs/MOTIVATIONAPP_SUMMARY.md](docs/MOTIVATIONAPP_SUMMARY.md)
- **Last reviewed:** 2026-07-19T18:29:48Z

_This section is maintained automatically by the MotivationApp Repo Insight flow._
<!-- MOTIVATIONAPP:END -->
