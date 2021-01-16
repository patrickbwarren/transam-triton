## Transam TRITON 

The TRITON was an 8080-based microcomputer released in late 1978 by
Transam Components Ltd, a British company based at 12 Chapel Street,
off the Edgeware Road in north London.  Some more information can be found
on 
[my webpage](https://sites.google.com/site/patrickbwarren/electronics/transam-triton),
and  the online [Centre for Computing
History](http://www.computinghistory.org.uk/).  Recently a [YouTube
video](https://www.youtube.com/watch?v=0cSRgJ68_tM) has appeared, and
a Facebook group has sprung up ('ETI Triton Single Board Computer').

There is now also an excellent
[emulator](https://github.com/woo-j/triton) written by Robin Stuart, that uses the
[SFML library](https://www.sfml-dev.org/).  Some of the original TRITON
documentation can also be found in Robin's repository.

Storage was provided by tape cassette with an interface driven by an
[AY-5-1013](https://datasheetspdf.com/datasheet/AY-5-1013A.html)
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
chip. This serialised bytes as 8 data bits, a parity bit, and 2 STOP
bits, transmitted at a rate of 300 baud.  This interface was not very
reliable so at a later date (circa 1995) it was hacked to drive an
[RS-232](https://en.wikipedia.org/wiki/RS-232) interface, first to a
BBC micro, and later to a linux laptop.  This hack intercepted the 5V
output of the UART before the tape cassette signal modulation stage.

To manage this modified tape / RS-232 interface, a serial data receiver (`tridat.c`)
and transmitter (`trimcc.c`) were written, to run on a standard linux
machine.  It is these codes that are in the present repository.

They can be compiled by
```
gcc -Wall tridat.c -o tridat
gcc -Wall trimcc.c -o trimcc
```
Note that 
you may have to add yourself the `dialout` group
to run them, to use the default serial port (`/dev/ttyS0`).

The transmitter code implements a rudimentary 'TriMCC' minilanguage (detailed below) and can be used to compile the `.tri` source codes (below) to `TAPE` binaries which can be loaded by Robin Stuart's emulator.

### Level 7.2 ROM dumps

ROM dumps for the TRITON Level 7.2 Monitor and BASIC are also included.  These can be compiled to binaries by
```
./trimcc -o MONA72.ROM L72_0000-03ff.tri
./trimcc -o MONB72.ROM L72_0c00-0fff.tri
./trimcc -o BASIC72.ROM L72_e000-ffff.tri
```
These `.ROM` files can be used directly with Robin Stuart's emulator (copy them into the main directory).

### Other TriMCC codes

All codes can be compiled to `TAPE` binaries suitable for Robin Stuart's emulator by
```
./trimcc -o TAPE <src_file>
```
Copy the resulting `TAPE` file into the main directory for Robin
Stuart's emulator and load it with the 'I' monitor command.  The 'tape headers' are
listed below.  To run these codes in TRITON, use the 'G' monitor command, with the starting address 1602 (hexadecimal).

`hex2dec.tri` (tape header `HEX2DEC`) - convert a 16-bit word
to decimal, illustrating some of the features of the TriMCC
minilanguage.

`tapeout.tri` (tape header `TAPEOUT`) - get a character and output to
tape, repeat indefinitely (used to test the RS-232 interface).

`rawsave.tri` (tape header `RAWSAVE`) - outputs a block of memory to
tape and used to manufacture the ROM dumps.

`galaxian.tri` (tape header `GALAXIAN`) - hand-coded [Galaxian](https://en.wikipedia.org/wiki/Galaxian)
clone. Keys: 1 - left; 2 - stop; 3 - right; spacebar - fire. Enjoy!

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
text.  For examples see the `.tri` files.

A `.tri` code is an ASCII file consisting of a stream of tokens separated by white space
characters, commas, semicolons, and/or newlines, as follows:

Raw machine code is written using hexadecimal tokens in the range `00` to `FF`.

Decimal numbers are preceded by `%` but the range is limited to 0-255 for single bytes and 0-65535 for 16-bit words.

The 8080 op-code mnemonics follow the standard naming scheme with the exception of 'Call subroutine if carry flip-flop = logic 1' for which the mnemonic `CCC` is used to avoid a clash with `CC`. 

ASCII text is delimited by `"..."` and an individual ASCII character by `'x'` where x is 0-9, A-Z etc.

Repeated tokens can be specified by a repeat count followed by `*`, thus for example `64*OD` generates 64 ASCII carriage return markers (see tape header below).

Comments can be included at any point: they are delimited by `#...#` and can span multiple lines.

Variables can be defined at any time with the syntax `VAR=<val>` where the value is represented a 2-byte hexadecimal word.  These can be used for example to define monitor entry points for example.  Decimal values can be assigned using the `%` notation above.  The high and low bytes of the word can be accessed by appending `.H` and `.L` to the variable name.

A label of the form `LABEL:` introduces a variable of this name and assigns the value to the location counter which keeps track of the number of bytes generated so far.

A special `ORG` variable can be set and this (re)sets the location counter for the labels.  Typically one would use this to set the origin of the compiled code to user-space memory (for example `ORG=1600`, see below).  Another special `END` variable is used to capture the value of the location counter at the end of the compiled code (the location of the final emitted byte).

With the above the standard tape header (here for `FILENAME`) can be generated by
```
64*0D "FILENAME" 20 04 ORG=1600 !END
...
```
followed by whatever code is required.  Note that the `!END` in this generates two bytes as a little-endian 16-bit word, as required for the tape format.  Thus the actual code starts at 1602.

Strings in TRITON are usually terminated by the ASCII
END OF TRANSMISSION marker (ASCII `04` or ctrl-D), so that a typical string would look like
```
STRING: "THIS IS A STRING" 04
```
This can be printed to the VDU by `LXI D !STRING; CALL !PSTRNG` assuming that
`PSTRNG=002B` has been assigned to the L7.2 monitor entry point to
print a string preceded by CR/LF.

The TriMCC compiler was written in C over twenty years ago and is certainly a bit clunky by modern standards.  An ongoing project is to replace this by something more modern written in python.

### Example TriMCC code

An example which illustrates the features of the TriMCC minilanguage is provided in the `hex2dec.tri` code, given also here:
```
# Standard tape header #
64*0D "HEX2DEC" 20 04 ORG=1600 !END
 
# Entry points for TRITON L7.2 monitor #
GETADR=020B PSTRNG=002B PCRLF=0033 OUTCH=0013

# Constants #
A=%10000 B=%1000 C=%100 D=%10 E=%1

# Main program #
ENTRY: LXI D !MESSG; CALL !GETADR; CALL !PDEC; 
JMP !ENTRY

# This prints HL in decimal #
PDEC:
LXI D !VAL; CALL !PSTRNG
LXI D !A; CALL !SUB
LXI D !B; CALL !SUB
LXI D !C; CALL !SUB
LXI D !D; CALL !SUB
LXI D !E; CALL !SUB
CALL !PCRLF;
RET

# Obtain a decimal digit and print it out #
SUB:   
MVI B '0'; DCR B;
LOOP: INR B; MOV A,L; SUB E; MOV L,A; MOV A,H; SBB D; MOV H,A; JNC !LOOP
MOV A,L; ADD E; MOV L,A; MOV A,H; ADC D; MOV H,A;
MOV A,B; CALL !OUTCH;
RET

# Text for messages #
MESSG: "TYPE 16-BIT WORD (0000 TO FFFF)" 04
VAL: "VALUE IS " 04
```
It works by subtracting the decimal numbers from 10000 to 1 from the
provided 16-bit word, and for each incrementing the ASCII code for
`'0'` to obtain the corresponding decimal digit (this relies on
the ASCII codes for the digits 0-9 being contiguous).

### Copying

This program (comprising the `.c` and `.tri` files)
is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see
<http://www.gnu.org/licenses/>.

### Copyright

Where indicated, copyright &copy;
1979-2021 Patrick B Warren (email: <patrickbwarren@gmail.com>).
