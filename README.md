# segagame

A monorepo of Sega Mega Drive (Genesis) homebrew games, each built with
[SGDK](https://github.com/Stephane-D/SGDK). Every game lives in its own top-level
folder with its own source, assets, docs and build script, so multiple games can be
developed side by side and reuse tooling/pipelines without stepping on each other.

## Games

- **[MicroRetroDodgeball](MicroRetroDodgeball/)** - a complete 3-a-side dodgeball game.
  Pick a national team, throw, dodge, chase rebounds and win the match. See
  [MicroRetroDodgeball/README.md](MicroRetroDodgeball/README.md) for what's implemented
  and [MicroRetroDodgeball/docs/planning.md](MicroRetroDodgeball/docs/planning.md) for
  the full design/build log.

## Repo layout

```
segagame/
├── MicroRetroDodgeball/   One complete game (see its own README for structure)
│   ├── src/               Game source
│   ├── assets/            Source art (pre-conversion)
│   ├── tools/              Python/PowerShell art-to-ROM pipeline scripts
│   ├── docs/               Design log + AI-session handover notes
│   ├── out/                 Build output (rom.bin) - not committed
│   └── build.bat            One-click build (Windows, requires SGDK)
└── <next game>/            Future games follow the same layout
```

Each game folder is self-contained and buildable on its own - `cd` into it and run its
`build.bat`. There's no shared build system yet; if a second game shows real overlap
with the first (e.g. a common sprite/tile pipeline, a shared UI font, common physics),
promote that code into a top-level `shared/` folder and have both games pull from it,
rather than duplicating it upfront.

## Building any game here

Requires [SGDK](https://github.com/Stephane-D/SGDK) installed locally (built and
tested against an SGDK install at `C:\SGDK`).

```
cd <GameFolder>
build.bat
```

This produces `<GameFolder>\out\rom.bin` - a checksummed, real Mega Drive ROM image.
Open it in an accurate emulator (Fusion, BlastEm, Regen, Genesis Plus GX) or flash it
to a Mega Drive flash cart (Mega EverDrive, Krikzz EverDrive-MD, Mega SD) to run on
real hardware.
