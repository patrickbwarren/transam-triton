## SFML-based Triton emulator

This describes the fork of Robin Stuart's Triton emulator which is
available in a [GitHub repository](https://github.com/woo-j/triton).
Some features have been added and few small improvements have been
made to reflect better the actual hardware.  The emulator is targetted
towards the Level 7.2 firmware (Monitor and BASIC), and the Triton
Resident Assembly Language Package (TRAP).  Level 7.2 documentation
can be found on Robin's [GitHub
repository](https://github.com/woo-j/triton) and also in the 'ETI
Triton 8080 Vintage Computer' Facebook group.

The emulator can be compiled using the `make triton` target in the
Makefile, or `make codes`.  The [SFML library](https://www.sfml-dev.org/) is required.

### Usage
```
./triton -m <mem_top> -t <tape_file> -u <user_rom(s)> -z <user_eprom> -h -?
```
The following command line options are available:
 - `-h` or `-?` prints a summary of command line options and function keys
 - `-m` sets the top of memory, for example `-m 0x4000`; the default is `0x2000`
 - `-t` specifies a tape binary, for example `-t TAPE`
 - `-u` installs one or two user ROM(s);
 - `-z` EPROM programmer: specifies the file to write the EPROM to with function key F8
 
To install two user ROMS using the `-u` option, separate the filenames
by a comma with no spaces, for example `-u ROM1,ROM2`.  ROMs are
always installed at `0400` first followed by `0800`.  To override
this one could make a blank ROM filled with `FF` bytes using the
EPROM programmer.  Alternatively one can use the following unix (linux) command
which generates 1024 zero bytes from `/dev/zero` and pipes them through
`sed` to change them to `FF` bytes:
```
dd if=/dev/zero bs=1024 count=1 | sed 's/\x00/\xff/g' > binfile
```
(obviously one can change `0xff` in this to another value if required).

When the emulator is running the following function keys can be used
to control the emulation:
 - F1: interrupt 1 (RST 1) - clear screen
 - F2: interrupt 2 (RST 2) - save and dump registers
 - F3: reset (RST 0)
 - F4: halt system (jam HLT instruction using interrupt)
 - F5: toggle emulator pause
 - F6: write 8080 status to command line
 - F7: EPROM programmer: UV erase the EPROM (set all bytes to 0xff)
 - F8: EPROM programmer: write the EPROM to the file specified by `-z`
 - F9: exit emulator

All other keyboard input is sent to the emulation.

### Implementation notes

#### Interrupts

First of all, because in Triton the interrupt system is so closely
tied to the RST instructions, it appears that they are synonymous but
they are not.  The following notes extracted from an email to Ian
Lockhart describe the situation in some detail.

The RST instructions are simply one-byte op-codes which execute a
subroutine call to the appropriate hard-coded place in memory, that's
to say they push the program counter on stack and jump to the required
location (`0008` for RST 1, `0010` for RST 2, etc).  They have nothing
to do with the interrupt system on the 8080, although they are
designed to work in tandem with it.  In particular, executing an RST
instruction in software does not change the interrupt enabled flag,
and equally the execution of an RST instruction is not affected by the
interrupt enabled flag.

There is only one interrupt line on the 8080 itself.  If activated,
and if the interrupt enabled flag is set, the 8080 fetches an op-code
off the databus and executes it, and also disables the interrupt
enabled flag to disable further interrupts.  This obviously makes good
sense since one would not necessarily want to enable interrupts whilst
already servicing one (also likely for the same reason interrupts are
disabled in Triton on startup or at a hardware reset -- see below).
The interrupt enabled flag can be set or unset in software by an EI
(`FB`) or DI (`F3`) instruction.

The RST instructions are designed with this interrupt system in mind,
and in a hardware interrupt the fetched op-code is usually an RST
instruction though there are examples on the internet where it is not.
Anyway, in Triton, the logic external to the 8080 (IC 11; described in the
manual under the CPU section "the heart of it") creates eight user
interrupt lines which have the effect of triggering an interrupt on
the 8080 and jamming the corresponding RST op-code onto the databus.

The upshot of all this is that after a hardware interrupt 1 (clear
screen) or interrupt 2 (print registers + flags and escape to the
function prompt), further interrupts are disabled.  They stay disabled
until an EI instruction is encountered at some point in the Monitor
code.  One can check this in the emulator by using function F6 to
write the 8080 status (the interrupt enabled/disbled flag status is
E/D).  One can clearly see the interrupt enabled flag is left unset
after one of these hardware interrupts, but becomes re-enabled for
example after a key press.  Rather than being a bug it appears that
this was a design choice.  The section of the Triton manual describing
the CPU ("The Heart of It") states "Interrupt 0 should not be used
even though it is available on the PCB. It simply duplicates the
manual reset operation but would create major problems if used as the
Monitor program contains an EI instruction early on in this routine. A
very rapid build up of interrupt nests would occur which would fill up
the stack in a fraction of a second."  Presumably the same could be
true if EI was executed too soon after any interrupt was serviced.

There is one peculiar thing though.  In the 8080A bugbook, there is
the statement that the microprocessor can still detect whether or not
an interrupt occurs even if the interrupt flag is disabled.  Further:
"If an interrupt did occur whilst the interrupt flag was disabled,
then an interrupt would occur as soon as the flag is enabled. Only one
interrupt event is remembered."  Tests with actual hardware appear to
indicate this is the case, for example one can restart the machine,
press interrupt 1 (nothing happens because the interrrupt enabled flag
is unset), then press a key on the keyboard (for example 'W') and the
pending interrupt is serviced (clear screen in this case).

This logic is implemented in the emulator.

In addition, a HLT instruction can be jammed onto the interrupt line
using function F4 (which also enables interrupts).  This has the
result that the 8080 enters the halted state from which one can only
recover with a hardware reset (function F3).  The contents of RAM is
preserved though.  The same thing happens if the emulator encounters a
file error during processing of tape input or output.

#### Parity

In the 8080 the parity bit is set if there is an even number of bits set
in the result.  So, it is an odd parity bit by the definition in
[Wikipedia](https://en.wikipedia.org/wiki/Parity_bit).  For Triton one
should have, for example
```
result = 0x00 --> parity = T
result = 0x01 --> parity = F
result = 0x02 --> parity = F
result = 0x03 --> parity = T
result = 0x04 --> parity = F
result = 0x05 --> parity = T
```

Some 8080 emulators get this the wrong way around (it is however
correct in Robin's emulator).  In a Triton Level 7.2 emulation this
leads to an extremely obscure bug since the problem only shows up when
trying to enter BASIC with the 'J' instruction: if the parity
calculator is implemented the wrong way around, the emulation hangs at
this point, but otherwise everything else appears to function
normally.

The 8-bit parity calculator in the current emulator is based on a
[Stack Overflow implementation](https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd/21618038)
```
byte ^= byte >> 4;
byte ^= byte >> 2;
byte ^= byte >> 1;
parity = (byte & 0x01) ? false : true;
```  
One can test the effect of getting this the wrong way around by swapping the `true` and `false` in this.

#### System ROMs

The ROM dumps for the Triton L7.2 Monitor and BASIC, and TRAP, can be
compiled to binaries using `trimcc` as described in [TRIMCC.md](TRIMCC.md),
```
./trimcc mona72_rom.tri -o MONA72_ROM
./trimcc monb72_rom.tri -o MONB72_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
./trimcc trap_rom.tri -o TRAP_ROM
```
(implemented as `make roms` in the Makefile).  If present, all these
files are loaded by the emulator.  For the L7.2 emulation to work
at least the two Monitor ROMs should be present.

The memory map for Level 7.2 Triton software is as follows
```
E000 - FFFF = BASIC72_ROM (BASIC)
C000 - DFFF = TRAP_ROM (TRAP)
2000 - BFFF = For off-board expansion
1600 - 1FFF = On board user RAM
1400 - 15FF = Monitor/BASIC RAM
1000 - 13FF = VDU
0C00 - 0FFF = MONB72_ROM (Monitor 'B')
0400 - 0BFF = User ROMs
0000 - 03FF = MONA72_ROM (Monitor 'A')
```

#### User ROMs

These can be loaded using the `-u` option at the command line.  If
just one file is specified it is loaded to `0400`-`07FF`.  If two
files are specified, for example `-u ROM1,ROM2`, then the second one
is loaded to `0800`-`0BFF`.  An example of a user ROM is the fast VDU
ROM described in [TRIMCC.md](TRIMCC.md). If the first byte in the
first user ROM is LXI SP (op code `31`), then the code is executed
automatically.  See the L7.2 documentation for more details and the
fast VDU user ROM (specifically [`fastvdu_rom.tri`](fastvdu_rom.tri))
for a working example.

Fast VDU user ROM more here ...

#### Keyboard emulation

This was fixed compared to Robin Stuart's emulator: the keyboard data
byte is strobed into port 3, with bit 8 set the whole time that the
key is depressed, and unset when the key is released.  This reflects
the behaviour of the real hardware.

#### Tape emulation

This remains as in Robin Stuart's emulator, except that the
possibility to select the tape file is given as a command line option.
Input bytes are read from the tape file as though from a cassette
recorder, and likewise output bytes are appended to the tape file.
This means that with the Monitor 'I' option, the emulated tape
interface is expecting to see the correct tape header in front of any
data file as described in [TRIMCC.md](TRIMCC.md).

#### Printer emulation

This feature was added to Robin Stuart's emulator. The bit-banged
output to port 6 bit 8 is captured and convert into ASCII characters,
which are then printed to `stdout`.  Since all other messages are
written to `stderr`, printer output can be captured by redirecting
`stdout` to a file, for example `./triton > printer_output.txt`.

Close examination of the Monitor code (`0104`-`0154`) shows just
what is happening with the serial printer output, which turns out to be
not quite as stated as in the documentation.  There is a start bit
(port 6 bit 8 high), followed by seven (7) data bits containing the
7-bit ASCII character code, a fake parity bit (port 6 bit 8 high
again), then two stop bits (port 6 bit 8 low).  For carriage return
the output is left low for an additional delay equivalent to 30 bits.
The two stop bits are handled in software by doubling the delay since
there is no need to write out two successive `00` bytes to port 6
(this has the potential to fool an emulation which is tracking port
writes!).  Also port 6 is initialised to `00` during startup (Monitor
code at `007D`).

An example output is captured below in the emulator, where the right
hand column is a trace of the actual output to port 6 bit 8 and the
other columns are the ASCII character, the ASCII character code in
hex, and the ASCII character code in binary.  In the right hand
bit-banged column you can see the initial start bit (high), followed
by the seven data bits, then a fake parity bit (high), then the
singly-written stop bit (low) at the end, making ten bits output in
total for each character.  To marry this up with the 7-bit ASCII code
being transmitted, note that the data is transmitted least significant
bit first, and is inverted.  Also, the fake parity bit could equally
be counted as the high order bit in an 8-bit extended ASCII code with
no parity.
```
F = 46 = 0100 0110 <-- 1100111010
U = 55 = 0101 0101 <-- 1010101010
N = 4E = 0100 1110 <-- 1100011010
C = 43 = 0100 0011 <-- 1001111010
T = 54 = 0101 0100 <-- 1110101010
I = 49 = 0100 1001 <-- 1011011010
O = 4F = 0100 1111 <-- 1000011010
N = 4E = 0100 1110 <-- 1100011010
? = 3F = 0011 1111 <-- 1000000110
```
The timer delay that governs the baud rate is controlled by `1402`/`3`
in user RAM as described in the manual.  For the fastest print speed
set these two bytes to `01` and `00` respectively.

#### EPROM programmer emulation

This feature was also added to Robin Stuart's emulator and has been
tested to work with the 'Z' function command in the Level 7.2 Monitor.
The target binary file should be specified at the command line with
`-z` option.  If the file exists it is loaded, otherwise a blank EPROM
is created with all bytes set to `FF`.  To facilitate the use of the
programmer, function F7 performs the equivalent to a UV erase by
setting all the bytes to `FF` (not necessary for a blank EPROM), and
function F8 causes the content of the EPROM to be written to the file
specified by `-z` at the command line.

When writing to the EPROM the number of write cycles per memory
location is monitored to act as a check on the firmware.  When saving
the EPROM to a file a warning is printed the write cycle count for any
memory location is less than 100, though in the emulation only one
write cycle is needed per memory location to store the data.  These
write cycle counts are initialised to zero at the start, and
reinitialised by the emulated UV erase step).

The details of the EPROM programmer emulation are a little complicated
    although only the bare minimum functionality of the Intel 8255
programmable peripheral interface (PPI) chip is emulated to meet the
needs of the EPROM programmer.  For completeness these details are
given here.

The EPROM programmer hardware consists of the above mentioned Intel
8255 programmable peripheral interface chip, directly interfaced to
the 2708 EPROM.  Some auxiliary logic and discrete electronics
implements a 20 V programming pulse of 1 ms duration, per write cycle
(this delay was not included in the emulation).  In the programmer the
ports available to the 8255 are operated in one of only two modes, and
only these modes need to be emulated.

More here...

The Monitor code that implements the Z function is tightly written and
takes some shortcuts.

The entry point from L7.2 monitor is at address `0F1C` and the code
is as follows:
```
0F1C  CD 08 02  CALL    0208    ; prompt for start address, return in HL 
0F1F  0E 64     MVI     C,64    ; number of write cycles is 0x64 = 100 decimal
0F21  11 00 04  LXI     D,0400  ; load DE with 0x0400 = 1024 bytes (EPROM capacity)
0F24  E5        PUSH    H       ; push HL (start address) onto stack
0F25  D5        PUSH    D       ; push DE = 0x400 onto stack
0F26  11 00 00  LXI     D,0000  ; load DE with start address of EPROM = 0x0000
0F29  CD 5F 0F  CALL    0F5F    ; call to read byte from EPROM into A
0F2C  47        MOV     B,A     ; copy A into B
0F2D  B6        ORA     M       ; or A with memory at HL
0F2E  B8        CMP     B       ; test for zero bits; ie does A | M == A ?
0F2F  C2 72 0F  JNZ     0F72    ; if so, print 'PROGRAM ERROR' and abort to function prompt
0F32  3E 88     MVI     A,88    ; set up for write cycle - control word to 0x88
0F34  06 08     MVI     B,08    ; bits 2 and 3 of port C will be 1 and 0 respectively 
0F36  CD 63 0F  CALL    0F63    ; call to write byte in A to EPROM
0F39  DB FE     IN      FE      ; fetch upper 4 bits from 8255 port C
0F3B  A7        ANA     A       ; set sign flag = bit 7 of A (write pulse)
0F3C  FA 39 0F  JM      0F39    ; loop back if sign flag set (write pulse not finished)
0F3F  79        MOV     A,C     ; copy number of remaining write cycles in C into A
0F40  FE 01     CPI     01      ; are we at the final write cycle?
0F42  C2 4C 0F  JNZ     0F4C    ; if not, skip the read test
0F45  CD 5F 0F  CALL    0F5F    ; otherwise, call to read byte from EPROM into A
0F48  BE        CMP     M       ; does it match what's in memory?
0F49  C2 7B 0E  JNZ     0E7B    ; if not, print 'READ ERROR' and abort to function prompt
0F4C  23        INX     H       ; increment HL (memory address)
0F4D  13        INX     D       ; increment DE (EPROM address)
0F4E  E3        XTHL            ; exchange stack with HL; HL now contains 0x400
0F4F  CD BF 00  CALL    00BF    ; compare HL with DE, ie does DE = 0x400?
0F52  E3        XTHL            ; exchange stack with HL, recovering memory address
0F53  C2 29 0F  JNZ     0F29    ; if DE is not yet 0x400, loop back for next byte
0F56  D1        POP     D       ; pop DE off stack (the value here is 0x400)
0F57  E1        POP     H       ; pop HL off stack (the start address again)
0F58  0D        DCR     C       ; decrement write cycle count
0F59  C2 24 0F  JNZ     0F24    ; if not zero, loop back for another cycle
0F5C  C3 3D 03  JMP     033D    ; print 'END' and return to function prompt
```
To accompany this is a short subroutine with two entry points. The entry point at
`0F5F` sets the 8255 control word to `98` so that the 8255 port A direction is
IN; and the entry point at `0F63` (called from `0F36`) should have the control word set
to `99` so that the 8255 port A direction is OUT.  In both cases bits 2 and 3 of
the lower half of port C are also set appropriate to a read or write cycle.
```
0F5F  3E 98     MVI     A,98    ; 8255 control word will be set to 0x98
0F61  06 04     MVI     B,04    ; bits 2 and 3 of port C will be 0 and 1 respectively
0F63  D3 FF     OUT     FF      ; output control word to 8255 (second entry point)
0F65  7B        MOV     A,E     ; copy low byte of EPROM address in DE
0F66  D3 FD     OUT     FD      ; output to 8255 port B
0F68  7E        MOV     A,M     ; get data from main memory location in HL
0F69  D3 FC     OUT     FC      ; output to 8255 port A (no effect unless control word is 0x88)
0F6B  7A        MOV     A,D     ; copy high byte of EPROM address in DE (only lowest two bits are used)
0F6C  B0        ORA     B       ; set bits 2 and 3 to control 2708
0F6D  D3 FE     OUT     FE      ; output to 8255 port C (affects lower 4 bits only)
0F6F  DB FC     IN      FC      ; input from 8255 port A (data acquired only if control word is 0x98)
0F71  C9        RET             ; return
```

More here...

Test for zero bytes implements `A | M == A` - explanation.

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

Unless otherwise stated, copyright &copy; 2021 Patrick B Warren
<patrickbwarren@gmail.com>

The original SFML-based emulator is copyright &copy; 2020 Robin Stuart
<rstuart114@gmail.com>
