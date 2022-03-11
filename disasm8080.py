#! /usr/bin/env python3
#
# Disassembler for Intel 8080 microprocessor.
# Copyright (c) 2013-2015 by Jeff Tranter <tranter@pobox.com>
#
# Modifications copyright (c) 2021 Patrick B Warren
# <patrickbwarren@gmail.com>.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Typical usage:
# ./disasm8080.py -u -b -a <start_address> -f 3 <binary>
#
# Possible enhancements:
# - Read Intel HEX file format
# - binary (front panel) output mode, e.g.
#
#       ADDRESS              DATA
# . ... ... ... ... ...   .. ... ...
# 0 001 001 000 110 100   11 000 011  JMP
# 0 001 001 000 110 101   00 110 100  064
# 0 001 001 000 110 110   00 001 001  011

import sys
import argparse
import signal

# Avoids an error if paging through less for example

signal.signal(signal.SIGPIPE, signal.SIG_DFL)

# Lookup table - given opcode byte as index, return mnemonic of instruction
# and length of instruction.  Alternative opcodes have a '*' prepended.

lookupTable = [
    ['NOP',         1],  # 00
    ['LXI     B,',  3],  # 01
    ['STAX    B',   1],  # 02
    ['INX     B',   1],  # 03
    ['INR     B',   1],  # 04
    ['DCR     B',   1],  # 05
    ['MVI     B,',  2],  # 06
    ['RLC',         1],  # 07
    ['*NOP',        1],  # 08
    ['DAD     B',   1],  # 09
    ['LDAX    B',   1],  # 0A
    ['DCX     B',   1],  # 0B
    ['INR     C',   1],  # 0C
    ['DCR     C',   1],  # 0D
    ['MVI     C,',  2],  # 0E
    ['RRC',         1],  # 0F

    ['*NOP',        1],  # 10
    ['LXI     D,',  3],  # 11
    ['STAX    D',   1],  # 12
    ['INX     D',   1],  # 13
    ['INR     D',   1],  # 14
    ['DCR     D',   1],  # 15
    ['MVI     D,',  2],  # 16
    ['RAL',         1],  # 17
    ['*NOP',        1],  # 18
    ['DAD',         1],  # 19
    ['LDAX    D',   1],  # 1A
    ['DCX     D',   1],  # 1B
    ['INR     E',   1],  # 1C
    ['DCR     E',   1],  # 1D
    ['MVI     E,',  2],  # 1E
    ['RAR',         1],  # 1F

    ['*NOP',        1],  # 20
    ['LXI     H,',  3],  # 21
    ['SHLD    ',    3],  # 22
    ['INX     H',   1],  # 23
    ['INR     H',   1],  # 24
    ['DCR     H',   1],  # 25
    ['MVI     H,',  2],  # 26
    ['DAA',         1],  # 27
    ['*NOP',        1],  # 28
    ['DAD     H',   1],  # 29
    ['LHLD    ',    3],  # 2A
    ['DCX     H',   1],  # 2B
    ['INR     L',   1],  # 2C
    ['DCR     L',   1],  # 2D
    ['MVI     L,',  2],  # 2E
    ['CMA',         1],  # 2F

    ['*NOP',        1],  # 30
    ['LXI     SP,', 3],  # 31
    ['STA     ',    3],  # 32
    ['INX     SP',  1],  # 33
    ['INR     M',   1],  # 34
    ['DCR     M',   1],  # 35
    ['MVI     M,',  2],  # 36
    ['STC',         1],  # 37
    ['*NOP',        1],  # 38
    ['DAD     SP',  1],  # 39
    ['LDA     ',    3],  # 3A
    ['DCX     SP',  1],  # 3B
    ['INR     A',   1],  # 3C
    ['DCR     A',   1],  # 3D
    ['MVI     A,',  2],  # 3E
    ['CMC',         1],  # 3F

    ['MOV     B,B', 1],  # 40
    ['MOV     B,C', 1],  # 41
    ['MOV     B,D', 1],  # 42
    ['MOV     B,E', 1],  # 43
    ['MOV     B,H', 1],  # 44
    ['MOV     B,L', 1],  # 45
    ['MOV     B,M', 1],  # 46
    ['MOV     B,A', 1],  # 47
    ['MOV     C,B', 1],  # 48
    ['MOV     C,C', 1],  # 49
    ['MOV     C,D', 1],  # 4A
    ['MOV     C,E', 1],  # 4B
    ['MOV     C,H', 1],  # 4C
    ['MOV     C,L', 1],  # 4D
    ['MOV     C,M', 1],  # 4E
    ['MOV     C,A', 1],  # 4F

    ['MOV     D,B', 1],  # 50
    ['MOV     D,C', 1],  # 51
    ['MOV     D,D', 1],  # 52
    ['MOV     D,E', 1],  # 53
    ['MOV     D,H', 1],  # 54
    ['MOV     D,L', 1],  # 55
    ['MOV     D,M', 1],  # 56
    ['MOV     D,A', 1],  # 57
    ['MOV     E,B', 1],  # 58
    ['MOV     E,C', 1],  # 59
    ['MOV     E,D', 1],  # 5A
    ['MOV     E,E', 1],  # 5B
    ['MOV     E,H', 1],  # 5C
    ['MOV     E,L', 1],  # 5D
    ['MOV     E,M', 1],  # 5E
    ['MOV     E,A', 1],  # 5F

    ['MOV     H,B', 1],  # 60
    ['MOV     H,C', 1],  # 61
    ['MOV     H,D', 1],  # 62
    ['MOV     H,E', 1],  # 63
    ['MOV     H,H', 1],  # 64
    ['MOV     H,L', 1],  # 65
    ['MOV     H,M', 1],  # 66
    ['MOV     H,A', 1],  # 67
    ['MOV     L,B', 1],  # 68
    ['MOV     L,C', 1],  # 69
    ['MOV     L,D', 1],  # 6A
    ['MOV     L,E', 1],  # 6B
    ['MOV     L,H', 1],  # 6C
    ['MOV     L,L', 1],  # 6D
    ['MOV     L,M', 1],  # 6E
    ['MOV     L,A', 1],  # 6F

    ['MOV     M,B', 1],  # 70
    ['MOV     M,C', 1],  # 71
    ['MOV     M,D', 1],  # 72
    ['MOV     M,E', 1],  # 73
    ['MOV     M,H', 1],  # 74
    ['MOV     M,L', 1],  # 75
    ['HLT',         1],  # 76
    ['MOV     M,A', 1],  # 77
    ['MOV     A,B', 1],  # 78
    ['MOV     A,C', 1],  # 79
    ['MOV     A,D', 1],  # 7A
    ['MOV     A,E', 1],  # 7B
    ['MOV     A,H', 1],  # 7C
    ['MOV     A,L', 1],  # 7D
    ['MOV     A,M', 1],  # 7E
    ['MOV     A,A', 1],  # 7F

    ['ADD     B',   1],  # 80
    ['ADD     C',   1],  # 81
    ['ADD     D',   1],  # 82
    ['ADD     E',   1],  # 83
    ['ADD     H',   1],  # 84
    ['ADD     L',   1],  # 85
    ['ADD     M',   1],  # 86
    ['ADD     A',   1],  # 87
    ['ADC     B',   1],  # 88
    ['ADC     C',   1],  # 89
    ['ADC     D',   1],  # 8A
    ['ADC     E',   1],  # 8B
    ['ADC     H',   1],  # 8C
    ['ADC     L',   1],  # 8D
    ['ADC     M',   1],  # 8E
    ['ADC     A',   1],  # 8F

    ['SUB     B',   1],  # 90
    ['SUB     C',   1],  # 91
    ['SUB     D',   1],  # 92
    ['SUB     E',   1],  # 93
    ['SUB     H',   1],  # 94
    ['SUB     L',   1],  # 95
    ['SUB     M',   1],  # 96
    ['SUB     A',   1],  # 97
    ['SBB     B',   1],  # 98
    ['SBB     C',   1],  # 99
    ['SBB     D',   1],  # 9A
    ['SBB     E',   1],  # 9B
    ['SBB     H',   1],  # 9C
    ['SBB     L',   1],  # 9D
    ['SBB     M',   1],  # 9E
    ['SBB     A',   1],  # 9F

    ['ANA     B',   1],  # A0
    ['ANA     C',   1],  # A1
    ['ANA     D',   1],  # A2
    ['ANA     E',   1],  # A3
    ['ANA     H',   1],  # A4
    ['ANA     L',   1],  # A5
    ['ANA     M',   1],  # A6
    ['ANA     A',   1],  # A7
    ['XRA     B',   1],  # A8
    ['XRA     C',   1],  # A9
    ['XRA     D',   1],  # AA
    ['XRA     E',   1],  # AB
    ['XRA     H',   1],  # AC
    ['XRA     L',   1],  # AD
    ['XRA     M',   1],  # AE
    ['XRA     A',   1],  # AF

    ['ORA     B',   1],  # B0
    ['ORA     C',   1],  # B1
    ['ORA     D',   1],  # B2
    ['ORA     E',   1],  # B3
    ['ORA     H',   1],  # B4
    ['ORA     L',   1],  # B5
    ['ORA     M',   1],  # B6
    ['ORA     A',   1],  # B7
    ['CMP     B',   1],  # B8
    ['CMP     C',   1],  # B9
    ['CMP     D',   1],  # BA
    ['CMP     E',   1],  # BB
    ['CMP     H',   1],  # BC
    ['CMP     L',   1],  # BD
    ['CMP     M',   1],  # BE
    ['CMP     A',   1],  # BF

    ['RNZ',         1],  # C0
    ['POP     B',   1],  # C1
    ['JNZ     ',    3],  # C2
    ['JMP     ',    3],  # C3
    ['CNZ     ',    3],  # C4
    ['PUSH    B',   1],  # C5
    ['ADI     ',    2],  # C6
    ['RST     0',   1],  # C7
    ['RZ',          1],  # C8
    ['RET',         1],  # C9
    ['JZ      ',    3],  # CA
    ['*JMP     ',   3],  # CB
    ['CZ      ',    3],  # CC
    ['CALL    ',    3],  # CD
    ['ACI     ',    2],  # CE
    ['RST     1',   1],  # CF

    ['RNC',         1],  # D0
    ['POP     D',   1],  # D1
    ['JNC     ',    3],  # D2
    ['OUT     ',    2],  # D3
    ['CNC     ',    3],  # D4
    ['PUSH    D',   1],  # D5
    ['SUI     ',    2],  # D6
    ['RST     2',   1],  # D7
    ['RC',          1],  # D8
    ['*RET',        1],  # D9
    ['JC      ',    3],  # DA
    ['IN      ',    2],  # DB
    ['CC      ',    3],  # DC
    ['*CALL    ',   3],  # DD
    ['SBI     ',    2],  # DE
    ['RST     3',   1],  # DF

    ['RPO',         1],  # E0
    ['POP     H',   1],  # E1
    ['JPO     ',    3],  # E2
    ['XTHL',        1],  # E3
    ['CPO     ',    3],  # E4
    ['PUSH    H',   1],  # E5
    ['ANI     ',    2],  # E6
    ['RST     4',   1],  # E7
    ['RPE',         1],  # E8
    ['PCHL',        1],  # E9
    ['JPE     ',    3],  # EA
    ['XCHG',        1],  # EB
    ['CPE     ',    3],  # EC
    ['*CALL    ',   3],  # ED
    ['XRI     ',    2],  # EE
    ['RST     5',   1],  # EF

    ['RP',          1],  # F0
    ['POP     PSW', 1],  # F1
    ['JP      ',    3],  # F2
    ['DI',          1],  # F3
    ['CP      ',    3],  # F4
    ['PUSH    PSW', 1],  # F5
    ['ORI     ',    2],  # F6
    ['RST     6',   1],  # F7
    ['RM',          1],  # F8
    ['SPHL',        1],  # F9
    ['JM      ',    3],  # FA
    ['EI',          1],  # FB
    ['CM      ',    3],  # FC
    ['*CALL    ',   3],  # FD
    ['CPI     ',    2],  # FE
    ['RST     7',   1],  # FF
]

# Parse command line options

parser = argparse.ArgumentParser()
parser.add_argument('binary', nargs='?', help='binary file to disassemble, or /dev/stdin')
parser.add_argument('-c', '--chars', action='store_true', help='treat printable ASCII codes as characters')
parser.add_argument('-p', '--pipe', action='store_true', help='make output suitable for piping to TriMCC')
parser.add_argument('-a', '--address', help='Specify starting address (defaults to 0)')
parser.add_argument('-s', '--skip', help='Skip initial bytes (defaults to 0)')
args = parser.parse_args()

# Get file name from command line arguments

file_name = '/dev/stdin' if args.binary is None else args.binary

# Current instruction address. Silently force it to be in valid range.
# Use an eval in here to allow for hexadecimal.

if args.address:
    address = eval(args.address) & 0xffff
else:
    address = 0x0000

# Open input file. Display error and exit if file name does not exist.

try:
    f = open(file_name, 'rb')
except FileNotFoundError:
    print("error: input file '%s' not found." % file_name, file=sys.stderr)
    sys.exit(1)

# Skip over initial bytes if requested.
# Use an eval in here to allow for hexadecimal.

if args.skip:
    f.read(eval(args.skip))

# Print initial origin address

if args.pipe is False:
    print('%04X             %s     %04X' % (address, 'ORG', address))
else:
    print('ORG=%04X' % address) # formatted for TriMCC

while True:
    try:
        b = f.read(1)  # Get binary byte from file

        if len(b) == 0:  # EOF
            if args.pipe is False:
                print('%04X             %s' % (address, 'END')) # Exit if end of file reached.
            break

        address_data = '%04X  ' % address
        code = ''

        op = ord(b)  # Get opcode byte

        n = lookupTable[op][1]  # Look up number of instruction bytes

        mnem = lookupTable[op][0] # Get Johnny mnemonic

        # Print instruction bytes

        if n == 1:
            address_data += '%02X        ' % op
        elif n == 2:
            try:  # Possible to get exception here if EOF reached.
                op1 = ord(f.read(1))
            except TypeError:
                op1 = 0  # Fake it to recover from EOF
            address_data += '%02X %02X     ' % (op, op1)
        elif n == 3:
            try:  # Possible to get exception here if EOF reached.
                op1 = ord(f.read(1))
                op2 = ord(f.read(1))
            except TypeError:
                op1 = 0  # Fake it to recover from EOF
                op2 = 0
            address_data += '%02X %02X %02X  ' % (op, op1, op2)

        # If opcode starts with '*' then put in comment that this is an alternative op code (likely an error).

        if mnem[0] == '*':
            alternative = True
            mnem = mnem.replace(mnem[:1], '')  # Remove the star
        else:
            alternative = False

        code += mnem

        # Handle any operands

        if n == 2:
            if args.chars and op1 > 0x1f and op1 < 0x7f: # ASCII printable characters
                code += "'%c'" % op1
            else:
                code += '%02X' % op1
        elif n == 3:
            code += '%02X%02X' % (op2, op1) # little-endian order for 8080

        if alternative:
            mnem = mnem.replace(mnem[:1], '')  # Remove the star
            # Line up comment at fixed column position
            if args.pipe is False:
                comment_alt_op_code = '# alternative op code used'
            else:
                comment_alt_op_code = '; alternative op code used'
            comment_col = 55 if args.pipe is False else 35
            comment = comment_alt_op_code.rjust(comment_col - len(address_data))
        else:
            comment = ''

        # Update address
        address += n

        # Check for address exceeding 0xFFFF, if so wrap around.
        if address > 0xffff:
            address = address & 0xffff

        # Finished a line of disassembly

        if args.pipe is False:
            line = f'{address_data} {code} {comment}'
        else:
            line = ('%-20s' % code) + f' # {address_data} {comment}'

        print(line.rstrip())

    except KeyboardInterrupt:
        # Exit if end of file reached.
        print('Interrupted by Control-C', file=sys.stderr)
        print('%04X            %s' % (address, 'END'))
        break
