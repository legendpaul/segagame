# Running MICRO RETRO DODGEBALL on real Mega Drive hardware

`out/rom.bin` (built by `build.bat`) is a standard 128KB, checksummed Mega Drive ROM
image - no special handling needed beyond what any homebrew/flash-cart ROM requires.

## What you need

- A Sega Mega Drive / Genesis console (any region - the header is set to `JUE`, so it
  boots on Japanese, US and European hardware without patching).
- A flash cartridge that accepts raw `.bin`/`.md` ROM images, e.g.:
  - Mega EverDrive (Krikzz)
  - Mega SD (Krikzz)
  - Any SD/USB-based Mega Drive flash cart with a menu/loader

## Steps

1. Build the ROM: run `build.bat` in this repo (requires SGDK - see main README).
2. Copy `out\rom.bin` onto the flash cart's SD card or USB drive, in whatever folder
   the cart's menu scans (check your cart's manual - most just need it in the root or
   a `roms`/`games` folder).
3. If your cart's tooling wants a `.md` extension instead of `.bin`, just rename the
   copy - the file contents are identical, only the extension differs.
4. Insert the SD card/USB into the flash cart, insert the cart into the console, and
   power on.
5. Select "MICRO RETRO DODGEBALL" from the cart's menu. It should boot straight to the standard
   Sega TMSS splash and then the title screen.

## Verifying it's genuinely hardware-ready

The ROM was built with the same SGDK toolchain and boot code (`sega.s`/TMSS handling)
used by essentially all Mega Drive homebrew released in the last decade, and the header
passes SGDK's own `sizebnd` size-alignment + checksum step - the two things a real
console's boot ROM actually checks before running a cartridge. It has not personally
been tested on a physical console as part of this build (no hardware was available in
this environment) - if you hit anything unexpected on real hardware, the most common
culprits are a bad flash cart write or a cart that needs the alternate `.md` extension;
the ROM itself needs no further conversion.

## Controls (physical pad)

| Button       | Action                                  |
| ------------ | ---------------------------------------- |
| D-Pad        | Move on court; hold Left/Right with a throw button to add spin |
| A            | Throw to left lane / hold to catch an incoming ball |
| B            | Throw to middle lane                    |
| C            | Throw to right lane                     |
| Start        | Confirm menu selection / begin match / return to menu after game over |
