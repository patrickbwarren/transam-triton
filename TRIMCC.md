## Triton RS-232 driver codes

### Introduction

The Triton tape cassette interface is driven by a
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
chip (the [AY-5-1013](https://datasheetspdf.com/datasheet/AY-5-1013A.html)),
which serialises each data byte as 8 bits, followed by a parity bit
and 2 STOP bits, transmitted at a rate of 300 baud.  This interface
was not very reliable so at a later date (circa 1995) it was hacked to
drive an [RS-232](https://en.wikipedia.org/wiki/RS-232) interface, by
intercepting the 5V output of the UART before the tape cassette signal
modulation stage. To manage this, a serial data receiver (`tridat.c`)
and transmitter (`trimcc.c`) were written, to run on a standard linux
machine.  It is these codes that are in the present repository.  They
can be compiled by issuing the command `make codes`.

The transmitter `trimcc` implements a rudimentary hybrid assembly /
machine code 'minilanguage' (detailed below) and can be used to
compile `.tri` source codes (below) to `TAPE` binaries which can be
loaded into the emulator described below, or to generate the ROMs
needed to run the emulator.

#### Receiver (`tridat.c`)
```
Receive Triton RS-232 data from /dev/ttyS0
./tridat [-o file]
-o <file>: write the byte stream in binary to a file
```
For `tridat` the default is to receive and print bytes from a
physically-connected Triton using the (default) serial port
`/dev/ttyS0`.  The `-o` option additionally writes these received bytes
to a file.

#### Transmitter (`trimcc.c`)
```
Compile and optionally transmit RS-232 data to Triton
./trimcc [-v] [-t] [-s] [-o tapefile] srcfile
-v (verbose): print the byte stream and variables
-t (transmit): attempt to transmit to /dev/ttyS0
-s (spaced): add a column of spaces after the 7th byte
-o <file>: write the byte stream in binary to a file
-o pipe: write the byte stream in binary to stdout
```
For `trimcc`, as well as the `-o <file>` option to write the compiled
binary output to a specified file, the `-v` option lists the compiled
code plus the defined variables (and the `-s` option adds an extra
column of space), and the `-t` option attempts to transmit the
compiled bytes to a physically-connected Triton via the (default)
serial port `/dev/ttyS0`.  Using `-o pipe` sends the binary to stdout
so for example one can do `./trimcc <srcfile> -o pipe | hexdump -C`.
This gets messed up if you also use the `-v` option, for obvious
reasons!
Note that you may have to add yourself the `dialout` group to use the
default serial port (`/dev/ttyS0`).  This is not necessary if just
compiling `.tri` codes with `trimcc -o`.

### TriMCC minilanguage

This is designed to be able to handle raw machine code, 8080 op-code
mnemonics, labels and cross references, ASCII characters, and ASCII
text.  A `.tri` file is an ASCII-encoded text file consisting of a
stream of tokens separated by white space characters, commas,
semicolons, and/or newlines.  Other files can be included by using an
`include <file>` directive, which is nestable to a certain level. For
example see `fastvdu_rom.tri` and `fastvdu_tape.tri`, which both
include `fastvdu.tri`.  This allows the same core code to be reused in
a TAPE file and for making a user ROM.

The token stream comprises:

- hexadecimal in the range `00` to `FF` exported as single bytes;

- hexadecimal in the range `0000` to `FFFF` exported as a pair of
  bytes in little-endian order (low byte first followed by high byte);

- decimal numbers preceded by `%` but the range is limited to 0-255 to
  represent a single byte;

- an individual ASCII character written as `'x'` (where x is 0-9, A-Z
  etc) which is exported as the corresponding ASCII byte;

- 8080 op-code mnemonics which are translated to single bytes following the
  naming scheme in the famous
  [8080A Bugbook](http://www.bugbookcomputermuseum.com/8080A-Bugbook.html),
  with the exception of 'Call subroutine if carry flip-flop = logic 1'
  for which the mnemonic `CCC` is used to avoid a clash with
  hexadecimal token `CC`;

- an ASCII text string designated by `"..."`, which is exported as the
  corresponding stream of ASCII bytes.

Comments can be included at any point: they are delimited by `#...#`
and can span multiple lines.

Variables are represented by 16-bit words and can be defined at any
time with the syntax `VAR=<val>` where `<val>` is hexadecimal.  These
can be used to define monitor entry points for example.  Decimal
values in the range 0 to 65535 can also be assigned using the `%`
notation above (note that these are stored as 16-bit words even if the
value is less then 256).  Additionally a label of the form `LABEL:`
introduces a variable of the same name and assigns it to the value of
the address counter for the next byte to be emitted.  Variables can be
re-used and always take the latest assigned value.  This means that
there can be multiple instances of `LOOP:` for example, for local
loops.

The value can be inserted into the stream at any point by referencing
the variable with an `!` character, and is exported in little-endian
order (low byte first followed by high byte).  For example if
`GETADR=020B` then `!GETADR` would generate 0x0B followed by 0x02.
The low and high bytes of the word can be individually accessed by
appending `.L` and `.H` to the variable name.  Thus `!GETADR.L` would
generate 0x0B and `!GETADR.H` would generate 0x02.

A special variable `ORG` can be used to (re)set the address counter.
Typically one would use this to set the origin of the compiled code to
user-space memory (for example `ORG=1600`, see below).  Another
special variable `END` contains the final value of the address counter
after all bytes have been generated.  This can be used to make tape
headers.

Repeated tokens can be specified by a repeat count followed by `*`,
thus for example `64*OD` generates 64 ASCII carriage return markers as
in the tape header below.  Alternatively, following a token by `>` and
a memory address expressed as a 16-bit word in hexadecimal or a
variable preceded by `!`, will export as many tokens as are needed to
fill memory up to but not including the specified memory address (if
the code has already reached the specified address then no tokens are
emitted). This can be used for example to pad out to a specified
position. For instance if the next byte to be exported would be at
address 0x1612 (based on the value of `ORG` and the number of bytes
exported thus far), and the code encounters `NOP>1620`, then the code
will emit 14 NOP op-codes (0x00) to fill to 0x161F and the next byte
to be exported will be at address 0x1620.  The same effect can be
achieved by having something like `FRAME=1620` earlier in the code,
and using `NOP>!FRAME`.  Finally, by specifying the value of `END` and
using `NOP>!END` one can pad the compiled code out to a specified
end address (see [`invaders_tape.tri`](invaders_tape.tri) for another
example).
 
With the above the standard tape header (here for `FILENAME`) can be
generated by
```
64*0D "FILENAME" 20 04 ORG=1600 !END
```

followed by whatever code is required.  Note that the `!END` in this
generates the end address as two bytes in a little-endian 16-bit word, as
required for the tape format.  Thus the actual code starts at the
address 0x1602.  The `!END` can be omitted if the end adress is hard-coded, as in 
[`invaders_tape.tri`](invaders_tape.tri)
and
[`galaxian_tape.tri`](galaxian_tape.tri)

Strings in Triton have to be explicitly terminated by the ASCII END OF
TRANSMISSION marker (0x04 or ctrl-D) so that a typical string would
look like
```
STRING: "THIS IS A STRING" 04
```
This can be printed to the VDU by `LXI D !STRING; CALL 002B` where
0x002B is the L7.2 monitor entry point to print a string preceded by
CR/LF.

The TriMCC compiler was written in C over twenty years ago and is
certainly a bit clunky by modern standards.  An ongoing project is to
replace this by something more modern written in python.

### Example TriMCC code

An example which illustrates the features of the TriMCC minilanguage
is provided in [`hex2dec.tri`](hex2dec.tri):
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

# Print HL in decimal format #
PDEC:
LXI D !A; CALL !SUB
LXI D !B; CALL !SUB
LXI D !C; CALL !SUB
LXI D !D; CALL !SUB
LXI D !E; CALL !SUB
RET

# Obtain a decimal digit and print it out #
SUB:
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
provided 16-bit word, and incrementing the ASCII code 0x30 for the digit 
'0' to obtain the corresponding decimal digit (this relies on
the ASCII codes for the digits 0-9 being contiguous).

Compiling this with `./trimcc hex2dec.tri -v -s` results in the byte stream
```
0000  0D 0D 0D 0D 0D 0D 0D 0D  0D 0D 0D 0D 0D 0D 0D 0D
0010  0D 0D 0D 0D 0D 0D 0D 0D  0D 0D 0D 0D 0D 0D 0D 0D
0020  0D 0D 0D 0D 0D 0D 0D 0D  0D 0D 0D 0D 0D 0D 0D 0D
0030  0D 0D 0D 0D 0D 0D 0D 0D  0D 0D 0D 0D 0D 0D 0D 0D
0040  48 45 58 32 44 45 43 20  04
1600  78 16 11 4E 16 CD 0B 02  11 6E 16 CD 2B 00 CD 17
1610  16 CD 33 00 C3 02 16 11  10 27 CD 36 16 11 E8 03
1620  CD 36 16 11 64 00 CD 36  16 11 0A 00 CD 36 16 11
1630  01 00 CD 36 16 C9 06 30  05 04 7D 93 6F 7C 9A 67
1640  D2 39 16 7D 83 6F 7C 8A  67 78 CD 13 00 C9 54 59
1650  50 45 20 31 36 2D 42 49  54 20 57 4F 52 44 20 28
1660  30 30 30 30 20 54 4F 20  46 46 46 46 29 04 56 41
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

### Other TriMCC codes

All `.tri` codes below can be compiled to binaries suitable for the
[emulator](EMULATOR.md) by
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

[`hex2dec_tape.tri`](hex2dec_tape.tri) (tape header `HEX2DEC`) -- convert a
16-bit word to decimal, illustrating some of the features of the
TriMCC minilanguage.

[`kbdtest_tape.tri`](kbdtest_tape.tri) (tape header `KBDTEST`) -- continuously
read from keyboard port and output to screen (for testing keyboard
emulators).

[`tapeout_tape.tri`](tapeout_tape.tri) (tape header `TAPEOUT`) -- get a
character and output to tape, repeat indefinitely (used to test the
RS-232 interface).

[`rawsave_tape.tri`](rawsave_tape.tri) (tape header `RAWSAVE`) -- outputs a
block of memory to tape and used to manufacture the ROM dumps.

[`galaxian_tape.tri`](galaxian_tape.tri) (tape header `GALAXIAN`) -- a
hand-coded [Galaxian](https://en.wikipedia.org/wiki/Galaxian)
clone. Keys: '1' : left; '2' : stop; '3' : right; 'SPACE' :
fire. Enjoy!

[`invaders_tape.tri`](invaders_tape.tri) (tape header `INVADERS`) -- a [Space
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

The above are also implemented as `make tapes` target in the Makefile.

### Triton ROM dumps

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
be used directly with the emulator.

### Fast VDU 

In Level 7.2 monitor (at least) output of a character is vectored
through 0x1479, so that by intercepting this one can fine-tune the
speed.  This is the basis for a FAST VDU user ROM which can be found
on [Gerald Sommariva's web
site](https://sites.google.com/view/transam-triton/downloads).  Here
this functionality was re-implemented in `fastvdu.tri` which can be
used to generate both a user ROM and a TAPE binary,
```
./trimcc fastvdu_tape.tri -o FASTVDU_TAPE
./trimcc fastvdu_rom.tri -o FASTVDU_ROM
```
The user ROM can be run with the emulator using `-u FASTVDU_ROM`.  If
loaded as a tape binary, the re-vectorisation of the VDU output is set
up by running the code at 0x1602 (with the 'G' monitor function).

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

