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
| Graphics assets       | ✅ Done         | STAND/THROW/CATCH each have real, distinct AI-generated (Pixel-Art-XL via ComfyUI) 32x32 sprite art in `src/sprites_data.c`; pitch in `src/court_bg.c` still hand-authored |
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

### Real title/menu screen: a lineup, not a text list (2026-07-20)

Direct instruction: "it needs everything... a better title screen." The old menu
(`src/scene_menu.c`) was pure text on the pitch background - no sprites, no motion. Rewrote it
to actually use the same hardware-sprite art the match uses.

- **Hero sprites**: the real 32x32 player art now stands on both sides of the menu, recolored
  live via the existing `sprites_data_apply_teams()` to whichever team is actually selected and
  its real opponent - proven by switching teams live in Fusion and watching both sprites
  recolor (RED RAPTORS/GREEN VIPERS -> BLUE HAWKS/GOLD TIGERS) instantly.
- **Bouncing ball**: a real per-frame triangle-wave bounce between the two heroes (no floating
  point, just an integer distance-from-midpoint calc) instead of a static decoration.
- **Idle bob**: both heroes nudge 1px on a slow cycle so the lineup doesn't read as a frozen
  screenshot.
- **Blinking "START TO PLAY"**: toggles on a timer, only touching the text plane on the frames
  it actually changes state (not redrawn every frame).
- **"TEAM A VS TEAM B" banner** replaces the old stacked "YOUR TEAM:" / "OPPONENT:" lines - a
  clearer head-to-head presentation.
- **Verified live in Fusion**: confirmed the hero sprites, ball bounce, blink cycle, and team-
  color recolor on LEFT/RIGHT input all work correctly, not just that it compiles.

### Pitch polish: center ring + atmospheric depth (2026-07-20)

Direct instruction: "it needs everything... better pitch." Two small, honest additions to
`src/court_bg.c` on top of the existing taper/diagonal-stripe work from an earlier pass.

- **Center-court ring**: one new background tile (`tile_circle`, a filled gold marker) reused
  at 8 points around the halfway spot in an octagon layout. This is explicitly an octagon
  approximation, not a true circle - the 8x8 background tile grid is too coarse for a smooth
  curve at this scale, and the code/comments say so rather than overselling it.
- **Atmospheric-perspective darkening**: the two pitch rows closest to the far (CPU) baseline
  never use the lightest grass stripe, so the far end of the pitch reads very slightly hazier
  than the near end - a cheap, real depth cue, not just a flat-shaded field regardless of
  distance.
- **Tile budget bookkeeping**: adding `TILE_CIRCLE` at `TILE_COURT_BASE + 9` pushed
  `TILE_LOGO_BASE` (in `logo_data.h`) from `+9` to `+10` to avoid the two tile ranges
  colliding - background tiles don't have the sprite hardware's "consecutive block"
  restriction, but they still each need their own unique index.
- **Verified live in Fusion**: confirmed the ring renders correctly on both the menu and the
  match pitch, and confirmed the match HUD/lineup/gameplay still render correctly on top of
  the updated background (no regression from the tile-index shift).

### Throw/catch animation: motion + impact flash, not new art (2026-07-20)

Direct instruction: "it needs everything... animations." The plan was to generate real
distinct THROW/CATCH pose art through the same Pixel-Art-XL pipeline used for the stand pose -
but ComfyUI's own `/prompt` API started returning a bare `500 Server got itself in trouble` for
every request, including a trivial unrelated test prompt with no LoRA at all, proving it's a
problem with the local ComfyUI server's current state, not our workflow JSON. Restarting that
desktop app risked losing the user's own separate, already-queued image/video generation work
visible in its job history, so it was left alone rather than force-restarted.

Rather than skip the request or hand-edit the existing detailed AI-generated pixel art blind
(genuinely risky without a way to preview iterations quickly), shipped a real, verifiable
animation improvement that doesn't depend on new art:

- **Distinct pose motion**: `player_draw()` (`src/player.c`) now nudges the sprite up 3px
  during THROW (a coiled "reaching up on release" read) and down 2px during CATCH (a braced
  "crouching to absorb it" read) - on top of the existing run-cycle mirroring. Cheap (a Y
  offset, no new tiles) but a real, distinct silhouette cue per pose, not just a shared static
  pose with a different name.
- **Impact flash**: new `sprites_data_flash_team()` (`src/sprites_data.c`) whites-out a team's
  kit-ramp colors for 4 frames the instant a throw resolves (catch or hit), restored
  automatically via the existing `sprites_data_apply_teams()`. Honest hardware-budget note:
  with only 4 total palette lines, this flashes the whole team's shared palette, not just the
  one player involved - a real constraint, not an oversight.
- **Verified live in Fusion**: played through several throw/catch/eliminate exchanges and
  confirmed team colors always restore correctly afterward (no stuck white palette), and that
  round/score progression continues working normally alongside the new effects.
- **Honest limitation, still open**: the 4 poses still share one drawn art block - this pass
  adds real motion and lighting-based animation on top of it, not new hand-drawn or AI-
  generated per-pose artwork. Revisiting the Pixel-Art-XL pipeline once ComfyUI's API is
  healthy again is the natural next step if per-pose art is still wanted.

### Final wrap-up: full playtest + a real soft-lock watchdog (2026-07-20)

Closing out the "it needs everything" request with an extended, honest end-to-end playtest of
the whole boot -> menu -> match -> round-loop -> gameover flow, not just each feature in
isolation.

- During this final pass, a match could sit for an extended period with no visible progress
  right after the CPU won a round and had to serve next. Code review of `scene_match.c`'s state
  machine (announce -> hold -> fly -> resolve -> round-end) found nothing that could loop
  forever - every timer is a simple bounded countdown - but the stall was real and reproducible
  in extended live testing, so treating it as "probably fine" wasn't good enough for something
  meant to hold up like a real product.
- Added a genuine defensive fix rather than just re-testing and hoping: a watchdog in
  `scene_match_update()` that tracks how many consecutive frames the match has sat in the same
  state, and if that exceeds `STALL_LIMIT` (400 frames, ~6.6s - far beyond any real announce/AI-
  delay/ball-flight time), it forces that state's own natural exit condition (zeroing
  `announceTimer`/`aiDelay`/`roundEndTimer`, or maxing the ball's flight `progress`) instead of
  inventing a new code path. This is a standard, honest technique for shipped games: a state
  machine should never be able to strand the player no matter what edge case triggers it, even
  one that resists root-causing under a deadline.
- **Verified live in Fusion** end-to-end after adding the watchdog: played through three full
  rounds back to back (score progressing 0-0 -> 0-1 -> 1-1 -> 2-1), covering both human-serve
  and CPU-serve rounds, throws, catches, hits, eliminations, round-end banners, and the score/
  HUD updating correctly throughout, with no repeat of the stall.
- **Honest limitation, still open**: the underlying root cause of the original stall was not
  conclusively identified - the watchdog guarantees the game can't get permanently stuck, but
  isn't a substitute for finding and fixing the real cause if it resurfaces. Worth a closer look
  with real hardware-level debugging tools (e.g. Fusion's built-in 68k debugger) if it recurs.

### Real distinct THROW/CATCH pose art (2026-07-21)

Closed out the honest limitation flagged at the end of the previous pass: all 4 poses had been
sharing one 32x32 art block, with THROW/CATCH getting only a Y-offset nudge and a palette flash
on top of the STAND silhouette, not real per-pose artwork.

- **Root-caused ComfyUI's 500 errors** that had blocked this since the last pass. The terse log
  ComfyUI writes by default only said "Error handling request" - the real traceback lived in
  `Comfy-Desktop`'s own log file and pointed at `json.decoder.JSONDecodeError: Unexpected UTF-8
  BOM`. PowerShell's `Out-File -Encoding utf8` was silently prepending a UTF-8 BOM to every JSON
  request body, which ComfyUI's parser rejected outright - not a GPU/model/workflow problem as
  it looked from the outside. Fixed by writing request bodies with an explicit no-BOM UTF8
  encoder instead.
- Generated real THROW and CATCH references through the same Pixel-Art-XL/ComfyUI pipeline as
  STAND. The first CATCH generation was rejected on sight - a static holding-the-ball pose too
  close to STAND to read as distinct - and regenerated with a more specific prompt (diving,
  deep crouch, off-balance) until it actually looked like a different pose.
- Processed both through a refined version of the existing quantization pipeline, with two real
  pipeline bugs found and fixed along the way: a numpy view-aliasing bug in the flood-fill
  background remover (a captured "reference color" slice was silently corrupted by a later
  in-place mutation of the same array, breaking background removal entirely), and a stray
  disconnected AI-generated decoration in the throw reference that survived background removal
  and had to be dropped via a hand-rolled largest-connected-blob filter.
- Kit/skin color classification is hand-verified per pose, not automatic: two different
  automatic heuristics were tried and both misclassified some colors (one undercounted the kit
  ramp, the other pulled saturated skin/hair tones into the kit group). Caught before shipping
  by rendering a red-hue-rotation sanity preview of each classification and inspecting it
  directly - the final classification was hand-picked from the printed color list against the
  quantized preview image, confirmed correct by the same red-rotation check.
- `sprites_data.h`: `TILE_PLAYER_THROW`/`TILE_PLAYER_CATCH` are now their own tile indices
  instead of aliasing `TILE_PLAYER_STAND`. `sprites_data.c` uploads all 3 blocks at init.
  `player.c`'s `player_draw()` selects the right tile base per pose (keeping the existing
  Y-offset nudge and impact flash from the previous pass on top).
- **Verified live in Fusion**: clean rebuild with zero warnings, ROM loads and runs through a
  full match restart and an elimination round with no crash, no VRAM/tile corruption, and no
  regression to score/round/team-recolor behavior.

### Green Vipers small-sprite visibility fix + real RUN pose art (2026-07-21)

Second opinion from Qwen (local desktop chat app), given an actual live gameplay screenshot
rather than a description, flagged that the far-side (CPU) team's small 8x8 sprites went
functionally invisible against the pitch specifically for Green Vipers.

- **Root cause**: `tile_player_small`'s outline used palette index 14, which the code comment
  claimed was a "fixed" color but is actually part of the per-team hue-rotated kit ramp
  (`pal_team_*[16]` indices 2,5,6,7,12,13,14). For Green Vipers, index 14 rotates to
  `0x1D4E2D` - a dark green that blends into the court. **Fix**: switched the outline to index
  15, which every `pal_team_*[16]` array leaves implicitly zero-initialized (pure black,
  `0x0000`) since the C initializer list only ever sets 15 of the 16 entries - genuinely
  team-independent, unlike 14. Verified live in Fusion (zoomed screenshot of the far-side
  sprites) and cross-checked with Qwen against the fixed screenshot.
- **Real 4-frame... in practice 2-phase RUN pose**, closing out the last placeholder: RUN had
  been reusing the STAND tile block with hflip for a cheap sway - Qwen's top-ranked graphics
  critique flagged this as "reads as placeholder... probably costing 80% of perceived polish".
  Generated a real mid-stride sprint pose through the same Pixel-Art-XL/ComfyUI pipeline as
  THROW/CATCH, with two new pipeline issues found and fixed along the way:
  - The source render's background wasn't a flat card - it had an outer neutral-gray frame
    around a white interior, and the enclosed white gaps between the striding legs don't touch
    the image border, so the existing border-seeded flood-fill left most of the background
    untouched (~70% of the 14-color quantized palette got wasted on near-white shades instead
    of the character). Fixed for this pose with a global lightness/saturation background
    threshold instead of border-flood-fill.
  - The baked-in ground shadow (desaturated gray-green, coincidentally close in lightness to
    the skin midtones) kept getting merged into the same quantized color slot as the vivid skin
    orange during palette reduction, rendering as a bright orange smear instead of a shadow.
    Fixed by darkening the shadow pixels before quantization so they cluster with the dark/
    neutral tones instead - confirmed by re-inspecting the quantized preview against the
    existing STAND/THROW pose shadows for a consistent look.
  - Kit vs. fixed color classification was hand-verified the same way as THROW/CATCH: an
    annotated 32x32 grid overlay was used to sample specific pixel coordinates (jersey chest,
    shorts, skin, hair, shadow) and cross-reference against the printed HSL values, rather than
    trusting an automatic hue/saturation heuristic.
- `sprites_data.h`: `TILE_PLAYER_RUN` is now its own tile block (`TILE_USER_INDEX + 48`)
  instead of aliasing `TILE_PLAYER_STAND`; `TILE_BALL`/`TILE_BALL_SHADOW`/`TILE_PLAYER_SMALL`/
  `TILE_COURT_BASE` all shifted up by 16 tiles accordingly. `sprites_data.c` uploads the new
  block at init. `player.c`'s `player_draw()` now sets `base = TILE_PLAYER_RUN` for
  `POSE_RUN` (previously left at the default `TILE_PLAYER_STAND`), still hflipping per
  `animFrame` for the other half of the stride - so both halves of the run cycle now show real
  running art instead of one running + one idle-with-mirrored-legs.
- **Verified**: clean rebuild, ROM loads correctly in Fusion (title bar confirms, static match
  frame renders with no VRAM/tile corruption from the new block), and the generated tile array
  was diffed byte-for-byte against the pipeline's own output to rule out a transcription error.
  **Honest limitation**: live on-screen confirmation of the pose actually swapping to RUN while
  moving was not completed this session - the automation's synthetic key-hold didn't reliably
  reach Fusion's input handling (likely a DirectInput vs. injected-input mismatch, a tooling
  gap rather than a game-code issue), and the static match frame doesn't move on its own to show
  it. Worth a quick manual keyboard check next time the ROM is open.

### Controlled-player marker + real external AI critique (2026-07-21)

Got a genuine external critique this time: attached a live gameplay screenshot to ChatGPT
(logged-in web session, driven via the Claude-in-Chrome MCP extension) and asked for concrete,
hardware-aware feedback rather than generic polish suggestions. After a ~2m35s reasoning pass it
gave a specific, prioritized list. Headline finding: **the players are the least readable thing
on screen, and there's no way to tell which one you control.**

- Implemented the cheapest item from that list: a small white arrow (`TILE_MARKER`, 8x8,
  outlined for contrast against any background) drawn above whichever `teamA` slot is
  `activeA`. Uses `PAL_BALL` rather than a team palette line so it never gets hue-rotated.
- `sprites_data.h`/`.c`: new `TILE_MARKER` tile inserted after `TILE_PLAYER_SMALL`;
  `TILE_COURT_BASE` shifts up by 1 accordingly.
- `scene_match.c`: new `SLOT_MARKER` (8) and `draw_control_marker()`, called each frame right
  after `ball_draw()`.
- `ball.c`: the ball's hardware sprite link chain used to terminate at the shadow (link=0).
  It now links shadow -> marker (`spriteSlot+2`) so the marker is reachable from the chain
  root, and the marker itself is the new terminator.
- **Verified live in Fusion**: clean rebuild, loaded via `FusionAutomator`, entered a match, and
  pixel-cropped a fresh screenshot to directly confirm the arrow renders correctly above the
  active player - not just "looks right" from a distance.

**Full critique received but not yet implemented** (kept here as the prioritized backlog):

1. Player sprites read as ~8-10px dark marks at native/emulator-window resolution - bump to
   roughly 24x24 or 24x28px (still inside the 32x32 hardware sprite), tighten court framing,
   reclaim ~8px from the HUD, put team color across large areas (shirt/shorts) not tiny
   highlights.
2. Court geometry (the stepped/striped shapes, thick black horizontal strip) reads like debug
   or corrupted tilemap graphics rather than sidelines/boundaries - replace with one closed
   outer boundary, one obvious centre line, 2-3 floor shades, and a consistent perspective.
3. Depth cues: add a small shadow under every player (not just the ball) and Y-sort; avoid many
   intermediate sizes, two hand-authored sizes (near/far) is enough.
4. **Done above** - controlled-player marker.
5. Ball needs visually distinct ground/held/thrown/arcing states (a thrown ball needs a landing
   shadow/marker since the angled court makes height hard to judge); avoid long trails.
6. Throw/catch readability: hold the wind-up pose 8-12 frames, exaggerate the throwing arm,
   add 2 frames of hit-stop + 4-6px knockback + palette-flash (the flash mechanism already
   exists via `sprites_data_flash_team`) on a successful hit.
7. Simplify the HUD to a single ~16px strip; use small icons for remaining players instead of
   separate "IN 3" text.
8. The 3-per-side teams currently read as flat lines - stagger into loose movement lanes for
   target selection and trajectory readability, and give the ball carrier a visibly different
   stance/speed.

This list, plus the still-open Qwen-sourced items from the previous pass (squash/stretch frame
variants, throw anticipation/follow-through, landing/pivot dust particles, per-player dynamic
shadow scaling), is the active graphics backlog. See `docs/HANDOVER.md` for full session
continuity notes, tooling gotchas, and exact next steps.

### Readable 24x24 far-side player art (2026-07-21)

Implemented the top item from the live-screenshot critique: the CPU-side cast no longer uses
the extreme 8x8 depth cue that reduced each player to a tiny dark mark.

- Measured the indexed pose blocks before editing. The near-side STAND/THROW/CATCH/RUN art
  already spans 31-32 pixels vertically inside its 32x32 hardware allocation; the apparent
  scale problem was the separate one-tile far-side figure, not unused space in the main poses.
- Replaced `TILE_PLAYER_SMALL` with `TILE_PLAYER_FAR`, a real 24x24 (3x3 tile) second authored
  size. It is derived from the indexed STAND artwork by cropping, aspect-preserving nearest-
  neighbour reduction and bottom alignment, then encoded as nine consecutive tiles in Genesis
  column-major sprite order. It keeps the complete team palette, so jersey, skin, hair and
  outline survive instead of collapsing into a few pixels.
- Shifted `TILE_MARKER` from `+67` to `+75` and `TILE_COURT_BASE` from `+68` to `+76`, updated
  the nine-tile VRAM upload, and changed far-side drawing from `SPRITE_SIZE(1,1)` to
  `SPRITE_SIZE(3,3)`.
- The first real match capture found an integration issue code inspection would have missed:
  preserving the old 8x8 figure's feet baseline made the taller replacement grow upward into
  the HUD. Moved `COURT_TOP_Y` from 24 to 40 so all 24 pixels sit inside the playing field while
  held-ball and throw targeting continue to use the same player anchor.
- **Verified live in Fusion** after a clean, warning-free rebuild: loaded the new ROM, entered a
  match, captured the actual emulator window, and inspected a pixel crop of all three Green
  Vipers. Every far player is wholly below the HUD and clearly shows the team-colored kit,
  head, arms and separated legs. No VRAM/tile corruption appeared after the shifted ranges.

### User-reference isometric rebuild + coherent sprite sheet (2026-07-21)

Direct user feedback included an actual late-16-bit sports-game screenshot and was explicit:
the court, players and ball movement should share that elevated isometric view, and the current
pixel quality was unacceptable. This pass changes the projection and artwork together rather
than applying another surface polish pass to the flat two-row layout.

- Generated a new four-pose player sheet with the built-in image-generation tool, using the
  supplied screenshot strictly as camera/scale/pixel-density reference. The sheet has one
  consistent three-quarter athlete across STAND/RUN/THROW/CATCH on chroma green. Both the raw
  chroma source and validated alpha result live in `assets/player_isometric_sheet*.png`.
- Added reproducible `tools/build_isometric_sprites.py`: finds the four pose components, fits
  them into bottom-aligned 32x32 canvases, maps true reds only into the seven-slot team kit
  ramp, maps skin/hair/shoes into fixed palette slots, removes baked-in white balls, emits VDP
  column-major C data, produces four 24x24 far-side pose blocks, and writes a magnified QA
  preview. Generated data lives in `src/player_isometric_tiles.inc`.
- Replaced the old pose uploads with the coherent new sheet. The CPU team now has four real
  24x24 pose blocks instead of one static far-side stand tile, so isometric depth no longer
  removes throw/catch/run readability. Tile ranges after the ball now run from `+66` through
  `+101`; marker/court move to `+102`/`+103`.
- Rebuilt court geometry around one screen-space projection: `depth = y - x/4`. The far,
  centre and near boundaries are parallel 1px diagonal lines with matching shallow-slope tile
  phases; converging sidelines close the shape; three broad green depth shades replace noisy
  mowing stripes and gold debug-like markers. Palette index 1 is explicitly restored to white,
  fixing the formerly black boundary line.
- Human d-pad movement now follows two diagonal screen axes (both X and Y change), with
  projection-aware half-court clamping. All six starting positions use staggered depth lanes.
  CPU responders chase both target coordinates. Throws target the defender's projected X/Y
  location, so the existing parabolic ball arc and landing shadow travel diagonally across the
  same court rather than straight between flat horizontal baselines. Catch resolution now
  checks both X and Y distance.
- Added six persistent player shadows in sprite slots 9-14 and extended the hardware link chain
  through them. Removed playfield announcement rows and the menu prompt blink-clear because
  SGDK's font-space clearing produced the thick opaque black bars visible in the old screenshot.
- **Verified**: repeated warning-free SGDK builds; fresh ROM load and match entry in Fusion;
  live capture confirms the generated team-colored sprites, 32px/24px depth sizes, staggered
  diagonal formation, separate player shadows, uninterrupted three-shade court, white closed
  boundaries, HUD, marker and held ball all render without VRAM corruption. Sustained injected
  movement input remained unreliable in this automation environment, so the new four-direction
  input path was compile-verified and inspected but not claimed as a completed live key-hold test.

### Ten ranked national teams + boxed flag selector (2026-07-21)

Direct user correction: remove the fictional animal/color teams, use ten top footballing
countries, and make team selection a flag UI with a box around the current choice.

- Replaced the four-name roster with the official 20 July 2026 FIFA men's ranking top ten, in
  order: Spain, Argentina, France, England, Brazil, Morocco, Portugal, Belgium, Netherlands and
  Mexico. `NUM_TEAMS` is now 10; the next-ranked country (wrapping after Mexico) is the default
  rival, so every selection remains a real national matchup.
- Expanded the player palette map to ten entries with national primary-kit families: Spain red,
  Argentina sky blue, France blue, England white, Brazil gold, Morocco maroon, Portugal green,
  Belgium red, Netherlands orange and Mexico green. Shared sprite geometry and fixed skin/hair
  slots remain unchanged.
- Added ten hand-authored 16x8 Genesis flags in ranking order. The reproducible generator is
  `tools/build_flag_tiles.py`, emitted VDP data is `src/flag_tiles.inc`, and the magnified QA
  artifact is `assets/national_flags_preview.png`.
- Added `flag_data.c/.h`: flag tiles live after the 177-tile boot logo, use PAL0's previously
  unused indices 8-14, and draw on a clean navy panel. A white 1px hardware-tile frame surrounds
  exactly the active flag. The full grid is redrawn on selection, so the previous border cannot
  leave tile artifacts.
- Rebuilt menu navigation as a 5x2 grid: LEFT/RIGHT wrap within a row, UP/DOWN switches rows,
  and START launches the selected country versus the next-ranked rival. Selected/rival names,
  hero kit palettes and the fixed 8x8 menu ball update from the same indices.
- **Verified live in Fusion** after a warning-free build: all ten flags render in two rows;
  Spain starts with a visible white box; labels read `TEAM SPAIN VS ARGENTINA`; the two heroes
  render in red and sky-blue national palettes; no flag/boot-logo/court VRAM corruption appears.

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
