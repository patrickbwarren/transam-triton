## Triton RS-232 driver codes

### Introduction

The Triton tape cassette interface is driven by a
[UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
chip (the
[AY-5-1013](https://datasheetspdf.com/datasheet/AY-5-1013A.html)),
which serialises each data byte as 8 bits, followed by a parity bit
and 2 STOP bits, transmitted at a rate of 300 baud.  This interface
was not very reliable so at a later date (circa 1995) it was hacked to
drive an [RS-232](https://en.wikipedia.org/wiki/RS-232) interface, by
intercepting the 5V output of the UART before the tape cassette signal
modulation stage. As an interface to this, a serial data receiver
(`tridat.c`) and transmitter (`trimcc.c`) were written.  It is these
codes that are in the present repository.  They can be compiled by
issuing the command `make codes`.

The transmitter implements a rudimentary hybrid assembly /
machine code 'minilanguage' (detailed below) and can be used to
compile `.tri` source codes (below) to tape binaries which can be
loaded into the emulator described below, or to generate the ROMs
needed to run the emulator.

### Receiver (`tridat.c`)

Receive RS-232 data from a physically-connected Triton using a serial
device such `/dev/ttyS0`. Usage is
```
./tridat [-?|-h] [-o binary_file] serial_device
```
The command line options are:

- `-h` or `-?` (help) : print out the help;
- `-o <binary_file>` : write the byte stream in binary to a file.

Note that to use the serial device you may have to add yourself to the
`dialout` group.

### Transmitter (`trimcc.c`)

Compile and optionally transmit RS-232 data to Triton through a serial device.  Usage is
```
./trimcc [-?|-h] [-v] [-s] [-p] [-o binary_file] [-t serial_device] [src_file]
```
The command line options are:

- `-h` or `-?` (help) : print out the help;
- `-v` (verbose) : print the byte stream and variables;
- `-s` (spaced) : add a column of spaces after the 7th byte;
- `-p` (pipe) : write the byte stream in binary to `/dev/stdout` (obviates `-o`);
- `-o <binary_file>` : write the byte stream in binary to a file;
- `-t` (transmit) : write the byte stream to a serial device, for example `/dev/ttyS0`.

If the source file is not provided, the input is taken from `/dev/stdin`.

With the `-t` option this transmits bytes to a physically-connected
Triton through a serial device such `/dev/ttyS0`.  Alternatively with
the `-o` option the code can also be used to generate binaries to run
with the [emulator](EMULATOR.md).  If neither `-t` nor `-o` are
specified, the byte stream is silently dropped.

Using `-p` sends the binary to `/dev/stdout` so for example one can do
```
./trimcc <srcfile> -p | hexdump -C
```
The `-C` option prints the data out in the most useful format.

Note that to use the serial device with `-t` option you may have to
add yourself to the `dialout` group.

### Disassembler (`disasm8080.py`)

This is provided for convenience and is a simplified version of an 8080
disassembler written by Jeff Tranter, available on his [GitHub
site](https://github.com/jefftranter/8080).  The usage is
```
./disasm8080.py [-h] [-c] [-n] [-a address] [-s skip] [binary]
```
Command line options are:

- `-h` : print help and exit;
- `-c` : treat printable ASCII codes as characters;
- `-n` : make output suitable for `trimcc` (don't list instruction addresses or instruction bytes);
- `-a` : specify starting address, for example `-a 0x1602` for a Triton user code;
- `-s` : skip initial bytes, for example to discard the tape header in a tape binary;

If a binary file is not provided, the input is taken from `/dev/stdin`.

The `-a` and `-s` options can take decimal, or hexadecimal values if
preceeded by `0x`.

To figure out how many bytes to skip in order to discard the tape header at
the start of a tape binary, one can investigate the binary using
`hexdump` (a standard unix/linux utility).  For example, with
```
hexdump -C HEX2DEC_TAPE
```
one gets:
```
00000000  0d 0d 0d 0d 0d 0d 0d 0d  0d 0d 0d 0d 0d 0d 0d 0d  |................|
*
00000040  48 45 58 32 44 45 43 20  04 78 16 11 4e 16 cd 0b  |HEX2DEC .x..N...|
00000050  02 11 6e 16 cd 2b 00 cd  17 16 cd 33 00 c3 02 16  |..n..+.....3....|
etc
```
From this one can see the tape header ends with the byte sequence `20
04 78 16` where the last two bytes are the end address of the code in
little-endian format as required by the tape header.  Hence the actual
code from starts at `0x4b`.  To disassemble this therefore one can use:
```
./disasm8080.py -s 0x4b -a 0x1602 HEX2DEC_TAPE
```
The result is practically identical what one gets by loading
`HEX2DEC_TAPE` into the Triton emulator using the 'I' function and
disassembling it using TRAP.

It's possible to compile code with `trimcc` and pipe it into the
disassembler, for example with the core code in `fastvdu.tri`:
```
./trimcc fastvdu.tri -p | ./disasm8080.py
```
This is particularly simple because there is no tape header to avoid.  User
ROMs can likewise be disassembled.

Equally, it's possible to disassemble code with the disassembler, with
the `-n` option, and pipe it into `trimcc`, for example
```
./disasm8080.py -n -a 0x400 FASTVDU_ROM | ./trimcc -v -s
```

### TriMCC minilanguage

This is designed to be able to handle raw machine code, 8080 op-code
mnemonics, labels and cross references, ASCII characters, and ASCII
text.  A `.tri` file is an ASCII-encoded text file consisting of a
stream of tokens separated by white space characters, commas,
semicolons, and/or newlines.  Other files can be included by using an
`include <file>` directive, which is nestable to a certain level. For
example see `fastvdu_rom.tri` and `fastvdu_tape.tri`, which both
include `fastvdu.tri`.  This allows the same core code to be used to
make a tape binary and for making a user ROM.

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

Comments can be included at any point: they are start with a `#`
character and are terminated either by another `#` character, or by the
end of the line.

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
`GETADR=020B` then `!GETADR` would generate `0B` followed by `02`.
The low and high bytes of the word can be individually accessed by
appending `.L` and `.H` to the variable name.  Thus `!GETADR.L` would
generate `0B` and `!GETADR.H` would generate `02`.

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
address `1612` (based on the value of `ORG` and the number of bytes
exported thus far), and the code encounters `NOP>1620`, then the code
will emit 14 NOP op-codes (`00`) to fill to `161F` and the next byte
to be exported will be at address `1620`.  The same effect can be
achieved by having something like `FRAME=1620` earlier in the code,
and using `NOP>!FRAME`.  Finally, by specifying the value of `END` and
using `NOP>!END` one can pad the compiled code out to a specified
end address (see [`invaders_tape.tri`](invaders_tape.tri) for another
example).

Strings in Triton have to be explicitly terminated by the ASCII END OF
TRANSMISSION marker (`04` or ctrl-D) so that a typical string would
look like
```
STRING: "THIS IS A STRING" 04
```
This can be printed to the VDU by `LXI D !STRING; CALL 002B` where
`002B` is the L7.2 monitor entry point to print a string preceded by
CR/LF.

The TriMCC compiler was written in C over twenty years ago and is
certainly a bit clunky by modern standards.  An ongoing project is to
replace this by something more modern written in python.

#### Tape headers

The tape header format is incorporated as 64 ASCII carriage
return markers (`0D`, or ctrl-M), followed by the title (in ASCII),
followed by an ASCII space (`20`), and followed by the ASCII END OF
TRANSMISSION marker (`04`, or ctrl-D).  After this header, the rest of
the bytes are loaded into memory starting from address `1600`.  The
first two bytes at `1600` are conventionally used to indicate the end
point for tape storage, so that the usual entry point for the
executable part of the code is the address `1602`.

With the above the standard tape header (here for `FILENAME`) can be
generated by
```
64*0D "FILENAME" 20 04 ORG=1600 !END
```
followed by whatever code is required. The `!END` here generates the
end address as two bytes in a little-endian 16-bit word, as required
for the tape format; the actual code starts at the address `1602`.
Alternatively the `!END` can be omitted if the end adress is
hard-coded, as in [`invaders_tape.tri`](invaders_tape.tri) and
[`galaxian_tape.tri`](galaxian_tape.tri)

### Example TriMCC code

An example which illustrates the features of the TriMCC minilanguage
is provided in [`hex2dec.tri`](hex2dec.tri):
```
# Standard tape header #
64*0D "HEX2DEC" 20 04 ORG=1600 !END
 
# Entry points for Triton L7.2 Monitor #
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
provided 16-bit word, and incrementing the ASCII code `30` for the digit 
'0' to obtain the corresponding decimal digit (this relies on
the ASCII codes for the digits 0-9 being contiguous).

Compiling this with `./trimcc hex2dec.tri -v -s` results in the byte
stream (c.f. disassembled version above)
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

All `.tri` codes below can be compiled to tape binaries suitable for the
[emulator](EMULATOR.md) by, for example,
```
./trimcc example_tape.tri -o EXAMPLE_TAPE
```
To load this into the emulator use the `-t EXAMPLE_TAPE` command line
option, and then from within the emulator load the tape file using the
monitor 'I' command with the correct tape header name (listed below).
These tape binaries can also be concatenated into a single master tape
binary with `cat *_TAPE > TAPE` for instance.  This master tape can be
loaded into the emulator with the `-t` option,and the code you want to
load picked out by using the 'I' command with the appropriate tape
header.  Compilation of all these example and the creation of a master
tape is implemented as a `make tapes` target in the Makefile.  To run
these codes in Triton, use the monitor 'G' command, with the starting
address `1602`.

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

[`invaders_tape.tri`](invaders_tape.tri) (tape header `INVADERS`) -- a
[Space Invaders](https://en.wikipedia.org/wiki/Space_Invaders) clone.
Keys '1', '3' and 'SPACE' as above; 'G' to
start a new game.

The Space Invaders clone was modified from a hex dump in Computing Today
(March 1980). Currently if the complete fleet of invaders is wiped
out, another fleet doesn't appear - this seems to be a bug.  Apart
from this it's surprisingly good!

### Triton ROM dumps

ROM dumps for the Triton L7.2 monitor and BASIC, and TRAP (Triton
Resident Assembly Language Package), are also included.  These can be
compiled to binaries by
```
./trimcc mona72_rom.tri -o MONA72_ROM
./trimcc monb72_rom.tri -o MONB72_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
./trimcc trap_rom.tri -o TRAP_ROM
```
This is implemented as `make roms` in the Makefile, and the resulting
binary ROM files can be used directly with the emulator.

### Fast VDU 

In Level 7.2 monitor (at least) output of a character is vectored
through `1479`, so that by intercepting this one can fine-tune the
speed with which characters are written to the VDU.  This is the basis
for a FAST VDU user ROM which can be found on [Gerald Sommariva's web
site](https://sites.google.com/view/transam-triton/downloads).  Here
this functionality was re-implemented in `fastvdu.tri` which can be
used to generate both a user ROM and a tape binary,
```
./trimcc fastvdu_tape.tri -o FASTVDU_TAPE
./trimcc fastvdu_rom.tri -o FASTVDU_ROM
```
The user ROM can be loaded into the emulator using `-u FASTVDU_ROM`.  For
the tape binary (which is really only for testing purposes),
re-vectorisation of the VDU output is set up by running the code at
`1602` with the monitor 'G' command.

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

The Triton Level 7.2 Monitor, BASIC, and TRAP are all copyright &copy;
Transam Components Limited and/or T.C.L. software (1979) or thereabouts.

The 8080 disassembler `disasm8080.py` is released under an Apache
License, Version 2.0 and is copyright &copy; 2013-2015 by Jeff Tranter
<tranter@pobox.com>, with modifications copyright &copy; 2021
Patrick B Warren <patrickbwarren@gmail.com>.

The original copyright on the fast VDU code is unknown but it may
belong to Gerald Sommariva, from whose [web
site](https://sites.google.com/view/transam-triton/downloads) the
`FASTVDU.ED82` user ROM was downloaded which forms the basis of the
current code.  Modifications to this original code are copyright (c)
2021 Patrick B Warren <patrickbwarren@gmail.com> and are released into
the public domain.

The file [`invaders_tape.tri`](invaders_tape.tri) is modified from a
hex dump in Computing Today (March 1980; page 32).  Copyright &copy; is
claimed in the magazine (page 3) but the copyright holder is not
identified.  No license terms were given and the code is hereby
presumed to be reproducible and modifiable under the equivalent of a
modern open source license.  The changes made to this code are
copyright &copy; 2021 Patrick B Warren and are released into the public
domain.
