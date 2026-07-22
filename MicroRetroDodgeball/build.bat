@echo off
rem Builds MEGA DODGEBALL using the SGDK toolchain installed at C:\SGDK.
rem Produces out\rom.bin - load this in an emulator (e.g. Fusion) or a
rem Mega Drive flash cart to run on real hardware.

setlocal
set GDK=C:\SGDK
set PATH=%GDK%\bin;%PATH%
cd /d %~dp0

"%GDK%\bin\make" -f "%GDK%\makefile.gen" %*

if exist out\rom.bin (
    echo.
    echo Build OK: out\rom.bin
) else (
    echo.
    echo Build FAILED - see errors above.
)
endlocal
