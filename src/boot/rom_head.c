#include "genesis.h"

/* Custom cartridge header for MICRO RETRO DODGEBALL (based on the SGDK default
 * template). This is what shows up as the ROM's internal name in
 * emulators, flash carts and multi-game menus. */
__attribute__((externally_visible))
const ROMHeader rom_header = {
    "SEGA MEGA DRIVE ",
    "(C)LP  2026     ",
    "MICRO RETRO DODGEBALL                           ",
    "MICRO RETRO DODGEBALL                           ",
    "GM 00000001-00",
    0x000,
    "JD              ",
    0x00000000,
    0x000FFFFF,
    0xE0FF0000,
    0xE0FFFFFF,
    "RA",
    0xF820,
    0x00200000,
    0x0020FFFF,
    "            ",
    "                                        ",
    "JUE             "
};
