## Triton emulator

Robin Stuart has written a superb [Triton
emulator](https://github.com/woo-j/triton) that uses the [SFML
library](https://www.sfml-dev.org/).  Some of the original Triton
documentation can also be found in Robin's repository.  A fork of this
emulator is included in the present repository, to which a few small
improvements have been made.

### Usage

- the `-m` command line option can be used to set the top of memory (for example `-m 0x4000`);

- the `-t` option specifies the tape file, for example generated using `trimcc` below;

- the `-u` option can be used to load a user ROM into memory at `0x400-0x7ff`.

### Notes

The keyboard emulation strobes the data into port 3 so that bit 8 is
set the whole time that a key is depressed, and unset when it is
released.  This reflects the behaviour of the real hardware.

The 'Y' function is vectored through RAM at 0x1473 so to change the
vector change the locations at 0x1474/5.

Printer speed is controlled by the pair of bytes at 0x1402/3.

ROM dumps for the Triton L7.2 Monitor and BASIC, and TRAP (Triton
Resident Assembly Language Package), are also included.  These can be
compiled to binaries by
```
./trimcc mona72_rom.tri -o  MONA72_ROM
./trimcc monb72_rom.tri -o  MONB72_ROM
./trimcc trap_rom.tri -o    TRAP_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
```
(implemented as `make roms` in the Makefile).
These `_ROM` files are loaded by the emulator.

### Copying

Where stated, these programs are free software: you can redistribute
them and/or modify them under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

These programs are distributed in the hope that they will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with these programs.  If not, see
<http://www.gnu.org/licenses/>.

### Copyright

Unless otherwise stated, copyright &copy; 1979-2021 Patrick B Warren
<patrickbwarren@gmail.com>

The original SFML-based emulator is copyright &copy; 2020 Robin Stuart
<rstuart114@gmail.com>
