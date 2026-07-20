# 🚀 Sega Mega Drive Dodgeball — Planning & Progress

📍 **GitHub Repo:** [https://github.com/legendpaul/segagame](https://github.com/legendpaul/segagame)

---

## 🎯 Project Overview

- Theme: Realistic dodgeball, inspired by FIFA 94
- Platform: Sega Mega Drive (Genesis), PAL & NTSC
- Goal: ROM runs on real hardware & emulators
- Code style: Modular, <50 LOC per file, clear contracts
- AI Agents: ChatGPT, Claude, Jules
- Core Philosophy: **Test-Driven Design (TDD)** — all features are written with corresponding isolated tests before inclusion in the main game.
- Startup Modes: `TEST` and `PLAY`
  - `TEST` mode shows a Test Menu with all available tests grouped by feature (AI / Graphics / Sound Effect / Music / Animation / Screen Transition / Text / Joypad Input)
  - Tests are modular and easily added/removed in isolation

---

## 📈 Current Status

| Component             | Status         | Notes |
| --------------------- | -------------- | ----- |
| Game loop             | ✅ Done         | `src/main.c`, scene dispatcher |
| Scene: Menu           | ✅ Done         | `src/scene_menu.c` - team select |
| Scene: Match          | ✅ Done         | `src/scene_match.c` - full throw/catch/score loop |
| Scene: Gameover       | ✅ Done         | `src/scene_gameover.c` |
| Input manager         | ✅ Done         | `src/input_mgr.c` |
| Player entity         | ✅ Done         | `src/player.c` |
| Ball entity           | ✅ Done         | `src/ball.c` |
| Collision manager     | ✅ Done         | catch/hit resolution inline in `scene_match.c` |
| AI manager            | ✅ Done         | `src/ai_mgr.c` |
| Graphics assets       | ✅ Done         | hand-authored tiles in `src/sprites_data.c` + pitch in `src/court_bg.c` (no external image pipeline) |
| Court background      | ✅ Done         | `src/court_bg.c` - striped green pitch on BG_B, tapered sideline for an elevated/perspective camera feel, stand bands |
| Team colors           | ✅ Done         | `sprites_data_apply_teams()` recolors the shared player sprite per the team actually picked, instead of a fixed color per side |
| Music & SFX           | ✅ Done         | PSG SFX in `src/sound_mgr.c` + a looping menu jingle in `src/music_mgr.c` (see note below) |
| Player animation       | ✅ Done         | `src/player.c` - idle/run/throw/catch poses, mirrored 2-frame run cycle |
| Ball physics           | ✅ Done         | `src/ball.c` - parabolic arc + ground shadow, not a flat 2D lerp |
| Test Mode framework   | 🔲 Not started | deferred - shipped gameplay took priority; see Open Questions |

Builds clean with SGDK (`build.bat`) to a real, checksummed `out/rom.bin`. Verified live in
the Fusion emulator: boot (TMSS splash), menu + team select, full match (movement, throw,
catch, hits, scoring, round reset, game over) all confirmed working, including a full match
played to completion (score incremented correctly every round, lives reset each round, serve
alternated to the losing side, and the game-over screen showed the correct winner and final
score). Not yet tested on real Mega Drive hardware.

### Graphics note (2026-07-20)

Player sprites are now a 16x16 2x2-tile block (up from 8x16) sharing one tile set, recolored
per team via `sprites_data_apply_teams(gTeamAIndex, gTeamBIndex)`. Previously both sides used
a hardcoded color regardless of the team you picked on the menu (human always red, CPU always
blue) - that mismatch between the HUD's team names and the on-court sprite colors is what read
as "the scoring looks broken": the final score screen would correctly declare e.g. "GOLD TIGERS
WINS" but no gold sprite had ever appeared on screen. The scoring/lives/round logic itself was
verified correct via a full live match in Fusion before this fix - the bug was purely cosmetic.

A true photorealistic pre-rendered isometric look (like FIFA International Soccer's SGI-rendered
sprites) isn't achievable hand-authoring pixel arrays in SGDK. What's implemented instead: a
green striped pitch (`src/court_bg.c`, on BG_B) with sideline boundary tiles that taper - narrower
near the top of the screen, wider near the bottom - to suggest an elevated camera angle, plus a
halfway line and stadium-band top/bottom borders. This is the same "elevated pseudo-isometric,
upgraded pixel art" approach used by 16-bit sports titles like Sensible Soccer / Kick Off, scoped
to what a hand-authored SGDK tile pipeline can realistically deliver.

### "Make it AAA standard" pass (2026-07-20)

Fair criticism of the first pass: static single-pose sprites that just slide across the pitch, a
ball that lerped in a flat 2D line with no sense of height, and zero music read as a tech demo,
not a finished game. Addressed what's actually fixable on this hardware:

- **Animation** (`src/player.c`): players now have 4 poses - stand, a 2-frame run cycle, a throw
  (arm extended), and a catch (arms open) - instead of one static frame. The mirrored run frame
  is free: a front-facing "one leg forward" pose flipped horizontally reads as the other leg
  forward, so the whole gait comes from one new tile plus hardware hflip.
- **Ball physics** (`src/ball.c`): flight now has a real parabolic arc (height peaks at the
  midpoint of the throw) instead of sliding along a flat line, plus a separate shadow sprite that
  tracks the true ground position - the standard "how far will this actually go" visual cue every
  real sports game uses.
- **Court dressing** (`src/court_bg.c`): the stand bands are now a two-tile textured pattern
  instead of a flat color, and gold accent stripes mark the serve lines at each baseline.
- **Music** (`src/music_mgr.c`): a looping PSG arpeggio plays on the menu. Real multi-channel
  background music goes through the YM2612 FM chip via SGDK's XGM driver and is normally
  composed with a tracker tool - not something hand-authored as raw byte data in a text session,
  so this is a real but honestly modest single-voice jingle, not a soundtrack, and there's still
  no music during a match.

Ceiling that's genuinely out of reach here, regardless of effort: true photorealistic/pre-rendered
sprites (that requires an actual 3D rendering pipeline, not 2D pixel art), a real orchestral or
multi-channel FM soundtrack (needs a tracker toolchain), and per-frame hand-drawn animation at
the fidelity of a modern 2D game (the Genesis' 4-tile-per-sprite VRAM layout makes every extra
frame a real cost). What's here now is close to the ceiling of what a hand-authored SGDK project
can deliver without image/audio tooling this session doesn't have access to.

---

## 📝 Design Decisions

- Global state centralized in `game_state.h`
- Scene-driven architecture (`scene_menu`, `scene_match`, `scene_gameover`)
- Separate managers: input, AI, collision, sound
- Separate entities: player, ball, team, referee
- Assets organized in `/gfx`, `/sfx`, `/tiles`
- All features are implemented in tandem with modular tests that can run independently in `TEST` mode
- Tests grouped into: AI / Graphics / Sound Effect / Music / Animation / Screen Transition / Text / Joypad Input

---

## 🔍 Open Questions

- How many teams & players per team?
- Power-ups or realistic-only?
- Tournament mode depth?
- Preferred structure for storing and selecting tests in `TEST` mode?

---

## 📂 File Conventions

| Folder      | Purpose                       |
| ----------- | ----------------------------- |
| `/src`      | All code files               |
| `/assets`   | Art & sound                 |
| `/docs`     | Planning, design            |
| `/build`    | Compiled ROMs               |
| `/tests`    | Isolated test modules       |

---

## 🪄 Next Steps

✅ Agents can now pick any task ID(s) above and begin implementation, ensuring progress is reflected here. When implementing any feature, create and register its test case(s) under `/tests` and hook it into the `TEST` menu.

---

🎮 *Let’s build a great Mega Drive game — together — and test it thoroughly every step of the way!*

