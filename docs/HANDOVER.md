# Handover — Mega Dodgeball (Sega Genesis/Mega Drive, SGDK)

Written 2026-07-21 for whichever AI/developer picks this up next. Read this before touching
code. It's organized so you can skim the headers and dive into whichever section you need.

---

## 1. What this project is

A 2-team, 3-players-per-side dodgeball game for the Sega Genesis/Mega Drive, built with SGDK
(C, m68k-elf-gcc). Elevated pseudo-isometric court view, hit/elimination and loose-ball rallies,
FM synth music, AI-generated pixel art sprites run through a custom quantization pipeline to fit
Genesis hardware constraints.

**Repo:** `C:\svn\git\segagame` (this repo). **Docs:** `docs/planning.md` is the living design
log — dated sections at the bottom document every major pass in detail, more granular than this
file. Read the last 2-3 dated sections in `planning.md` for recent technical detail this file
summarizes.

**Companion repo:** `C:\svn\git\LPUtils25` — contains `FlaUIAutoRunner`, a separate .NET project
with the `FusionAutomator` class used to drive the Fusion emulator via real UI Automation
instead of blind screen-coordinate clicking. Details in §4.

---

## 2. Build / run loop (do this first, verify your environment works)

```
cd C:\svn\git\segagame
.\build.bat
```
Output: `out\rom.bin`. A clean build currently produces zero warnings. If you see warnings you
introduced, fix them before moving on — this codebase has been kept warning-clean deliberately.

To load the ROM into Fusion and verify it visually, see §4 (FusionAutomator) — do **not** try to
drive Fusion via raw screen-coordinate clicking or synthetic `keybd_event` PowerShell scripts,
both were tried and are unreliable (see §4 for what actually works).

Git: this is a real git repo, commit as you go with descriptive messages (see the existing log
style via `git log --oneline -20`). The user has explicitly asked for git-index-lock issues to
be worked around by killing GitHub Desktop first if you hit "Operation not permitted" on
`.git/index.lock`:
```
Get-Process GitHubDesktop -ErrorAction SilentlyContinue | Stop-Process -Force
```
Then run git commands via Windows-native `git.exe` through PowerShell, not through the Linux
bash sandbox mount (it has its own separate permission quirks even after the lock clears).

---

## 3. Architecture map

```
src/
  main.c              - entry point, scene dispatcher
  scene_boot.c         - boot splash (Minnka logo)
  scene_menu.c          - title, Team 1 select and Team 2 select flow
  title_data.c/.h       - MICRO RETRO DODGEBALL arcade title tile art
  ui_data.c/.h          - custom small/large fonts, scores, panels and buttons
  scene_match.c          - THE BIG ONE: match state machine, player/ball draw loop, AI hooks
  scene_gameover.c
  player.c / player.h    - player entity: pose state machine, animation, draw
  ball.c / ball.h         - flight arc, spin, shadow, damped loose-ball rebound physics
  ai_mgr.c                 - CPU team behavior
  input_mgr.c                - joypad wrapper (BUTTON_LEFT/RIGHT/A/START etc via SGDK)
  sound_mgr.c / music_mgr.c / fm_synth.c  - audio (real YM2612 FM synth, not PSG beeps)
  sprites_data.c / .h        - ALL tile art (player poses, ball, marker) + per-team palettes
  court_bg.c / stadium_tiles.inc - authored 40x28 isometric stadium tilemap
  teams.c                      - 10 national teams in current FIFA ranking order
  flag_data.c / .h             - 5x2 flag selector, selection box, PAL0 flag colors
  logo_data.c / .h               - boot splash tile data
  game_state.h                    - global state shared across scenes

docs/
  planning.md   - living design doc, dated sections, READ THE TAIL for recent context
  HANDOVER.md   - this file

assets/stadium_source_v1.png      - preserved high-resolution generated source
assets/stadium_genesis_preview.png - exact fixed-palette 320x224 conversion
tools/build_stadium_tiles.py       - reproducible source-to-VDP converter
```

### Key architectural facts you need before editing `sprites_data.*`

- **Tile budget layout** (all offsets from `TILE_USER_INDEX`, defined in `sprites_data.h`):
  ```
  TILE_PLAYER_STAND   +0    (16 tiles, 4x4 block = 32x32px, max HW sprite size)
  TILE_PLAYER_THROW   +16   (16 tiles)
  TILE_PLAYER_PICKUP  +32   (16 tiles)
  TILE_PLAYER_RUN     +48   (16 tiles)
  TILE_BALL           +64   (4 seam-rotation tiles, 8x8)
  TILE_BALL_SHADOW    +68   (normal + compact airborne shadow at +69)
  TILE_PLAYER_FAR_STAND +70   (9 tiles, 3x3)
  TILE_PLAYER_FAR_RUN   +79   (9 tiles)
  TILE_PLAYER_FAR_THROW +88   (9 tiles)
  TILE_PLAYER_FAR_PICKUP +97   (9 tiles)
  TILE_MARKER_YELLOW   +106   (2 tiles — controlled-player ground star)
  TILE_MARKER_RED      +108   (2 tiles — possession ground star)
  TILE_COURT_BASE      +110   (481 generated stadium tiles; 488 reserved)
  TILE_LOGO_BASE       +598   (177 boot-logo tiles)
  TILE_FLAG_BASE       +775   (panel/boxes + 20 small + 80 large flag tiles)
  TILE_TITLE_BASE      +880   (large title glyphs, football and backdrop)
  TILE_UI_BASE         +948   (308 font tiles + 8 reusable panel/button tiles)
  ```
  A Genesis hardware sprite reads N×M **consecutive** VRAM tiles in column-major order (col0
  top-to-bottom, then col1...) starting at one base index — you cannot mix tiles from different
  blocks at runtime. If you add a new pose or tile block, insert it, then shift every constant
  after it by the right amount (grep `TILE_USER_INDEX +` to find them all) and update the
  `VDP_loadTileData` call in `sprites_data_init()`. This has been done cleanly 3 times this
  session (see git log) — follow the same pattern.

- **Palette lines**: only 4 total. `PAL0` = court/font (shared, not swappable). `PAL1`/`PAL2` =
  `PAL_TEAM_A`/`PAL_TEAM_B` — these are *slots*, not fixed teams; `sprites_data_apply_teams()`
  loads the actual chosen team colors into them per match. `PAL3` = `PAL_BALL`.

- **Per-team jersey palette arrays** (`pal_team_red/blue/green/gold[16]` in `sprites_data.c`)
  only initialize **15 of 16** entries in their C initializer list. Index 15 is therefore
  *implicitly* zero-initialized to `0x0000` (pure black) for every team — genuinely
  team-independent. Index 14 is **not** safe the same way; it's part of the hue-rotated "kit
  ramp" (indices `2,5,6,7,12,13,14`) and will render as a different color per team. This exact
  confusion caused a real shipped bug (Green Vipers' small-sprite outline going invisible — see
  `planning.md`'s "Green Vipers small-sprite visibility fix" section) and was fixed by moving to
  index 15. **If you need a color that must look identical across every team, use index 15, not
  14 or any other index in the kit ramp.** Fixed (non-kit) indices are `1,3,4,8,9,10,11`.

- **Sprite hardware link chain** (`scene_match.c`): Genesis hardware sprites form a linked list
  starting at slot 0; each `VDP_setSpriteFull()` call's last argument is the *next* slot in the
  chain, not sprite data. Current chain: six depth-sorted players in slots 0-5 (slot ownership is
  reassigned every frame from projected feet position) → `SLOT_BALL=6` (ball at 6, shadow at 7)
  → `SLOT_MARKER=8` → player shadows in
  slots 9-14 (slot 14 terminates, link=0). If you
  add another persistent on-screen sprite, you must extend this chain (see how `ball.c`'s
  shadow was changed from `link=0` to `link=b->spriteSlot+2` this session to make room for the
  marker) — a sprite not reachable via the chain from slot 0 is simply never rendered, with no
  error.

- **Player pose system** (`player.c`): `POSE_STAND` / `POSE_RUN` / `POSE_THROW` / `POSE_PICKUP` /
  `POSE_HIT`.
  `player_draw()` picks the tile block from the current pose and gets `hflip` only from the
  player's stable `facingLeft` team direction. RUN uses its own real art (`TILE_PLAYER_RUN`)
  with a four-beat body path; actions use anticipation, contact and recovery offsets without
  ever flipping the player away from the opposition.

- **Far-side art**: four `TILE_PLAYER_FAR_*` blocks remain available as separately encoded 24x24
  versions, but the actual match deliberately renders both teams at equal 32x32 size.

- **Team half is not sprite scale**: `Player.farSide` controls court clamping independently of
  `Player.small`. This separation is essential now that both teams intentionally use equal-sized
  sprites. Do not revert clamping to `small`.

---

## 4. Fusion emulator automation (READ THIS BEFORE TRYING TO INTERACT WITH FUSION)

Fusion 3.64 lives at `C:\Program Files\Fusion364\Fusion364\Fusion.exe`. Screen-coordinate
clicking on it is fragile (window position/focus shifts) — **use the `FusionAutomator` C# class**
instead, in the sibling repo:

```
C:\svn\git\LPUtils25\FlaUIAutoRunner\FlaUIAutoRunner\Applications\Emulators\Fusion\FusionAutomator.cs
```

A minimal runner project already wraps it:
```
C:\svn\git\LPUtils25\FlaUIAutoRunner\FusionRunner\  (Program.cs, FusionRunner.csproj)
```

**To load a ROM and verify the title bar:**
```powershell
cd 'C:\svn\git\LPUtils25\FlaUIAutoRunner\FusionRunner'
dotnet run --no-build -- 'C:\svn\git\segagame\out\rom.bin'
```
This launches/attaches to Fusion, sends `Ctrl+G` (the real menu accelerator — menu-tree
navigation was tried and found unreliable, see the FlaUIAutoRunner commit message for why),
types the ROM path into the native "Load Genesis/32X ROM" dialog via real keystrokes, and
prints title-bar text before/after plus `ROM loaded: True/False`.

**After loading, to actually get into a match** (from boot splash), the exact number of `Return`
presses needed varies — **always screenshot-verify at each step, don't assume**. Typical
sequence from a fresh load: `Return` (splash → team select, shows "CHANGE TEAM"), `Return`
(confirms, shows "START TO PLAY" hint), `Return` (starts the match). Use
`mcp__computer-use__key` with text `"Return"` — this reliably reaches Fusion's input layer.

**Two PowerShell helper scripts** exist in the outputs/scratch folder from this session (you may
need to recreate them if the scratch folder was cleared — they're short, see below):

- `focus_fusion.ps1` — closes stray File Explorer windows that sometimes appear on top of Fusion
  and steal focus, then force-focuses Fusion via `SetForegroundWindow`. **Run this before
  sending any keys to Fusion** if you're not sure it's focused.
- `capture_fusion.ps1` — takes a clean screenshot of *just* the Fusion window (via
  `GetWindowRect` + `Graphics.CopyFromScreen`, not a full-desktop screenshot with other windows
  overlapping), saves to `fusion_capture.png`. Use this instead of the generic desktop
  screenshot tool when you need to inspect game state precisely (e.g. pixel-cropping a sprite).

Both scripts are plain PowerShell using `Add-Type` for P/Invoke — **write them to a `.ps1` file
and execute via `-File`, never pass inline `-Command` strings with embedded escaped quotes**.
That specific pattern (backslash-escaped `\"` inside a single-quoted `-TypeDefinition` C# string
passed through the process-spawning tool) reliably breaks with C# compiler errors in this
environment. Also avoid `$env:VAR`, `2>$null`, `$_` inside `Where-Object` script blocks, and
`$pid` (reserved) when writing PowerShell for this environment — use `Test-Path 'env:VAR'`,
omit the redirect, `Where-Object Name -match 'x'` simplified syntax, and rename to `$procId`
respectively.

**Movement/gameplay input**: `mcp__computer-use__key` and `mcp__computer-use__hold_key` (the
official computer-use tools) reliably reach Fusion's input layer — confirmed by watching the
CPU actually score during a held-key test (0-0 → 0-2). A custom PowerShell script using raw
`keybd_event` did **not** reliably work for sustained gameplay input in one attempt this session
— stick to the official tools for anything beyond simple taps.

---

## 5. What was done this session (chronological)

1. **Restored/verified THROW/CATCH pose art** (carried over from before this session started,
   committed as `8201b34`/`c2709ba`).
2. **Screen shake on hard hits** (`6b36440`) — `scene_match.c`, pure code, zero new art.
3. **Green Vipers small-sprite visibility bug** — root-caused to palette index 14 (part of the
   hue-rotated kit ramp) being wrongly treated as team-independent; fixed by switching to index
   15 (genuinely always black). See §3 above for the general lesson.
4. **Real RUN pose art** — generated via the AI art pipeline (§6), replacing the old
   STAND+hflip placeholder. `TILE_PLAYER_RUN` is now its own 16-tile block. Both fixes committed
   together as `d5b74a4`.
5. **Got a real external critique** by attaching an actual live gameplay screenshot to ChatGPT
   (logged-in web session at chatgpt.com, driven via the `mcp__claude-in-chrome__*` MCP tools —
   see §7 for exactly how the screenshot upload was accomplished, it was non-trivial) and asking
   for specific, hardware-aware feedback. Got a genuinely useful, prioritized list back after a
   ~2.5 minute reasoning pass. Full list preserved in `planning.md`'s newest dated section and
   summarized in §8 below.
6. **Implemented the cheapest item from that critique**: a controlled-player marker (white arrow
   above the active player). Committed as `a2ec287`. Verified by pixel-cropping a fresh
   screenshot, not just eyeballing it.

Current `git log --oneline -6` for reference:
```
a2ec287 Add controlled-player marker (real screenshot critique from ChatGPT)
d5b74a4 Fix Green Vipers sprite visibility + add real RUN pose art
6b36440 Add screen shake on hard hits (impact feedback)
c2709ba Update planning.md: real distinct THROW/CATCH pose art done
8201b34 Give THROW and CATCH their own real per-pose sprite art
83ec8f2 Add match state watchdog, final full playtest pass
```

---

## 6. AI art generation pipeline (for any new sprite pose/art)

Local **ComfyUI Desktop** app running, REST API at `http://127.0.0.1:8000`. Models already
downloaded: SDXL base (`sd_xl_base_1.0.safetensors`) + **Pixel-Art-XL LoRA**
(`pixel-art-xl.safetensors`, the model that actually produces authentic pixel art — earlier
attempts with vanilla SD1.5 failed, see `planning.md`'s early sections) + fixed VAE
(`sdxl_vae.safetensors`).

**Workflow JSON pattern** (see `docs/../../../.../outputs/pose_gen/workflow_run.json` from this
session for a full working example, or reconstruct):
`CheckpointLoaderSimple → LoraLoader → CLIPTextEncode(positive) + CLIPTextEncode(negative) →
EmptyLatentImage(1024×1024) → KSampler(cfg~7-8, steps 30, euler/normal or dpmpp_2m/karras) →
VAEDecode → SaveImage`. Submit via PowerShell `Invoke-WebRequest -Method Post` to `/prompt` with
body `{client_id, prompt: <workflow>}`.

**Critical gotcha**: PowerShell's `Out-File -Encoding utf8` silently prepends a UTF-8 BOM that
breaks ComfyUI's JSON parser (silent 500 with an unhelpful top-level log — the real traceback is
in Comfy Desktop's own log file, mentions `Unexpected UTF-8 BOM`). Fix:
```powershell
[System.IO.File]::WriteAllText($path, $body, (New-Object System.Text.UTF8Encoding $false))
```

**Retrieve output images** via `GET /view?filename=X&type=output`, or poll `/history` for a
`prompt_id` → `outputs` → `images[].filename`. Prompts that "look stuck" may actually have
completed — check `/history` before assuming a submission failed (this happened once this
session; a run that looked ambiguous had actually succeeded).

**Post-processing pipeline** (Python, in `outputs/pose_gen/` from this session — recreate if
gone):
- `quantize_pose.py` — flood-fill background removal (chain-tolerant, handles gradient
  backgrounds) → 8× nearest-neighbor downsample to recover the true pixel grid → largest-
  connected-blob filter (drops stray AI-generated decorations) → crop to bbox → pad to square →
  downsample to 32×32 (the Genesis hardware max sprite size).
  - **Known failure mode**: if the source image has a hard-edged outer frame (e.g. gray border
    around a white card) rather than a uniform background touching the image edge, border-
    seeded flood-fill can't reach enclosed background regions (e.g. the gap between striding
    legs in a running pose) — up to 70% of the palette gets wasted on background shades. Fix:
    use a global lightness/saturation threshold instead of border-flood-fill for that specific
    image (done for the RUN pose this session).
- `encode_tile.py` — median-cut quantize to 14 colors + transparent → **hand-classify** each
  color as "kit" (hue-rotated per team, slots `2,5,6,7,12,13,14`) or "fixed" (identical across
  teams, slots `1,3,4,8,9,10,11`) by inspecting printed HSL values and cross-referencing against
  an annotated pixel-grid overlay of the source image (sample specific coordinates — jersey
  chest, shorts, skin, hair, shadow — don't trust an automatic hue/saturation heuristic, it has
  failed multiple times: pulled saturated skin tones into the kit group, or misjudged the skin
  ramp size). Emits a ready-to-paste `static const u32 tile_player_<name>[16][8]` C array.
  - **Known failure mode**: a baked-in ground shadow (desaturated, similar lightness to skin
    midtones) can get merged into the same quantized color slot as vivid skin orange during the
    7-way lightness-based color grouping, rendering as a bright orange smear. Fix: darken the
    shadow's pixels *before* quantization so they cluster with dark/neutral tones instead (done
    for the RUN pose this session — see the code comment in `sprites_data.c` above
    `tile_player_run`).

After generating a new tile array: paste it into `sprites_data.c`, add the tile index constant
to `sprites_data.h` (shifting everything after it, see §3), add the `VDP_loadTileData` call in
`sprites_data_init()`, wire pose selection in `player.c` if it's a new player pose, rebuild,
load in Fusion, and **pixel-crop a real screenshot to verify** — don't just eyeball a downscaled
emulator window.

---

## 7. Getting real feedback from external AI consultants (Qwen / ChatGPT)

The user has explicitly and repeatedly emphasized: **actually share real screenshots and ask for
real, specific feedback — don't just claim progress.** This worked well this session and
produced genuinely useful, actionable critique (see §8). Two working paths:

### Qwen desktop app
A real GUI chat client (`Qwen Chat`, `Qwen3.7-Plus` model). Via `computer-use` tools: click the
"+" attach button near the input box → click "Upload attachment" (careful — the popup menu also
has "Create Image"/"Create video"/"Deep Research"/"Web Dev"/"MCP" options that shift position
depending on prior state; zoom in to confirm the exact item before clicking) → a native file
picker opens → click the filename field → type the full path → Enter.

### ChatGPT via Chrome (logged-in web session)
The user is logged into `chatgpt.com` in their real Chrome browser. Drive it via the
`mcp__claude-in-chrome__*` MCP tools (load via `ToolSearch` if deferred — batch-load
`tabs_context_mcp`, `navigate`, `computer`, `read_page`/`get_page_text`,
`javascript_tool`, `find` in one call).

**Uploading a local screenshot file was the hard part** — document this for next time:
- A native OS file-open dialog cannot be driven: `computer-use` grants browsers **read-only**
  tier (visible, not clickable) by design, and the Chrome MCP extension has no access to native
  OS dialogs outside the page.
- Clipboard copy-paste (`right_click` an image → "Copy image" → paste into the input) **froze
  the CDP tab** (context menu is a native overlay, `Page.captureScreenshot` timed out) — avoid.
- `fetch()` to a local `http://localhost:PORT/...` image from the `https://chatgpt.com` page
  context is blocked by mixed-content policy. `fetch("data:image/...;base64,...")` is **also**
  blocked (ChatGPT's CSP `connect-src` doesn't exempt `data:` URLs) — both dead ends.
- **What worked**: shrink the screenshot first (resize + re-encode as JPEG quality ~70 via
  PowerShell `System.Drawing`, target ~15-20KB so the base64 stays under roughly 25K characters
  — large enough base64 strings get expensive/unwieldy to embed in a single tool call), base64-
  encode it, then run this via `javascript_tool` (`action: "javascript_exec"`) on the ChatGPT
  tab — **no network fetch involved, avoids both blocking issues above**:
  ```js
  const b64 = "...(the base64 string, embedded literally)...";
  function b64ToBlob(b64s, mime) {
    const byteChars = atob(b64s);
    const byteNumbers = new Array(byteChars.length);
    for (let i = 0; i < byteChars.length; i++) byteNumbers[i] = byteChars.charCodeAt(i);
    return new Blob([new Uint8Array(byteNumbers)], {type: mime});
  }
  const blob = b64ToBlob(b64, "image/jpeg");
  const file = new File([blob], "screenshot.jpg", {type: "image/jpeg"});
  const dt = new DataTransfer();
  dt.items.add(file);
  const input = document.querySelector('input[type=file]');
  input.files = dt.files;
  input.dispatchEvent(new Event('change', {bubbles: true}));
  ```
  Confirmed working: the thumbnail appears in the composer.
- **Sending the message**: after typing the prompt text, the visible Send button can render
  *below the current viewport* and clicking it silently does nothing. **Click into the textarea
  and press `Return` instead** — far more reliable, confirmed by the URL changing to a real
  conversation URL.
- **Waiting for the response**: with "High" reasoning effort selected, an image+text query took
  **2 minutes 35 seconds**. Poll with `get_page_text` every ~10s; you'll see incremental
  "thinking" status lines (e.g. "Critiqued sprite visibility") before the real answer appears.
  An "Answer now" link appears during extended thinking — click it if you want to force an
  earlier (possibly less thorough) answer rather than waiting out the full reasoning budget.

---

## 8. Prioritized backlog (what to do next)

This is the actual, current punch list — ranked by the source that produced it. Recommend
working top-to-bottom within each source, or picking whichever single item best matches what
you're asked to do.

### From ChatGPT's live-screenshot critique (2026-07-21), items 1 and 4 done so far:

1. ~~Scale up player sprites for legibility~~ — **done 2026-07-21**. Near-side pose art already
   occupied the full 32px height; the real problem was the CPU side's 8x8 depth cue. It is now
   a separately encoded, palette-preserving 24x24 figure, with the CPU baseline moved down so
   it sits wholly below the HUD. Verified from a fresh Fusion match screenshot and pixel crop.
2. ~~Redesign court geometry~~ — **done 2026-07-21** from a user-supplied reference. The court,
   player lanes and movement now share one `depth = y - x/4` projection, with three diagonal
   white boundaries and three broad floor shades. The opaque midcourt text strip was removed.
3. **Depth cues without hardware scaling**: player shadows and two authored sizes (32px near,
   24px far, all four poses) are **done**. True per-frame Y sorting remains open.
4. ~~Controlled-player marker~~ — **done**, `a2ec287`.
5. **Distinct ball states** — ground/held/thrown/arcing should look visually different (a
   thrown ball needs a projected landing shadow/marker since the angled court makes height hard
   to judge). Avoid long trails; 1-2 solid echo frames read as intentional motion.
6. **Throw/catch readability** — hold wind-up pose 8-12 frames, exaggerate the throwing arm
   beyond realistic proportions, add 2 frames of hit-stop + 4-6px knockback + palette-flash
   (the flash mechanism already exists — `sprites_data_flash_team()`) on a successful hit.
7. **Simplify the HUD** to a single ~16px strip; small icons for remaining-player count instead
   of "IN 3" text.
8. **Player spacing/AI**: diagonal staggered depth lanes are **done**. A dedicated slower ball-
   carrier movement speed remains open.
   Ball carrier should have a visibly different stance and ideally move slightly slower.

### Carried over from an earlier Qwen critique (see `planning.md`'s earlier dated sections):
- ~~Real run-cycle art~~ — **done** this session.
- ~~Screen shake~~ — **done**.
- Squash/stretch drawn frame variants for jump/throw (task #43 in the task list — not started).
- Anticipation + follow-through on THROW (overlaps with critique item 6 above).
- Dust particles on landing/pivots — not started.
- Dynamic shadow scaling for players — overlaps with critique item 3 above.

### Also still open / never started:
- Game modes/depth: tournament mode, difficulty settings (task #41 — explicitly deprioritized
  in favor of graphics/animation work per prior user instruction, but still on the list).

---

## 9. Tone/process notes for whoever picks this up

- The user wants **actual verified changes**, not claims. Every fix in this session was
  rebuilt, reloaded into Fusion, and confirmed via a pixel-cropped screenshot before being
  called done — keep doing that, don't report something as fixed from code review alone.
- The user explicitly wants external AI tools (Qwen, ChatGPT) used as real second opinions with
  actual screenshots attached — not simulated or described secondhand. §7 above documents
  exactly how to do this reliably; it took real trial-and-error to get working, don't skip
  straight back to guessing.
- Commit messages in this repo are intentionally detailed (see `git log`) — they document not
  just what changed but what was tried and rejected, and why, because that's genuinely useful to
  the next person (you). Keep that standard.

---

## 10. Broadcast presentation pass (2026-07-22)

The menu flow is now:

`boot -> title -> Player 1 country -> Player 2 country -> MATCH UP -> match`

The new matchup screen is implemented by `flag_data_draw_matchup()` and rendered from
`MENU_MATCHUP` in `scene_menu.c`. A or Start confirms the fixture; B returns to Player 2 select.
The match opens each round with a temporary FIFA-style flag card and whistle, then clears to a
Virtua Striker-inspired compact top scoreboard containing country flags, names, clock and score.

Critical SGDK note: plane-taking APIs in this project must receive the `VDPPlane` enum values
`BG_A`, `BG_B` or `WINDOW`. Do not pass `VDP_BG_A` / `VDP_BG_B`; in the installed SGDK those are
VRAM address macros, and using them here can silently select the wrong plane. All source call sites
were corrected in this pass after this bug was found to be the reason UI clears damaged the pitch.

Verified reference footage:

- FIFA International Soccer (Mega-CD): https://www.youtube.com/watch?v=68TFHQlugiY
- Virtua Striker 2 (Dreamcast): https://www.youtube.com/watch?v=3lSwlIjwhVY

---

## 11. Current rally rules and physics (2026-07-22)

Ignore older historical sections which describe catches; they document superseded iterations.
The live rule is now strictly `throw -> hit or miss -> loose rebound -> physical pickup`.

`scene_match.c` owns two explicit loose states: `MS_LOOSE_A` and `MS_LOOSE_B`. A human-side
loose ball assigns control to the closest surviving player but waits for the player to enter the
pickup box. A CPU-side loose ball assigns the closest survivor and visibly drives that player to
the moving ball. Do not replace either path with `ball_init(... BALL_HELD_*)`; that would restore
the teleporting possession change this pass deliberately removed.

`ball_startRicochet()` converts the completed throw vector to 8.8 fixed-point rebound velocity.
`ball_updateLoose()` applies friction, two diminishing floor bounces and damped reflection at the
projected side/back/center walls. `Ball.looseFarSide` records which receiving half owns that box.
Sound callers use the returned contact flag to trigger the plastic-wall bounce cue.

The automated Fusion rally used for QA temporarily forced throws and retrieval on both sides.
Those hooks were removed and the normal `GS_BOOT` entry restored before the final ROM build.

---

## 12. Illustrated title VRAM bank (2026-07-22)

The current title is a 475-tile full-screen illustration generated from
`assets/title_source_v2.png`, not the old per-letter `MICRO` / `RETRO` / `DODGEBALL` renderer.
Regenerate it with `python tools/build_title_tiles.py` (requires Pillow, NumPy and scikit-learn).
The fixed random seed makes the MiniBatchKMeans whole-tile clustering deterministic. The exact
console-colour preview is `assets/title_screen_v2_preview.png`.

Critical VRAM rule: do not raise the title bank above roughly 500 tiles at its current
`TILE_TITLE_BASE`. The first exact 1,099-tile attempt compiled but crossed into the default
0xB000 window-plane map region and rendered as large corrupt blocks in Fusion. The converter's
`MAX_TILES` guard prevents repeating that failure.

The title illustration temporarily overlaps `TILE_UI_BASE`; this is deliberate. `title_data_draw()`
loads the scene-local title bank, and the title-confirm path in `scene_menu.c` must call
`ui_data_init()` before `enter_selector()` to restore the UI glyph tiles. Fusion hardware capture
verified the selector is clean after this swap. The prompt is its own tiny title glyph bank and
blinks by changing tilemap entries, never by pulsing the illustration palette.
- `docs/planning.md`'s dated sections are the source of truth for implementation detail; this
  file is the map to get you oriented fast, not a replacement for reading the actual code and
  the last couple of `planning.md` entries.
