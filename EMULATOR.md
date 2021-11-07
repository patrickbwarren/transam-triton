## Triton emulator

Robin Stuart has written a superb [Triton
emulator](https://github.com/woo-j/triton) that uses the [SFML
library](https://www.sfml-dev.org/).  Some of the original Triton
documentation can also be found in Robin's repository.  A fork of this
emulator is included in the present repository, to which a few small
improvements have been made.

### Compilation

Is provided by a target `make triton` in the Makefile, or `make codes`.

### Usage
```
Triton emulator
./triton -m <mem_top> -t <tape_file> -u <user_roms> -z <user_eprom>
-m sets the top of memory, for example -m 0x4000, defaults to 0x2000
-t specifies a tape binary, by default TAPE
-u installs user ROMs; to install two ROMS separate the filenames by a comma
-z specifies an EPROM file to initially load from, and save into
F1: interrupt 1 (RST 1) - clear screen
F2: interrupt 2 (RST 2) - save and dump registers
F3: reset (RST 0)
F4: toggle pause
F5: write 8080 status to command line
F6: erase EPROM
F7: write EPROM
F9: exit emulator
```

### Implementation notes

#### Interrupts

First of all, because in Triton the interrupt system is so closely
tied to the RST instructions, it appears that they are synonymous but
they are not.  The following notes extracted from an email to Ian
Lockhart describe the situation in some detail.

The RST instructions are simply one-byte op-codes which execute a
subroutine call to the appropriate hard-coded place in memory, that's
to say they push the program counter on stack and jump to the required
location (0x0008 for RST 1, 0x0010 for RST 2, etc).  They have nothing
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
(0xFB) or DI (0xF3) instruction.

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
code.  One can check this in the emulator by using function F5 to
write the 8080 status (the interrupt enabled/disbled flag status is
E/D).  One can clearly see the interrupt enabled flag is left unset
after one of these hardware interrupts, but becomes re-enabled for
example after a key press.  Rather then being a bug it appears that
this was a design choice.  The section of the Triton manual describing
the CPU ("The Heart of It") states "Interrupt 0 should not be used
even though it is available on the PCB. It simply duplicates the
manual reset operation but would create major problems if used as the
Monitor program contains an EI instruction early on in this routine. A
very rapid build up of interrupt nests would occur which would fill up
the stack in a fraction of a second."

There is one peculiar thing though.  In the 8080A bugbook, there is
the statement that the microprocessor can still detect whether or not
an interrupt occurs even if the interrupt flag is disabled.  Further:
"If an interrupt did occur whilst the interrupt flag was disabled,
then an interrupt would occur as soon as the flag is enabled. Only one
interrupt event is remembered."  

Tests with actual hardware appear to indicate this is the case, for
example one can restart the machine, press interrupt 1 (nothing
happens because the interrrupt enabled flag is unset), then press a
key on the keyboard (for example 'W') and the pending interrupt is
serviced (clear screen in this case).

This logic is implemented in the emulator.

#### parity

Note that the parity bit is set if there is an even number of bits set
in the result, so it is an odd parity bit by the definition in
[Wikipedia](https://en.wikipedia.org/wiki/Parity_bit).  For Triton one
should have, for example
```
0x00 --> parity T
0x01 --> parity F
0x02 --> parity F
0x03 --> parity T
0x04 --> parity F
0x05 --> parity T
```
Some 8080 emulators get this the wrong way around.  In a Triton Level
7.2 emulation this leads to an extremely obscure bug since it only
shows up when trying to enter BASIC with the 'J' instruction -- the
emulation hangs.

The 8-bit parity calculator in the current emulator is based on a
[Stack Overflow implementation](https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd/21618038)
```
byte ^= byte >> 4;
byte ^= byte >> 2;
byte ^= byte >> 1;
parity = (byte & 0x01) ? false : true;
```  
One can test the effect of getting this the wrong way around by swapping the `true` and `false` in this.

### ROMs

ROM dumps for the Triton L7.2 Monitor and BASIC, and TRAP (Triton
Resident Assembly Language Package), are also included in this repository.  These can be
compiled to binaries by
```
./trimcc mona72_rom.tri -o  MONA72_ROM
./trimcc monb72_rom.tri -o  MONB72_ROM
./trimcc trap_rom.tri -o    TRAP_ROM
./trimcc basic72_rom.tri -o BASIC72_ROM
```
(implemented as `make roms` in the Makefile).
These `_ROM` files are loaded by the emulator.

### Keyboard emulation

The keyboard emulation strobes the data into port 3 so that bit 8 is
set the whole time that a key is depressed, and unset when it is
released.  This reflects the behaviour of the real hardware.

The 'Y' function is vectored through RAM at 0x1473 so to change the
vector change the locations at 0x1474/5.

### Tape emulation

### Printer emulation

I went through the monitor code (0x0104 - 0x0154) to understand just
what's happening with the serial printer output.  It turns out to be
not quite as stated as in the documentation - there is a start bit
(port 6 bit 8 high), followed by seven (7) data bits containing the
7-bit ASCII character code, a fake parity bit (port 6 bit 8 high
again), then two stop bits (port 6 bit 8 low).  For carriage return
the output is left low for an additional delay equivalent to 30 bits.
The two stop bits are handled in software by doubling the delay since
there is no need to write out two successive 0x00 bytes to port 6
(this has the potential to fool an emulation which is tracking port
access!).  Also port 6 is initialised to 0x00 during startup (monitor
code at 0x007d).

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
The timer delay that governs the baud rate is controlled by 0x1402-3
in user RAM as described in the manual.  For the fastest print speed
set these two bytes to 0x01 and 0x00 respectively.

### EPROM programmer emulation


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
