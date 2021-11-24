## SFML-based Triton emulator

This describes the fork of Robin Stuart's Triton emulator which is
available in a [GitHub repository](https://github.com/woo-j/triton).
Some features have been added and some small changes have been
made to reflect better the actual hardware.  The emulator is targetted
towards the Level 7.2 firmware (monitor and BASIC), and the Triton
Resident Assembly Language Package (TRAP).  Level 7.2 documentation
can be found on Robin's [GitHub
repository](https://github.com/woo-j/triton) and also in the 'ETI
Triton 8080 Vintage Computer' Facebook group.

The emulator can be compiled using the `make triton` target in the
Makefile, or `make codes`.  The [SFML library](https://www.sfml-dev.org/) is required.

### Usage
```
./triton [-h|-?] [-m mem_top] [-u user_rom(s)] [-z user_eprom] [tape_file]
```
The following command line options are available:

 - `-h` or `-?` prints a summary of command line options and function keys
 - `-m` sets the top of memory, for example `-m 0x4000`; the default is `0x2000`
 - `-u` installs one or two user ROM(s);
 - `-z` [EPROM programmer] specifies the file to write the EPROM to with function key F4

An optional binary tape file can be specified.  Several examples are
given in [TRIMCC.md](TRIMCC.md).

To install two user ROMS using the `-u` option, separate the filenames
by a comma with no spaces, for example `-u ROM1,ROM2`. For more
information see the section on user ROMs below.

When the emulator is running the following function keys can be used
to control the emulation:

 - F1: interrupt 1 (RST 1) - clear screen;
 - F2: interrupt 2 (RST 2) - save and dump registers;
 - F3: reset (RST 0);
 - F4: write the EPROM to the file specified by `-z`;
 - shift + F4: toggle simulation of EPROM `READ ERROR` failure mode;
 - ctrl + shift + F4: UV erase the EPROM (set all bytes to `0xff`);
 - F5: toggle emulator pause;
 - shift + F5: write 8080 status to command line;
 - ctrl + shift + F5: dump core;
 - F9: exit emulator.

All other keyboard input is sent to the emulation.  In a core dump
(ctrl + shift + F5) the entire contents of memory (64k) are written to
`core`.

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
until an EI instruction is encountered at some point in the monitor
code.  One can check this in the emulator by using shift + F5 to
write the 8080 status (the interrupt enabled/disbled flag status is
E/D).  One can clearly see the interrupt enabled flag is left unset
after one of these hardware interrupts, but becomes re-enabled for
example after a key press.  Rather than being a bug it appears that
this was a design choice.  The section of the Triton manual describing
the CPU ("The Heart of It") states "Interrupt 0 should not be used
even though it is available on the PCB. It simply duplicates the
manual reset operation but would create major problems if used as the
monitor program contains an EI instruction early on in this routine. A
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

The ROM dumps for the Triton L7.2 monitor and BASIC, and TRAP, can be
compiled to binaries using `trimcc` as described in [TRIMCC.md](TRIMCC.md),
```
./trimcc mona72_rom.tri -o MONA72_ROM
./trimcc monb72_rom.tri -o MONB72_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
./trimcc trap_rom.tri -o TRAP_ROM
```
(implemented as `make roms` in the Makefile).  If present, all these
files are loaded by the emulator.  For the L7.2 emulation to work
at least the two monitor ROMs should be present.

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
is loaded to `0800`-`0BFF`.

If the first byte in the first user ROM is the instruction LXI SP (op
code `31`), then Triton vectors here to execute the code automatically: see
the L7.2 documentation for more details, and
[`fastvdu_rom.tri`](fastvdu_rom.tri)) described in
[TRIMCC.md](TRIMCC.md) for a working example.

To just load a ROM in the second position, make up a blank ROM filled
with `FF` bytes using the EPROM programmer and load that into the
first position.  Alternatively one can make such a blank ROM with the
following unix (linux) command which generates 1024 zero bytes from
`/dev/zero` and pipes them through `sed` to change them to `FF` bytes:
```
dd if=/dev/zero bs=1024 count=1 | sed 's/\x00/\xff/g' > blank_rom
```
(obviously one can change `0xff` in this to another value if required).

#### Keyboard emulation

This was modified from Robin Stuart's emulator: the keyboard data byte
is strobed into port 3, with bit 7 (counting from 0) set the whole
time that the key is depressed, and unset when the key is released.
This reflects the behaviour of the real hardware.

#### Tape emulation

This remains as in Robin Stuart's emulator, except that the binary
tape file should be provided at the command line as indicated above.
Input bytes are read from the tape file as though from a cassette
recorder, and likewise output bytes are appended to the tape file.
This means that with the monitor 'I' command, the emulated tape
interface is expecting to see the correct tape header in front of any
data file as described in [TRIMCC.md](TRIMCC.md).  If no tape is
loaded then bytes written to the tape are lost and bytes read from the
tape return `FF`.

#### Printer emulation

This feature was added to Robin Stuart's emulator. The bit-banged
output to port 6 bit 7 (counting from 0) is captured and convert into
ASCII characters, which are then printed to `stdout`.  Since all other
messages are written to `stderr`, printer output can be captured by
redirecting `stdout` to a file, for example `./triton >
printer_output.txt`.

The relevant part of the monitor firmware that deals with printing
starts from `0104`:
```
0104  3A 01 14  LDA     1401    # load printer control byte from 1401
0107  FE 55     CPI     55      # check if printing is switched on
0109  C2 37 01  JNZ     0137    # if not, vector to VDU output at 0137
010C  F1        POP     PSW     # pop character to be printed in A off stack
010D  F5        PUSH    PSW     # and push it back again for VDU output routine
010E  F3        DI              # disable interrupts when printing
010F  C5        PUSH    B       # push BC onto stack for safe keeping
0110  2F        CMA             # complement character in accumulator
0111  4F        MOV     C,A     # and copy into C
0112  06 08     MVI     B,08    # countdown how many bits to bang out
0114  3E 80     MVI     A,80    # stick 0x80 into A for the start bit
0116  D3 06     OUT     06      #   bang out start bit / data bit
0118  CD B3 01  CALL    01B3    #   wait for prescribed time
011B  79        MOV     A,C     #   copy character from C into A
011C  0F        RRC             #   rotate right for the next data bit
011D  4F        MOV     C,A     #   and copy back to C
011E  05        DCR     B       #   decrement the bit bang counter
011F  F2 16 01  JP      0116    # if not done, loop back to bang out next bit
0122  06 02     MVI     B,02    # set the bit counter to 0x02
0124  FE 79     CPI     79      # check for CR (see note below)
0126  C2 2B 01  JNZ     012B    # if not, use the current bit counter
0129  06 20     MVI     B,20    # otherwise set the bit counter to 0x20
012B  AF        XRA     A       # clear the accumulator to 0x00
012C  D3 06     OUT     06      # bang out '0' = stop bit(s)
012E  CD B3 01  CALL    01B3    #   wait for prescribed time
0131  05        DCR     B       #   decrement stop bit counter
0132  F2 2E 01  JP      012E    # if not done, loop back to wait some more
0135  FB        EI              # enable interrupts again
0136  C1        POP     B       # recover BC from safe keeping
```
The standard VDU output follows from this point at `0137`, starting
with a `POP PSW` to recover the character to be printed into A.

The above main code is complemented by a timer delay routine that sets
the baud rate (A is lost):
```
01B3  E5        PUSH    H       # stash HL for safe keeping
01B4  2A 02 14  LHLD    1402    # fetch the delay value into HL from 1402
01B7  2B        DCX     H       #   decrement HL
01B8  7D        MOV     A,L     #   move low byte into A
01B9  B4        ORA     H       #   logical OR with high byte
01BA  C2 B7 01  JNZ     01B7    # if either is non-zero, loop back
01BD  E1        POP     H       # recover HL from safe keeping
01BE  C9        RET             # return - end of delay routine
```
A close examination now shows just what is happening with the serial
printer output.  There is a start bit (port 6 bit 7 high), followed by
seven (7) data bits containing the 7-bit ASCII character code, a fake
parity bit (port 6 bit 7 high again), then two stop bits (port 6 bit 7
low).  For carriage return the output is left low for an _additional_
delay equivalent to 30 bits (the bit counter is set to `0x20` = 32
decimal, rather than `0x02`).  The stop bits are handled in software
by increasing the delay rather than writing successive `00` bytes to
port 6.  Also port 6 is initialised to `00` during startup (monitor
code at `007D` not shown).

The test for carriage return is made at `0124` in the main routine
where the accumulator is compared to `0x79`.  This is what the
character code `0x0d` for CR turns into after the CMA (complement
accumulator) step and subsequent bit shifting operations.

An example output is captured below in the emulator, where the right
hand column is a trace of the actual output to port 6 bit 7 and the
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
As described in the manual, and as seen in the code above, the timer
delay that governs the baud rate is controlled by the two bytes at
`1402` and `1403`.  For the fastest print speed set these to `0x01` and
`0x00` respectively.

#### EPROM programmer emulation

This feature was also added to Robin Stuart's emulator and has been
tested to work with the 'Z' command in the Level 7.2 monitor.
The target binary file should be specified at the command line with
`-z` option.  The file is loaded if it exists, otherwise a blank EPROM
is created with all bits set to 1.  Note that in programming a 2708,
bits can only be set to '0', not set to '1'.  This is implemented
faithfully in the emulator.

Being an emulation, the situation where bits which should be
programmed to be '0' but remain '1' normally does not arise however
this failure mode resulting in a `READ ERROR` can be simulated using
shift + F4.  Conversely, since it is possible to load an _existing_
EPROM with arbitary bit pattern, the failure mode where bits which
should be programmed to be '1' but are actually '0' can be more easily
created and results in a `PROGRAM ERROR`.

The following function keys are available to simulate the physical hardware:

- F4 causes the content of the EPROM (in whatever state it is
  in) to be written to the file specified by `-z` at the command line;

- shift + F4 toggles a `READ ERROR` failure mode simulation in which
  the emulator does not overwrite the existing data in the EPROM and the
  bit pattern is unchanged after a programming pulse.

- ctrl + shift + F4 performs the equivalent to a UV erase by setting all
  bits to 1 (not necessary for a blank EPROM);

When programming the EPROM the number of write cycles per memory
location is monitored to act as a check on the firmware.  When saving
the EPROM to a file (function F4) a warning is printed at the command
line if the write cycle count for any memory location is less than
100, though in the emulation only one write cycle is needed per memory
location to store the data.  These write cycle counts are initialised
to zero at the start, and reinitialised by the emulated UV erase step.
If the `READ ERROR` simulation mode is turned on (shift + F4), the
cycle count is still incremented but the emulator does not overwrite
the existing data in the EPROM.

In terms of physical hardware the EPROM programmer consists of an
[Intel 8255](https://en.wikipedia.org/wiki/Intel_8255) programmable
peripheral interface (PPI) chip, directly interfaced to a [2708
EPROM](https://en.wikipedia.org/wiki/EPROM) which provides 1k of
memory configured as 1024 addresses (10-bits) of an 8-bit wide data
bus.  Some auxiliary logic and discrete electronics implements a 20 V
programming pulse of 1 ms duration, per write cycle (this 1 ms
hardware delay was omitted in the emulation).  The details of the
EPROM programmer emulation are a little complicated although only the
bare minimum functionality of the [Intel
8255](https://en.wikipedia.org/wiki/Intel_8255) has been emulated.
For completeness these details are given here.  The original
description of the hardware and how it functions features in
[Electronics Today
International](https://en.wikipedia.org/wiki/Electronics_Today_International)
(ETI) Jan 1980 p42-45, and the relevant issue can be found
[here](https://worldradiohistory.com/ETI_Magazine.htm) (the article
has also been uploaded as a PDF to the Facebook group).

In the programmer the three ports (A, B, C) available to the 8255 are
operated in a simple I/O mode (mode 0) with the directionality
governed by bits set in a control word.  The ports are mapped to
Triton I/O ports as follows:
```
Triton port FC = 8255 port A (either as an input or as an output port)
Triton port FD = 8255 port B (configured as an output port)
Triton port FE = 8255 port C (upper 4 bits configured for input; lower 4 bits for output)
Triton port FF = 8255 control word (output only)
```
To correspond to the use-pattern of the firmware only two control
words have been implemented in the emulator:
```
control word     port A   port B   port C
10001000 (0x88)   OUT      OUT    IN / OUT
10011000 (0x98)   IN       OUT    IN / OUT
```
Thus, bit 4 of the control word controls the direction of port A
(Triton port `FC`).

The three ports A, B, and C, thus configured, are used as follows:
```
port A (Triton FC) - 2708 data (input / output)
port B (Triton FD) - lowest 8 bits of 2708 address (output)
port C (Triton FE) - bits 0, 1 (output) = high bits of 2708 address
                     bits 2, 3 (output) = 2708 chip control (see below)
                     bit 7 (input) = 2708 programming cycle status
```
Reading and writing data to the 2708 is controlled by bits 2
and 3 of port C (Triton port `FE`):
```
bit 3 bit 2 (port C = Triton port FE)         logic test
  0    1     2708 chip select enabled         port C & 0x0C == 0x04
  1    0     initiate 2708 programming pulse  port C & 0x0C == 0x08
```
The logic test is exactly as implemented in the emulator.  In summary:

- to _fetch_ data from the EPROM, write `0x98` to Triton port `FF`
  (control word), write the least significant 8 bits of the
  required address to Triton port `FD` (port B), logical OR the
  most significant two bits of the address with `0x04` (chip select
  enabled) and write these 4 bits to Triton port `FE` (port C),
  then read the data from Triton port `FC` (port A);

- to _program_ the EPROM, write `0x88` to Triton port `FF` (control
  word), write the data to Triton port `FC` (port A), write the least
  significant 8 bits of the required address to Triton port `FD` (port
  B), logical OR the most significant two bits of the address with
  `0x08`, and write these 4 bits to Triton port `FE` (port C) to
  initiate programming pulse;

- to _test_ whether the programming pulse is completed, read the top 4
  bits of Triton port `FE` (port C); the programming pulse is
  completed if bit 7 is set to '0'.

In the emulator, as mentioned, the 1 ms is not emulated so that after
initiating the programming pulse the bit 7 of port C is _immediately_
set to 0.

For completeness, the L7.2 monitor code that implements the 'Z'
function is described next.  The entry point from the function prompt
is at address `0F1C`:
```
0F1C  CD 08 02  CALL    0208    # prompt for start address, return in HL
0F1F  0E 64     MVI     C,64    # number of write cycles is 0x64 = 100 decimal
0F21  11 00 04  LXI     D,0400  # load DE with 0x0400 = 1024 bytes (EPROM capacity)
0F24  E5        PUSH    H       #   push HL (start address) onto stack
0F25  D5        PUSH    D       #   push DE = 0x400 onto stack
0F26  11 00 00  LXI     D,0000  #   load DE with start address of EPROM = 0x0000
0F29  CD 5F 0F  CALL    0F5F    #     call to read byte from EPROM (DE) into A
0F2C  47        MOV     B,A     #     copy A into B
0F2D  B6        ORA     M       #     or A with memory at HL
0F2E  B8        CMP     B       #     test for zero bits; ie does A | M == A ?
0F2F  C2 72 0F  JNZ     0F72    #     if so, print 'PROGRAM ERROR' and abort to function prompt
0F32  3E 88     MVI     A,88    #     set up for write cycle - control word to 0x88
0F34  06 08     MVI     B,08    #     bits 2 and 3 of port C will be 1 and 0 respectively
0F36  CD 63 0F  CALL    0F63    #     call to write byte from memory (HL) into EPROM (DE)
0F39  DB FE     IN      FE      #       read upper 4 bits from 8255 port C
0F3B  A7        ANA     A       #       set sign flag = bit 7 of A (program pulse)
0F3C  FA 39 0F  JM      0F39    #     loop back if sign flag set (program pulse not finished)
0F3F  79        MOV     A,C     #     copy number of remaining write cycles in C into A
0F40  FE 01     CPI     01      #     are we at the final write cycle?
0F42  C2 4C 0F  JNZ     0F4C    #     if not, skip the read test
0F45  CD 5F 0F  CALL    0F5F    #       otherwise, call to read byte from EPROM (DE) into A
0F48  BE        CMP     M       #       does it match what's in memory (HL)?
0F49  C2 7B 0E  JNZ     0E7B    #       if not, print 'READ ERROR' and abort to function prompt
0F4C  23        INX     H       #     increment HL (memory address)
0F4D  13        INX     D       #     increment DE (EPROM address)
0F4E  E3        XTHL            #     exchange stack with HL; HL now contains 0x400
0F4F  CD BF 00  CALL    00BF    #     compare HL with DE, ie does DE = 0x400?
0F52  E3        XTHL            #     exchange stack with HL, recovering memory address
0F53  C2 29 0F  JNZ     0F29    #   if DE is not yet 0x400, loop back for next byte
0F56  D1        POP     D       #   pop DE off stack (the value here is 0x400)
0F57  E1        POP     H       #   pop HL off stack (the start address again)
0F58  0D        DCR     C       #   decrement write cycle count
0F59  C2 24 0F  JNZ     0F24    # if not zero, loop back for another write cycle
0F5C  C3 3D 03  JMP     033D    # print 'END' and return to function prompt
```
To accompany this is a short subroutine with entry points at `0F5F` and `0F63`:
```
0F5F  3E 98     MVI     A,98    # 8255 control word will be set to 0x98
0F61  06 04     MVI     B,04    # bits 2 and 3 of port C will be 0 and 1 respectively
0F63  D3 FF     OUT     FF      # write control word to 8255 (second entry point)
0F65  7B        MOV     A,E     # copy low byte of EPROM address in DE
0F66  D3 FD     OUT     FD      # write to 8255 port B
0F68  7E        MOV     A,M     # get data from main memory location in HL
0F69  D3 FC     OUT     FC      # write to 8255 port A (no effect unless control word is 0x88)
0F6B  7A        MOV     A,D     # copy high bits of EPROM address from DE (only lowest two bits are used)
0F6C  B0        ORA     B       # set bits 2 and 3 for the 2708 chip select or program pulse
0F6D  D3 FE     OUT     FE      # write to 8255 port C (affects lower 4 bits only)
0F6F  DB FC     IN      FC      # read from 8255 port A (data acquired only if control word is 0x98)
0F71  C9        RET             # return
```
Examining this, first note that the subroutine at `0F5F` doubles up to
perform _two_ functions depending on the entry point.

If called at `0F5F` the subroutine _reads_ data from the EPROM: it
sets the 8255 control word to `0x98` so that the 8255 port A direction
is IN, and sets bits 2 and 3 of the byte to be written to port C to
'1' and '0' respectively to enable the chip select for the EPROM.  The
instructions at `0F68` and `0F69` which pull data from memory and
write to port A are superfluous (in the emulator, the data is
discarded), but reading data from port A at `0F6F` recovers the byte
stored in the desired EPROM memory location into the accumulator.

Conversely, when the main code at `0F36` calls entry point at `0F63`
the subroutine _writes_ data to the EPROM: in the main code the
control word is preset to `0x99` so that the 8255 port A direction is
OUT, and bits 2 and 3 of the byte to be output to port C are set to
'0' and '1' respectively to trigger a programming pulse.  The byte to
be written is pulled from main memory and written to port A just
before programming pulse is triggered (ie the port C write at `0F6D`).
At this point the step at `0F6F` where data is read from port A into
the accumulator returns garbage (in the emulator, this is represented
by `0xff`).  This returned value is unused however (discarded) in the
main code.

This subroutine is called _three_ times in the main code,
with the entry point at `0F5F` being used twice to read data from the
EPROM, and the entry point at `0F63` being used once to write data to
the EPROM.  Thus to save space it makes sense to have the instructions
which set up a read operation in the subroutine, and to keep the
instructions which set up a write operation in the main code.  Also it
makes sense to combine the common elements of the read and write
operations in a subroutine like this, despite the small inefficiency
in the number of port operations.  It is presumed this was done to
save space.

Returning now to the main code, each of the 1024 data bytes in memory
is written to the EPROM sequentially in an inner loop, starting from the
desired memory location, and this is repeated 100 times in an outer
loop.  This is to ensure the total duration of the programming step
for each memory location in the 2708 is 100 ms, as described in the
EPROM programmer documentation.  The register pair DE keeps track of
the memory location in the EPROM, the register pair HL keeps track of
the position in main memory, and register C is initialised to `0x64`
(decimal 100) to count down the number of write cycles.

A `PROGRAM ERROR` occurs if a bit in the 2708 is '0' when it should be
written as '1'. This test is made every time a byte is written to the
EPROM and is implemented concisely at `0F2C` to `0F2E`.  The way it
works can be explained by looking at a truth table for a single bit.
```
memory bit (M)  1  1  0  0
EPROM bit (A)   1  0  1  0
A | M           1  1  1  0
A | M == A ?    t  f  t  t
```
Thus we see `A | M == A` returns false only if the bit in memory is
'1' and the corresponding bit in the EPROM is '0'.  This would
correspond to an improperly erased bit in the EPROM. When applied to
the A and M registers, this tests all 8 bits in parallel, and if the
result of the final comparison is non-zero then at least one of these
bits failed (in the 8080, the truth of a comparison test is
represented by '0' for true and '1' for false).  This results in a
`PROGRAM ERROR`.  The situation can be simulated by attempting to
write to an EPROM with all bits zet to '0'. This can be created either
using 'Z' in the Triton emulation writing from `0x1600` having filled all
the memory locations with `0x00`, or outside the emulation using a
command line incantation similar to that mentioned already:
```
dd if=/dev/zero bs=1024 count=1 > blank_rom_all_zeros
./triton -z blank_rom_all_zeros
```

The test for whether the programming step is complete is performed by
reading the upper 4 bits from port C into the accumulator at `0F39`,
and using the `ANA A` instruction at `0F3B`.  This operation doesn't
change the contents of the accumulator but it _does_ set the flags, in
particular the sign flag now matches the high bit in the accumulator.
Thus this affords a cheap (and space-saving) way to test for whether
the high bit in the accumulator is set or reset, signalling the
completion of the programming step.  Note the tight coupling between
the firmware and hardware design at this point: any of the top 4 bits
of C _could_ have been used for this signal, but using bit 7
simplifies the test and saves space in the code.

A `READ ERROR` occurs if the final byte read does not match that in
memory.  This test at `0F3F` and `0F40` is performed _after_ the
last write cycle (even though register C has only counted down to
`0x01`, the 100th write step has been done). This catches errors where
a bit in the 2708 is '1' and it should have been written to be '0'.
In the emulator this failure mode can be simulated by toggling the
`READ ERROR` failure mode simulation (shift + F4).

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

The Triton Level 7.2 Monitor from which the above EPROM programmer
code was extracted is copyright &copy; Transam Components Limited and/or
T.C.L. software (1979) or thereabouts.

The original copyright on the fast VDU code is unknown but it may
belong to Gerald Sommariva, from whose [web
site](https://sites.google.com/view/transam-triton/downloads) the
`FASTVDU.ED82` user ROM was downloaded which forms the basis of the
current code.  Modifications to this original code are copyright (c)
2021 Patrick B Warren <patrickbwarren@gmail.com> and are released into
the public domain.
