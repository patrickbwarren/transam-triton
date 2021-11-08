## SFML-based Triton emulator

This describes the fork of Robin Stuart's Triton emulator which is
available in a [GitHub repository](https://github.com/woo-j/triton).
A few small improvements have been made to reflect better the actual
hardware, and add functionality.  The emulator is targetted towards
the Level 7.2 firmware (Monitor and BASIC), and the Triton Resident
Assembly Language Package (TRAP).  Level 7.2 documentation can be
found on Robin's [GitHub repository](https://github.com/woo-j/triton)
and also in the 'ETI Triton 8080 Vintage Computer' Facebook group.

The emulator can be compiled using the `make triton` target in the
Makefile, or `make codes`.  The [SFML library](https://www.sfml-dev.org/) is required.

### Usage
```
SFML-based Triton emulator
./triton -m <mem_top> -t <tape_file> -u <user_rom(s)> -z <user_eprom>
-h (help): print this help
-m sets the top of memory, for example -m 0x4000, defaults to 0x2000
-t specifies a tape binary, by default TAPE
-u installs user ROM(s); to install two ROMS separate the filenames by a comma
-z specifies a file to write the EPROM to, with F7
F1: interrupt 1 (RST 1) - clear screen
F2: interrupt 2 (RST 2) - save and dump registers
F3: reset (RST 0)
F4: toggle pause
F5: write 8080 status to command line
F6: UV erase the EPROM (set all bytes to 0xff)
F7: write the EPROM to the file specified by -z
F9: exit emulator
```

More here...

The 'Y' function is vectored through RAM at `1473` so to change the
vector change the locations at `1474`/`5`.

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
code.  One can check this in the emulator by using function F5 to
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

Not yet implemented...

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
