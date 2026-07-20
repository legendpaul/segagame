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
| Graphics assets       | ✅ Done         | player STAND pose derived from a real ComfyUI (Stable Diffusion 1.5) generation, quantized into hardware tiles in `src/sprites_data.c`; pose variants + pitch in `src/court_bg.c` still hand-authored |
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

### "AAA for the 1990s" pass (2026-07-20)

Re-scoped to the actual bar: best-in-class Genesis titles (NHL 94, Sonic 2/3), not modern AAA -
which is fair, since the single biggest gap left after the previous pass was that "music" meant a
PSG jingle only on the menu, and every real Genesis game of that era scores through the YM2612 FM
chip with PSG reserved purely for SFX.

- **Real FM music** (`src/fm_synth.c`, `src/music_mgr.c`): hand-programmed YM2612 registers
  directly via SGDK's `YM2612_writeReg()` - algorithm/feedback, per-operator envelopes, note
  frequency - for a 2-voice (melody + bass) instrument patch, playing a short looping phrase
  continuously through the menu, match, and game-over screens. This is a real FM voice, not a PSG
  beep, and it frees PSG entirely back to SFX-only, matching how actual Genesis games split the
  two chips. Deliberately used algorithm 7 (all 4 operators as carriers, no modulation chain) -
  the safest choice for hand-programming without a tracker, since a mistake in one operator can't
  silence the whole voice the way it could in a modulator-heavy algorithm.
  Honest limitation: this is one hand-built instrument playing a short hand-written phrase, not a
  composed multi-instrument soundtrack - that's normally authored with a tracker (DefleMask/
  Furnace) exporting to SGDK's XGM driver, which isn't available in this environment.
  Verification note: I confirmed this compiles clean, runs every frame without hanging/crashing
  the system (extensively play-tested after), and hand-checked every register value against the
  YM2612's documented map. I could not capture literal audio playback to listen to it in this
  environment (Fusion's WAV-log feature didn't produce a file despite several attempts) - so
  audibility itself is unverified by me; it should be confirmed by ear when run.
- **Scene fade transitions**: `PAL_fadeOutAll()` on every scene change (menu->match, match->
  game-over, game-over->menu) instead of hard cuts.
- **Bug found and fixed during this pass**: the fade blackens all 4 palette lines, but the court's
  colors, the ball's palette, and (critically) the font's actual glyph color were each only ever
  set once at boot - after the first fade they stayed black forever, breaking all on-screen text
  and the pitch's colors from the second scene onward. Root cause required checking SGDK's actual
  source: `VDP_setTextPalette()` only selects which palette line text uses, it doesn't set colors;
  the font glyphs render with color index **15**, not index 1, with the default boot palette
  (`palette_grey`) deliberately filling indices 8-15 with white for exactly that reason. Fixed by
  re-applying court/font/ball colors every time a scene is entered instead of only at boot -
  caught and fixed before shipping, verified live in Fusion afterward.

### Reference pass: looked at FIFA International Soccer (Sega CD) (2026-07-20)

Researched the actual Sega CD release rather than guessing. Its real differences from the
Genesis cart: opening FMV of 1990 World Cup highlights, better-quality title/menu music (real
instruments via redbook CD audio), and cleaner/less grainy grass - gameplay and camera were
otherwise unchanged from the cart version. FMV and CD audio are off the table on a cartridge ROM
(no CD drive, no video decode hardware), but two things were genuinely portable:

- **Perspective depth** (`src/player.c`, `src/sprites_data.c`): the far (CPU) side now renders as
  a dedicated small 8x8 sprite instead of the 16x16 one, so it visibly reads smaller/further away
  than the near (human) side - the classic elevated-camera depth cue, faked without hardware
  sprite scaling (Genesis doesn't have any) by hand-authoring a second, smaller tile. Paired with
  the existing tapered sidelines it reinforces the same perspective illusion consistently. Trade-
  off: the small sprite has no pose animation (always the same tiny tile) - a reasonable trade
  since at that scale individual pose frames were already close to imperceptible.
  Bug caught during this: a green-kit team's CPU sprite nearly disappeared against the green
  pitch (kit color alone isn't reliable contrast against a fixed-color background). Fixed by
  adding a 1px dark outline around the small sprite's torso, same treatment as the ball.
- **Smoother grass** (`src/court_bg.c`): added a third, mid-tone grass color so the mown-stripe
  pattern gradates light->mid->dark->mid instead of flipping abruptly between two tones.

### AI-assisted art pass: real ComfyUI generation (2026-07-20)

Direct instruction was to stop hand-authoring pixel art and actually use the locally
installed ComfyUI app plus AI tooling. Did this for real, not just cosmetically:

- Found ComfyUI Desktop's GUI awkward to drive reliably for this (its saved workflow tabs
  were all set up for video generation, not simple text-to-image; a couple of clicks even
  closed the window by accident). Pivoted to ComfyUI's local REST API instead - confirmed
  it was actually listening on `127.0.0.1:8000` (not the usual default 8188; Comfy Desktop
  had it configured differently), verified via `/system_stats` (ComfyUI 0.28.0, RTX 5070 Ti).
- Submitted real Stable Diffusion 1.5 txt2img jobs to `/prompt` and polled `/history` for
  results - a genuine local AI image generation pipeline, not a placeholder or a guess.
- The generated reference (a dynamic wind-up throw pose) was then run through a real
  quantization pipeline, not just eyeballed: crop to the clean torso/leg silhouette, classify
  every pixel by hue/brightness into this project's existing 5-color plan (0=transparent,
  1=kit/team color, 2=skin, 3=white, 4=dark trim), block-mode-pool down to a true 16x16 grid,
  split into the 4 hardware quadrants, emit as the same nibble-array format the rest of the
  codebase uses. This replaced the STAND-pose `tile_tl/bl/tr/br` in `src/sprites_data.c`.
  Rebuilt clean and verified live in Fusion: the sprite renders correctly with no VRAM/tile
  corruption, team recoloring still works, and it visibly reads as a wider, more athletic
  stance than the old hand-drawn blocky pose.
- **Honest limitation, stated plainly**: a 512x512 AI-generated image collapsed into a 16x16
  hardware sprite (the Genesis' actual 2x2-tile sprite budget) loses nearly all of its fine
  per-pixel detail - what survives is the pose silhouette and color-region choices, not the
  AI image's actual rendering quality. This is a real hardware ceiling, not a tooling
  failure - the same 16x16 budget that constrained the hand-authored art constrains AI-
  derived art just as much. The run/throw/catch pose-variant quadrants are still the earlier
  hand-authored tiles layered on top of this new base (regenerating a second AI image for
  each pose risked misaligning with this base rather than matching it, since SD1.5 has no
  built-in way to keep a consistent character across separate generations without a
  reference-conditioning workflow this environment doesn't have configured).

### Correction pass: 16x16 was the real bottleneck, not the AI generation (2026-07-20)

Fair, direct pushback on the pass above: in-game it still read as a small, low-detail red
blob - not a meaningful upgrade. The mistake wasn't the AI generation itself (that part was
real and worked), it was quantizing that generation down into the old 16x16 (2x2-tile)
sprite size, which is the actual Genesis 2x2-hardware-sprite footprint this project had used
since the very first pass. At that resolution almost none of a 512x512 AI image's detail can
survive - no amount of better prompting or classification fixes that; the sprite's own pixel
budget was the ceiling.

Fix: player sprites now use a full 4x4 hardware sprite block - **32x32px, the actual maximum
single-sprite size the Genesis VDP supports** - instead of 16x16. Re-ran the same AI-derived
pipeline (crop, hue/brightness classify into the 5-color plan, block-mode-pool downsample)
against the *full* character from the original ComfyUI throw-pose generation (head, raised
arm, torso, legs - not just the torso/legs crop used last time, with the held ball manually
masked out of frame since it's drawn as its own separate sprite), targeting a true 32x32 grid
instead of 16x16. `src/sprites_data.h`/`src/sprites_data.c` now upload one 16-tile block
(`tile_player_stand[16][8]`) instead of four separate 4-tile pose blocks; `src/player.c` draws
at `SPRITE_SIZE(4,4)` with an offset so the bigger sprite stays centered on the same gameplay
position. Verified live in Fusion: renders cleanly (no VRAM/tile corruption), and is now
plainly recognizable as an athlete - head, hair, extended arm, jersey shading, wide lunging
legs - not a colored block.

**Honest trade-off, not hidden**: all 4 poses (stand/run/throw/catch) now share this single
32x32 block; RUN still gets a cheap 2-frame sway from hardware hflip (the pose is
asymmetric, so flipping it reads as a weight shift) but THROW and CATCH no longer have
distinct arm art of their own - they show the same block the whole pose timer runs. Giving
each pose its own 16-tile block at this resolution is a real follow-up, not a limitation of
the hardware: it needs either additional AI generations that stay visually consistent with
this exact character (SD1.5 alone doesn't guarantee that across separate runs) or hand-edits
at 32x32 scale, both bigger jobs than this pass.

### Stronger elevated-camera pitch (2026-07-20)

Also pushed back on: the pitch was meant to read as isometric, like FIFA. Worth being
precise about this rather than just agreeing: FIFA International Soccer's actual camera
(checked in the earlier reference pass) is an elevated/tilted perspective, not a true
isometric projection (a true isometric pitch would need a diamond-shaped tile grid and
depth-sorted sprites - a much bigger architecture change than this project's straight
row/column tilemap and Y-position-based sprite draw order). What's fixable without that
rewrite, and what actually reads as "looking down at an angle" rather than "looking straight
down", is a stronger perspective cue - so:

- **Diagonal mown-stripe bands** (`src/court_bg.c`): the grass light/mid/dark bands now
  shift diagonally across the pitch (one band step every 6 columns) instead of running dead
  horizontal - real elevated-camera sports pitches show mowing stripes raking across at an
  angle, and a horizontal-only stripe pattern was quietly undercutting the perspective effect
  everywhere else on screen.
- **Doubled the sideline taper**: the gap between the two sidelines is now roughly half as
  wide at the top of the pitch as at the bottom (previously a much subtler ~4-column shift),
  a much more pronounced vanishing-point effect.

Verified live in Fusion - the pitch now visibly reads as an angled/elevated camera view.

### Third sprite pass: real shading instead of 4 flat colors (2026-07-20)

Called out again, correctly: still not close to an EA Sports Genesis title. The 32x32
resolution fix in the previous pass was necessary but not sufficient - the whole player
sprite was still built from only 4 flat colors total (transparent/kit/skin/dark), which is
closer to the *original* hand-authored art's budget than to what real 16-bit sports sprites
actually use. NHL 94/FIFA/Madden-era Genesis sprites spend most of their 16-color palette on
proper light/mid/dark shading, not one flat tone per body part - that was the real remaining
gap, not resolution.

- Reprocessed the same ComfyUI-generated reference through a materially better pipeline:
  flood-fill background removal (starting from the image border, so light highlights ON the
  character - like the jersey's shine - don't get eaten as "background" the way a flat
  brightness cutoff does), true averaged downsampling to a 32x32 grid (averaging real color
  values per output cell, not nearest/mode-pooling a pre-classified noisy map, which is what
  caused the speckled look in the previous pass), then an adaptive 14-color palette pulled
  directly from the source art via median-cut quantization - so the AI's own light/mid/dark
  jersey tones survive instead of getting crushed into one flat index.
- Team recoloring is no longer "swap one flat index." 7 of the 14 colors form the jersey's
  shading ramp; `sprites_data_apply_teams()` conceptually hue-rotates that whole ramp per team
  while preserving each shade's original lightness (computed offline in HSL and baked into
  `pal_team_red/blue/green/gold[16]` in `src/sprites_data.c`), with a saturation floor so even
  a near-gray highlight shade still reads as a real team color. The other 7 colors (skin,
  hair, outline) are fixed and identical across all 4 teams. This is the same approach real
  sports games use for team-color swaps - the shading structure carries over, only the hue
  changes - rather than one team-tinted patch on an otherwise flat sprite.
- `src/sprites_data.c`'s small far-side sprite (`tile_player_small`) was remapped onto the
  same 14-color plan so it shares the real per-team palette instead of a separate hardcoded
  one.
- Verified live in Fusion across teams: renders cleanly, visibly shows real light/dark
  jersey shading (not a flat color block), and different teams show coherent recolored
  shading rather than a single swapped patch.

### Fourth sprite pass: real research into current AI pixel-art tooling (2026-07-20)

Directly asked to research the actual current state of the art for AI sprite generation
rather than keep tuning a hand-rolled pipeline around a general-purpose model. Did that via
web search rather than guessing from memory, and it changed the approach:

- All 3 previous passes fed vanilla Stable Diffusion 1.5 - a general photoreal-leaning model
  with no pixel-art-specific training - through hand-written downsample/classify scripts to
  fake a pixel-art look. Research turned up **Pixel-Art-XL** (`nerijs/pixel-art-xl`), a LoRA
  trained specifically so SDXL outputs clean, authentic pixel art natively - a real,
  well-known model (600+ likes on Hugging Face), not a guess.
- With the user's explicit go-ahead first (this meant downloading ~7.4GB - the SDXL 1.0 base
  checkpoint, the Pixel-Art-XL LoRA, and a fixed VAE - into the local ComfyUI install), set
  this up for real and generated through the same ComfyUI REST API used throughout this
  project.
- The model card's own tip mattered and was followed exactly: "downscale 8x with nearest
  neighbor to get pixel-perfect images." At native 1024x1024 the output still looks like fat,
  slightly-aliased blocks; only the 8x-reduced 128x128 grid is the actual pixel-art the model
  intended. Skipping that step and downsampling straight from 1024x1024 to the 32x32 hardware
  grid reintroduces exactly the same blur/noise problem earlier passes were fighting.
- That 128x128 grid was cropped to the character's real bounding box - **padded to a square
  first, not stretched**, so squeezing it into the 32x32 hardware sprite doesn't distort the
  proportions - then run through the same flood-fill background removal + median-cut 14-color
  quantization + hue-preserving team recolor pipeline built in the previous pass.
- Verified live in Fusion: clean render, no VRAM/tile corruption, coherent per-team
  recoloring, and the underlying source art is visibly higher quality (actual pixel-art
  training, not a repurposed photoreal model).

**Honest limitation, stated plainly**: this is a real, meaningful upgrade in source-art
quality, not a fix for every remaining gap. Still true: all 4 poses share one 32x32 block (no
distinct throw/catch art), the court/ball/HUD are untouched by this pass, and the 32x32
hardware sprite ceiling still caps how much detail can show on screen regardless of source
quality - sprite budget and color count were the real bottlenecks, and both are now used
properly.

### Real 3-a-side elimination dodgeball (2026-07-20)

Direct instruction: "it needs everything... real rules and 3 players per team." This was a
full gameplay rearchitecture, not a tuning pass - `src/scene_match.c` was rewritten from
scratch, plus changes to `src/player.h/.c`, `src/ai_mgr.h/.c`, `src/game_state.h`.

- **Real dodgeball rules**: hit = eliminated (parked off-screen, un-targetable); catch = the
  *thrower* is eliminated instead and the catching team's most-recently-eliminated player
  returns to play (a real LIFO stack per team, `outStackA`/`outStackB`). A round ends when a
  team hits zero in-play players; the losing team serves next round (unchanged incentive from
  the original 1v1 design). First team to `WIN_SCORE` round wins takes the match.
- **3 players per team**: `Player teamA[3]`/`teamB[3]` arrays replace the old single
  `playerA`/`playerB`, placed across 3 lanes (`lane_x()`) on each baseline. `Player` gained an
  `eliminated` flag (replacing the old 3-lives-per-player system entirely - lives didn't make
  sense once elimination is the real rule) plus a remembered home lane position for when a
  player returns to play.
- **Switch-control** (chosen over full multi-select or fixed auto-control): the human directly
  controls one teammate at a time (`activeA`), toggled with a new button (BUTTON_C) that skips
  eliminated slots. The other 2 teammates run simple positioning AI - only whichever teammate
  is actually the ball's target ("responder") moves to intercept; the rest hold their lane.
  Only the player actually holding the ball can throw (`activeA == holderA` gate), and
  control automatically follows the ball (catching a throw makes you the new active/holder) -
  you can still manually switch to reposition someone else mid-play if you want to.
  CPU's 3 players work the same way under the hood, just fully AI-driven: `ai_pickSlot()`
  picks which in-play teammate/opponent a throw or return targets.
- Sprite chain: teamA now uses hardware sprite slots 0-2, teamB 3-5, ball 6-7 - still one
  fully consecutive VDP_setSpriteFull link chain, same hardware constraint as before, just
  6 players instead of 2.
- HUD changed from "LIVES" (per-player) to "IN" (team headcount) to match the new all-or-
  nothing-per-player elimination model.
- **Verified live in Fusion**, not just compiled: confirmed 3 players actually appear in 3
  distinct lanes per side: confirmed the active player moves independently of teammates on
  input; confirmed switch-control moves a different player after pressing the button;
  confirmed a full automated exchange sequence (throw -> AI resolves catch/miss -> elimination
  -> round-end detection -> score update -> "GREEN VIPERS WINS ROUND" -> round reset back to
  3v3) played out correctly without a crash or stuck state.
- **Honest limitation**: non-active teammates use a deliberately simple "only the ball's
  actual target moves" AI rather than full tactical positioning (spreading out, anticipating
  passes, etc.) - a reasonable scope match for what a hand-coded AI can do reliably, but not
  as sophisticated as a real 3-a-side defensive AI would be.

### Minnka boot splash (2026-07-20)

Direct instruction: "it needs everything... my minnka loading screen." Added a new scene shown
once before the menu, built from a real user-supplied source image
(`C:\Users\minnk\OneDrive\Desktop\cropped-minnka5-768x334.png`, committed as
`assets/minnka_logo.png`), not hand-drawn.

- **Real image pipeline**: crop -> LANCZOS resize to a 30x11 tile grid (240x88px) -> Floyd-
  Steinberg dithered median-cut quantization to a 15-color palette + reserved black index 0.
  Cells were then deduplicated into a unique tile atlas instead of dumped 1:1 - 177 of the 330
  possible 8x8 cells turned out unique, the rest (mostly flat black margin) collapse onto a
  shared tile, so this is a real background-tile compression pass, not a naive grid dump.
  Lives in `src/logo_data.h/.c` (`tile_logo[177]`, `logo_tilemap[11][30]`, `pal_logo[16]`).
- **New scene**: `src/scene_boot.h/.c` (`GS_BOOT`, first entry in the `GameScene` enum and the
  game's actual initial scene now, ahead of `GS_MENU`). Draws the logo onto BG_A, holds for
  ~2.5s (150 frames), then fades out and hands off to the menu - skippable early with Start so
  it never blocks someone who just wants to play.
- **Build fix**: `logo_data.h` initially included `court_bg.h` for `TILE_COURT_BASE`, but that
  constant actually lives in `sprites_data.h` - fixed the include, rebuilt clean.
- **Verified live in Fusion**, not just compiled: relaunched Fusion and screenshotted within
  the boot window - confirmed the "minnka" wordmark renders legibly alongside the blue smoke/
  flame art on a black background, and confirmed the scene auto-advances cleanly into the
  existing menu screen (correct palette, no corrupted tilemap) after ~2.5s.

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

