## Transam Triton 

The
[Triton](https://sites.google.com/site/patrickbwarren/electronics/transam-triton)
was an [8080](https://en.wikipedia.org/wiki/Intel_8080)-based
microcomputer released in late 1978 by Transam Components Ltd, a
British company based at 12 Chapel Street, off the Edgeware Road in
north London.  Some basic information can be found in the online
[Centre for Computing History](http://www.computinghistory.org.uk/).
Recently a [YouTube
video](https://www.youtube.com/watch?v=0cSRgJ68_tM) has appeared, and
a Facebook group ('ETI Triton Single Board Computer') has sprung up.

There is now also an excellent [Triton
emulator](https://github.com/woo-j/triton) written by Robin Stuart,
that uses the [SFML library](https://www.sfml-dev.org/).  Some of the
original Triton documentation can also be found in Robin's repository.

Storage for Triton was provided by tape cassette with an interface
driven by an
[AY-5-1013](https://datasheetspdf.com/datasheet/AY-5-1013A.html)
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
chip. This serialised each data byte as 8 bits, followed by a parity
bit and 2 STOP bits, transmitted at a rate of 300 baud.  This
interface was not very reliable so at a later date (circa 1995) it was
hacked to drive an [RS-232](https://en.wikipedia.org/wiki/RS-232)
interface, first to a BBC micro, and later to a linux laptop.  This
hack intercepted the 5V output of the UART before the tape cassette
signal modulation stage.

To manage this RS-232 interface, a serial data receiver (`tridat.c`)
and transmitter (`trimcc.c`) were written, to run on a standard linux
machine.  It is these codes that are in the present repository.

They can be compiled by
```
gcc -O -Wall trimcc.c -o trimcc
gcc -O -Wall tridat.c -o tridat
```
(implemented as `make codes` in the Makefile).

The transmitter `trimcc` implements a rudimentary minilanguage
(detailed below) and can be used to compile the `.tri` source codes
(below) to `TAPE` binaries which can be loaded by Robin Stuart's
emulator.

For `trimcc`, as well as the `-o` option to save the compiled binary
output to a specified file, the `-v` option lists the compiled code
plus the defined variables, and the `-t` option attempts to transmit
the compiled bytes to a physically-connected Triton via the (default)
serial port `/dev/ttyS0`.

For `tridat` the default is to receive and print bytes from a
physically-connected Triton using the (default) serial port
`/dev/ttyS0`.  The `-o` option additionally saves these received bytes
to a file.

Note that you may have to add yourself the `dialout` group to use the
default serial port (`/dev/ttyS0`).  This is not necessary if just
compiling `.tri` codes with `trimcc -o`.

### Triton Level 7.2 ROM dumps

ROM dumps for the Triton L7.2 Monitor and BASIC are also
included.  These can be compiled to binaries by
```
./trimcc L72_0000-03ff.tri -o MONA72.ROM
./trimcc L72_0c00-0fff.tri -o MONB72.ROM
./trimcc L72_e000-ffff.tri -o BASIC72.ROM
```
(implemented as `make roms` in the Makefile).  These `.ROM` files can
be used directly with Robin Stuart's emulator (copy them into the main
directory).

### Other TriMCC codes

All codes can be compiled to `TAPE` binaries suitable for Robin
Stuart's emulator by
```
./trimcc <src_file> -o TAPE
```
Copy the resulting `TAPE` file into the main directory for Robin
Stuart's emulator and load it with the 'I' monitor command.  The 'tape
headers' are listed below.  To run these codes in Triton, use the 'G'
monitor command, with the starting address 1602 (hexadecimal).

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

[`galaxian.tri`](galaxian.tri) (tape header `GALAXIAN`) and
[`galaxian_raw.tri`](galaxian_raw.tri) -- hand-coded
[Galaxian](https://en.wikipedia.org/wiki/Galaxian) clone. Keys: '1' :
left; '2' : stop; '3' : right; 'SPACE' : fire. Enjoy!

[`invaders.tri`](invaders.tri) (tape header `INVADERS`) and
[`invaders_raw.tri`](invaders_raw.tri) -- [Space
Invaders](https://en.wikipedia.org/wiki/Space_Invaders) based on a hex
dump in Computing Today (March 1980) and converted to run with Triton
L7.2.  Keys as above: '1' : left; '3' : right; 'SPACE' : fire; and
when the game is over 'G' to start a new game.  Surprisingly good!

Note the tape header format is incorporated into these files: 64 ASCII
carriage return markers (`0D`, or ctrl-M), followed by the title (in
ASCII), followed by an ASCII space (`20`), and followed by the ASCII
END OF TRANSMISSION marker (`04`, or ctrl-D).  After this header, the
rest of the bytes read from `TAPE` are loaded into memory starting
from address 1600.  The first two bytes at 1600 are conventionally
used to indicate the end point for tape storage, so the usual entry
point for the executable part of the code is the address 1602.

### TriMCC minilanguage

This is designed to be able to handle raw machine code, 8080 op-code
mnemonics, labels and cross references, ASCII characters, and ASCII
text.  A `.tri` file is an ASCII-encoded text file consisting of a
stream of tokens separated by white space characters, commas,
semicolons, and/or newlines.  Other files can be included by using an
`include <file>` directive, which is nestable to a certain level. For
examples see `galaxian.tri` and `invaders.tri` which are wrappers for
the raw data files.  This enables the raw data files to be separately
compiled to binaries, without the tape headers.

The token stream comprises:

Raw machine code is written using hexadecimal tokens in the range `00` to `FF`.

Decimal numbers are preceded by `%` but the range is limited to 0-255
to represent a single byte.

An individual ASCII character is written as `'x'` where x is 0-9, A-Z
etc, and is replaced by the corresponding ASCII byte code.

The 8080 op-code mnemonics follow the standard naming scheme with the
exception of 'Call subroutine if carry flip-flop = logic 1' for which
the mnemonic `CCC` is used to avoid a clash with hexadecimal token
`CC`.  These are all replaced by the corresponding byte code.

An ASCII text string is designated by `"..."`, and is interpreted to
the corresponding sequence of ASCII byte codes.

Repeated tokens can be specified by a repeat count followed by `*`,
thus for example `64*OD` generates 64 ASCII carriage return markers
(see tape header below).

Comments can be included at any point: they are delimited by `#...#`
and can span multiple lines.

Variables can be defined at any time with the syntax `VAR=<val>` where
the value is represented a 16-bit word.  These can be used for example
to define monitor entry points for example.  Decimal values in the
range 0 to 65535 can be assigned using the `%` notation above.
Additionally a label of the form `LABEL:` introduces a variable of the
same name and assigns it to the value of the address counter
for the next byte to be emitted.

The value of a variable can be inserted into the stream at any point
by referencing the variable with an `!` character.  The value appears
as a little-endian 16-bit word represented by a pair of bytes.  For
example if `GETADR=020B` then `!GETADR` would generate `OB` followed
by `02`.  The low and high bytes of the word can be individually
accessed by appending `.L` and `.H` to the variable name.  Thus
`!GETADR.L` followed by `!GETADR.H` is equivalent to `!GETADR`.

A special variable `ORG` can be used to (re)set the address counter.
Typically one would use this to set the origin of the compiled code to
user-space memory (for example `ORG=1600`, see below).  Another
special variable `END` contains the value of the address counter at
the end of the compiled code (ie one after the address of the final
emitted byte).  This can be used to make tape headers.

With the above the standard tape header (here for `FILENAME`) can be
generated by
```
64*0D "FILENAME" 20 04 ORG=1600 !END
```
followed by whatever code is required.  Note that the `!END` in this
generates the end address as two bytes in a little-endian 16-bit word, as
required for the tape format.  Thus the actual code starts at the
address 1602.

Strings in Triton have to be explicitly terminated by the ASCII END OF
TRANSMISSION marker (`04` or ctrl-D) so that a typical string would
look like
```
STRING: "THIS IS A STRING" 04
```
This can be printed to the VDU by `LXI D !STRING; CALL !PSTRNG` assuming that
`PSTRNG=002B` has been assigned to the L7.2 monitor entry point to
print a string preceded by CR/LF.

The TriMCC compiler was written in C over twenty years ago and is
certainly a bit clunky by modern standards.  An ongoing project is to
replace this by something more modern written in python.

### Example TriMCC code

An example which illustrates the features of the TriMCC minilanguage
is provided in `hex2dec.tri` code
```
# Standard tape header #
64*0D "HEX2DEC" 20 04 ORG=1600 !END
 
# Entry points for Triton L7.2 monitor #
GETADR=020B PSTRNG=002B PCRLF=0033 OUTCH=0013

# Constants #
A=%10000 B=%1000 C=%100 D=%10 E=%1

# Main program loops indefinitely #
ENTRY:
LXI D !SMESSG; CALL !GETADR;
LXI D !SVALUE; CALL !PSTRNG;
CALL !PDEC; CALL !PCRLF
JMP !ENTRY

PDEC: # Prints HL in decimal #
LXI D !A; CALL !SUB
LXI D !B; CALL !SUB
LXI D !C; CALL !SUB
LXI D !D; CALL !SUB
LXI D !E; CALL !SUB
RET

SUB: # Obtain a decimal digit and print it out #
MVI B '0'; DCR B;
LOOP: INR B; MOV A,L; SUB E; MOV L,A; MOV A,H; SBB D; MOV H,A; JNC !LOOP
MOV A,L; ADD E; MOV L,A; MOV A,H; ADC D; MOV H,A;
MOV A,B; CALL !OUTCH;
RET

# Text for messages #
SMESSG: "TYPE 16-BIT WORD (0000 TO FFFF)" 04
SVALUE: "VALUE IS " 04
```
It works by subtracting the decimal numbers from 10000 to 1 from the
provided 16-bit word, and incrementing the ASCII code for
`'0'` to obtain the corresponding decimal digit (this relies on
the ASCII codes for the digits 0-9 being contiguous).

Compiling this with `./trimcc hex2dec.tri -v` results in the machine code
```
1600  0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
1610  0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
1620  0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
1630  0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D 0D
1640  48 45 58 32 44 45 43 20 04
1600  78 16 11 4E 16 CD 0B 02 11 6E 16 CD 2B 00 CD 17
1610  16 CD 33 00 C3 02 16 11 10 27 CD 36 16 11 E8 03
1620  CD 36 16 11 64 00 CD 36 16 11 0A 00 CD 36 16 11
1630  01 00 CD 36 16 C9 06 30 05 04 7D 93 6F 7C 9A 67
1640  D2 39 16 7D 83 6F 7C 8A 67 78 CD 13 00 C9 54 59
1650  50 45 20 31 36 2D 42 49 54 20 57 4F 52 44 20 28
1660  30 30 30 30 20 54 4F 20 46 46 46 46 29 04 56 41
1670  4C 55 45 20 49 53 20 04
```
and variable list
```
     ORG = 1600 = %5632        END = 1678 = %5752     GETADR = 020B = %523    
  PSTRNG = 002B = %43        PCRLF = 0033 = %51        OUTCH = 0013 = %19     
       A = 2710 = %10000         B = 03E8 = %1000          C = 0064 = %100    
       D = 000A = %10            E = 0001 = %1         ENTRY = 1602 = %5634   
  SMESSG = 164E = %5710     SVALUE = 166E = %5742       PDEC = 1617 = %5655   
     SUB = 1636 = %5686       LOOP = 1639 = %5689   
```

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

Where stated, copyright &copy; 1979-2021, 1995-2021, 2021
Patrick B Warren (PBW).  
Email: <patrickbwarren@gmail.com>.

The file [`invaders_raw.tri`](invaders_raw.tri) is based on a hex dump in
Computing Today (March 1980; page 32).  Copyright (c) is claimed in
the magazine (page 3) but the copyright holder is not identified.  No
license terms were given - the hex dump is hereby presumed to be
clonable under the equivalent of a modern open source license.  The
changes made to the code are hereby released into the public domain
(PBW, January 2021).

