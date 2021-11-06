## Transam Triton 

The
[Triton](https://sites.google.com/site/patrickbwarren/electronics/transam-triton)
was an [8080-based](https://en.wikipedia.org/wiki/Intel_8080)
microcomputer released in late 1978 by Transam Components Ltd, a
British company based at 12 Chapel Street, off the Edgeware Road in
north London.  Some basic information can be found in the online
[Centre for Computing History](http://www.computinghistory.org.uk/).
Recently a [YouTube
video](https://www.youtube.com/watch?v=0cSRgJ68_tM) has appeared,
another [web site](https://sites.google.com/view/transam-triton/) has sprung up, and
a Facebook group ('ETI Triton 8080 Vintage Computer') has been started.
I have a small web page detailing some of the history
[here](https://sites.google.com/site/patrickbwarren/electronics/transam-triton).

Storage for Triton was provided by tape cassette with an interface
driven by a
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
chip (the
[AY-5-1013](https://datasheetspdf.com/datasheet/AY-5-1013A.html)).
This serialised each data byte as 8 bits, followed by a parity
bit and 2 STOP bits, transmitted at a rate of 300 baud.  This
interface was not very reliable so at a later date (circa 1995) it was
hacked to drive an [RS-232](https://en.wikipedia.org/wiki/RS-232)
interface, first to a BBC micro, and later to a linux laptop.  This
hack intercepted the 5V output of the UART before the tape cassette
signal modulation stage.

To manage this RS-232 interface, a serial data receiver (`tridat.c`)
and transmitter (`trimcc.c`) were written, to run on a standard linux
machine.  It is these codes that are in the present repository.
They can be compiled by issuing the command `make codes`.

The transmitter `trimcc` implements a rudimentary minilanguage
(detailed below) and can be used to compile `.tri` source codes
(below) to `TAPE` binaries which can be loaded into the
emulator described below.

For `trimcc`, as well as the `-o <file>` option to write the compiled
binary output to a specified file, the `-v` option lists the compiled
code plus the defined variables (and the `-s` option adds an extra
column of space), and the `-t` option attempts to transmit the
compiled bytes to a physically-connected Triton via the (default)
serial port `/dev/ttyS0`.  Using `-o pipe` sends the binary to stdout
so for example one can do `./trimcc <srcfile> -o pipe | hexdump -C`.
This gets messed up if you also use the `-v` option, for obvious
reasons!

For `tridat` the default is to receive and print bytes from a
physically-connected Triton using the (default) serial port
`/dev/ttyS0`.  The `-o` option additionally writes these received bytes
to a file.

Note that you may have to add yourself the `dialout` group to use the
default serial port (`/dev/ttyS0`).  This is not necessary if just
compiling `.tri` codes with `trimcc -o`.

#### Notes

The 'Y' function is vectored through RAM at 0x1473 so to change the
vector change the locations at 0x1474/5.

Printer speed is controlled by the pair of bytes at 0x1402/3.

### Triton emulator

Robin Stuart has written a superb [Triton
emulator](https://github.com/woo-j/triton) that uses the [SFML
library](https://www.sfml-dev.org/).  Some of the original Triton
documentation can also be found in Robin's repository.  A fork of this
emulator is included in the present repository, to which a few small
improvements have been made:

- the keyboard emulation now correctly strobes the data into port 3 so that
bit 8 is set the whole time that a key is depressed, and unset when
it is released;

- the `-m` command line option can be used to set the top of memory (for example `-m 0x4000`);

- the `-t` option specifies the tape file, for example generated using `trimcc` below;

- the `-u` option can be used to load a user ROM into memory at `0x400-0x7ff`.

### Triton Level 7.2 ROM dumps

ROM dumps for the Triton L7.2 Monitor and BASIC, and TRAP (Triton
Resident Assembly Language Package), are also included.  These can be
compiled to binaries by
```
./trimcc mona72_rom.tri -o  MONA72_ROM
./trimcc monb72_rom.tri -o  MONB72_ROM
./trimcc trap_rom.tri -o    TRAP_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
```
(implemented as `make roms` in the Makefile).  These `_ROM` files can
be used directly with Robin Stuart's emulator.

### Other TriMCC codes

All `.tri` codes below can be compiled to binaries suitable for
Robin Stuart's emulator by
```
./trimcc <src_file> -o <tape_file>
```
To load one of these into the emulator use the `-t <tape_file` command
line option, and then from within the emulator load the tape file with
the 'I' monitor command.  The 'tape headers' are listed below.  Note
that you can concatenate the binaries into a singe tape file with `cat
*_TAPE > COMBI_TAPE` for instance, then load this combi-tape into the
emulator and pick out which code you want to load by using the I
command with the appropriate tape header.

To run these codes in Triton, use the 'G'
monitor command, with the starting address 0x1602.

[`hex2dec.tri`](hex2dec.tri) (tape header `HEX2DEC`) -- convert a
16-bit word to decimal, illustrating some of the features of the
TriMCC minilanguage.

[`kbdtest.tri`](kbdtest.tri) (tape header `KBDTEST`) -- continuously
read from keyboard port and output to screen (for testing keyboard
emulators).

[`tapeout.tri`](tapeout.tri) (tape header `TAPEOUT`) -- get a
character and output to tape, repeat indefinitely (used to test the
RS-232 interface).

[`rawsave.tri`](rawsave.tri) (tape header `RAWSAVE`) -- outputs a
block of memory to tape and used to manufacture the ROM dumps.

[`galaxian.tri`](galaxian.tri) (tape header `GALAXIAN`) wraps
[`galaxian_raw.tri`](galaxian_raw.tri) -- a hand-coded
[Galaxian](https://en.wikipedia.org/wiki/Galaxian) clone. Keys: '1' :
left; '2' : stop; '3' : right; 'SPACE' : fire. Enjoy!

[`invaders.tri`](invaders.tri) (tape header `INVADERS`) wraps
[`invaders_raw.tri`](invaders_raw.tri) -- a [Space
Invaders](https://en.wikipedia.org/wiki/Space_Invaders) clone modified
from a hex dump in Computing Today (March 1980).  Keys as above: '1' :
left; '3' : right; 'SPACE' : fire; and when the game is over 'G' to
start a new game. Currently if the complete fleet of invaders is wiped
out, another fleet doesn't appear - this seems to be a bug.  Apart
from this it's surprisingly good!

Note the tape header format is incorporated as 64 ASCII carriage
return markers (0x0D, or ctrl-M), followed by the title (in ASCII),
followed by an ASCII space (0x20), and followed by the ASCII END OF
TRANSMISSION marker (0x04, or ctrl-D).  After this header, the rest of
the bytes are loaded into memory starting from address 0x1600.  The
first two bytes at 0x1600 are conventionally used to indicate the end
point for tape storage, so that the usual entry point for the
executable part of the code is the address 0x1602.

More details of the TriMCC minilanguage are given in [TRIMCC.md](TRIMCC.md).

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

Where stated, copyright &copy; 1979-2021 Patrick B Warren (PBW)
<patrickbwarren@gmail.com>

Apart from the modifications the emulator is copyright &copy; 2020
Robin Stuart <rstuart114@gmail.com>

The file [`invaders_raw.tri`](invaders_raw.tri) is modified from a hex
dump in Computing Today (March 1980; page 32).  Copyright (c) is
claimed in the magazine (page 3) but the copyright holder is not
identified.  No license terms were given and the code is hereby presumed
to be reproducible and modifiable under the equivalent of a modern
open source license.  The current changes made to the code are hereby
released into the public domain (PBW, January 2021).

