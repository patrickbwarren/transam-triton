/*
    triton - a Transam Triton emulator
    Copyright (C) 2020 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */

/* Forked from https://github.com/woo-j/triton
 * Additional modifications:
 * Copyright (c) 2021 Patrick B Warren (PBW) <patrickbwarren@gmail.com>.
 */

/* Intel 8080 emulator for the Transam Triton
 * All operands implemented except IN, OUT and HLT -- now added (PBW)
 * Also handles memory mapping
 * Uses the conventions described at Emulator101 (http://www.emulator101.com)
 * This code is supposed to be easy to understand rather than efficient!
 * Interrupts are handled elsewhere -- hardware interrupts now emulated (PBW)
 * Machine reset added (PBW)
 */

#include <iostream>
#include "8080.hpp"

#define MEM_BASE 0x1000
extern uint16_t mem_top; // should be defined in triton.cpp

// Macros: make sure we only write into user addressable RAM

#define MEM_READ(address) memory[(uint16_t)(address)]

#define MEM_WRITE(address, byte) { uint16_t addr_ = address;		\
    if ((addr_ >= MEM_BASE) && (addr_ < mem_top)) memory[addr_] = (uint8_t)(byte); }

// 8-bit parity calculator from
// https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd/21618038

bool Parity(uint8_t byte) {
  byte ^= byte >> 4;
  byte ^= byte >> 2;
  byte ^= byte >> 1;
  return (byte & 0x01) ? false : true;
}

void WriteStatus8080(FILE *fp, State8080 *state) {
  fprintf(fp, "A=%02X ", state->a);
  fprintf(fp, "BC=%02X%02X ", state->b, state->c);
  fprintf(fp, "DE=%02X%02X ", state->d, state->e);
  fprintf(fp, "HL=%02X%02X ", state->h, state->l);
  fprintf(fp, "SP=%04X ", state->sp);
  fprintf(fp, "PC=%04X ", state->pc);
  //fprintf(fp, "PC=%04X (%02X) ", state->pc, state->memory[state->pc]);
  fprintf(fp, "%c", state->cc.z ? 'Z' : 'z');
  fprintf(fp, "%c", state->cc.s ? 'S' : 's');
  fprintf(fp, "%c", state->cc.p ? 'P' : 'p');
  fprintf(fp, "%c", state->cc.cy ? 'C' : 'c');
  fprintf(fp, "%c", state->cc.ac ? 'A' : 'a');
  fprintf(fp, " %c", state->int_enable ? 'E' : 'D');
  if (state->interrupt) fprintf(fp, " pending %02X", state->interrupt);
  if (state->halted) fprintf(fp, " (halted)\n"); else fprintf(fp, "\n");
}

void Reset8080(State8080 *state) {
  state->a = 0x00;
  state->pc = 0x0000;
  state->int_enable = false;
  state->interrupt = 0x00;
  state->port_op = 0x00;
  state->port = 0x00;
  state->halted = false;
}

int SingleStep8080(State8080 *state, uint8_t *memory) { // return the number of machine cycles
  uint8_t *opcode = &memory[state->pc];
  uint8_t current_opcode;
  int answer;
  int offset;
  //printStatus(state);
  if (state->halted) return 0;
  if (state->interrupt && state->int_enable) { // service the interrupt
    current_opcode = state->interrupt;
    state->interrupt = 0x00;
    state->int_enable = false; // disable interrupts
    state->pc--; // necessary to cancel the increment in RST handler below
  } else {
    current_opcode = opcode[0]; // normal operation
  }
  switch(current_opcode) {
  case 0x00: // NOP - No-operation
  case 0x08:
  case 0x10:
  case 0x18:
  case 0x20:
  case 0x28:
  case 0x30:
  case 0x38:
    state->pc++;
    return 4;
  case 0x01: // LXI B - Load immediate register pair B & C
    state->c = opcode[1];
    state->b = opcode[2];
    state->pc += 3;
    return 10;
  case 0x02: // STAX B - Store accumulator
    offset = (state->b << 8) | (state->c);
    MEM_WRITE(offset, state->a);
    state->pc++;
    return 7;
  case 0x03: // INX B - Increment register pair
    answer = (state->b << 8) | (state->c);
    answer++;
    state->b = (answer >> 8) & 0xff;
    state->c = answer & 0xff;
    state->pc++;
    return 5;
  case 0x04: // INR B - Increment register
    answer = (int) state->b + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->b & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->b = answer & 0xff;
    state->pc++;
    return 5;
  case 0x05: // DCR B - Decrement register
    answer = (int) state->b + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->b & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->b = answer & 0xff;
    state->pc++;
    return 5;
  case 0x06: // MVI B - Move immediate register
    state->b = opcode[1];
    state->pc += 2;
    return 7;
  case 0x07: // RLC - Rotate accumulator left
    state->cc.cy = ((state->a & 0x80) != 0);
    state->a = (state->a << 1) & 0xff;
    state->a += state->cc.cy;
    state->pc++;
    return 4;
    //case 0x08: Duplicate of NOP
  case 0x09: // DAD B - Double add
    offset = (state->h << 8) | (state->l);
    answer = (state->b << 8) | (state->c);
    state->cc.cy = (offset + answer > 0xffff);
    answer += offset;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 10;
  case 0x0a: // LDAX B - Load accumulator
    offset = (state->b << 8) | (state->c);
    state->a = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x0b: // DCX B - Decrement register pair
    answer = (state->b << 8) | (state->c);
    answer--;
    state->b = (answer >> 8) & 0xff;
    state->c = answer & 0xff;
    state->pc++;
    return 5;
  case 0x0c: // INR C - Increment register
    answer = (int) state->c + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->c & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->c = answer & 0xff;
    state->pc++;
    return 5;
  case 0x0d: // DCR C - Decrement register
    answer = (int) state->c + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->c & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->c = answer & 0xff;
    state->pc++;
    return 5;
  case 0x0e: // MVI C - Move immediate register
    state->c = opcode[1];
    state->pc += 2;
    return 7;
  case 0x0f: // RRC - Rotate accumulator right
    state->cc.cy = ((state->a & 0x01) != 0);
    state->a = (state->a >> 1) & 0xff;
    state->a += state->cc.cy << 7;
    state->pc++;
    return 4;
    //case 0x10: Duplicate of NOP
  case 0x11: // LXI D - Load immediate register pair D & E
    state->e = opcode[1];
    state->d = opcode[2];
    state->pc += 3;
    return 10;
  case 0x12: // STAX D - Store accumulator
    offset = (state->d << 8) | (state->e);
    MEM_WRITE(offset, state->a);
    state->pc++;
    return 7;
  case 0x13: // INX D - Increment register pair
    answer = (state->d << 8) | (state->e);
    answer++;
    state->d = (answer >> 8) & 0xff;
    state->e = answer & 0xff;
    state->pc++;
    return 5;
  case 0x14: // INR D - Increment register
    answer = (int) state->d + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->d & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->d = answer & 0xff;
    state->pc++;
    return 5;
  case 0x15: // DCR D - Decrement register
    answer = (int) state->d + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->d & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->d = answer & 0xff;
    state->pc++;
    return 5;
  case 0x16: // MVI D - Move immediate register
    state->d = opcode[1];
    state->pc += 2;
    return 7;
  case 0x17: // RAL - Rotate accumulator left through carry
    answer = (int) state->a << 1;
    answer += state->cc.cy;
    state->cc.cy = (answer > 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
    //case 0x18: Duplicate of NOP
  case 0x19: // DAD D - Double add
    offset = (state->h << 8) | (state->l);
    answer = (state->d << 8) | (state->e);
    state->cc.cy = (offset + answer > 0xffff);
    answer += offset;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 10;
  case 0x1a: // LDAX D - Load accumulator
    offset = (state->d << 8) | (state->e);
    state->a = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x1b: // DCX D - Decrement register pair
    answer = (state->d << 8) | (state->e);
    answer--;
    state->d = (answer >> 8) & 0xff;
    state->e = answer & 0xff;
    state->pc++;
    return 5;
  case 0x1c: // INR E - Increment register
    answer = (int) state->e + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->e & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->e = answer & 0xff;
    state->pc++;
    return 5;
  case 0x1d: // DCR E - Decrement register
    answer = (int) state->e + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->e & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->e = answer & 0xff;
    state->pc++;
    return 5;
  case 0x1e: // MVI E - Move immediate register
    state->e = opcode[1];
    state->pc += 2;
    return 7;
  case 0x1f: // RAR - Rotate accumulator right through carry
    answer = (int) state->a >> 1;
    answer += state->cc.cy << 7;
    state->cc.cy = ((state->a & 0x01) != 0);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
    //case 0x20: Duplicate of NOP
  case 0x21: // LXI H - Load immediate register pair H & L
    state->l = opcode[1];
    state->h = opcode[2];
    state->pc += 3;
    return 10;
  case 0x22: // SHLD - Store H and L direct
    offset = (opcode[2] << 8) | opcode[1];
    MEM_WRITE(offset, state->l);
    MEM_WRITE(offset + 1, state->h);
    state->pc += 3;
    return 16;
  case 0x23: // INX H - Increment register pair
    answer = (state->h << 8) | (state->l);
    answer++;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 5;
  case 0x24: // INR H - Increment register
    answer = (int) state->h + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->h & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->h = answer & 0xff;
    state->pc++;
    return 5;
  case 0x25: // DCR H - Decrement register
    answer = (int) state->h + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->h & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->h = answer & 0xff;
    state->pc++;
    return 5;
  case 0x26: // MVI H - Move immediate register
    state->h = opcode[1];
    state->pc += 2;
    return 7;
  case 0x27: // DAA - Decimal adjust accumulator
    if (((state->a & 0x0f) > 0x09) | (state->cc.ac != 0)) {
      state->a += 0x06;
      state->cc.ac = true;
    } else state->cc.ac = false;
    if (((state->a & 0xf0) > 0x90) | (state->cc.cy != 0)) {
      state->a = (state->a + 0x60) & 0xff;
      state->cc.cy = true;
    }
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a);
    state->pc++;
    return 4;
    //case 0x28: Duplicate of NOP
  case 0x29: // DAD H - Double add
    offset = (state->h << 8) | (state->l);
    answer = (state->h << 8) | (state->l);
    state->cc.cy = (offset + answer > 0xffff);
    answer += offset;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 10;
  case 0x2a: // LHLD - Load H and L direct
    offset = (opcode[2] << 8) | opcode[1];
    state->l = MEM_READ(offset);
    state->h = MEM_READ(offset + 1);
    state->pc += 3;
    return 16;
  case 0x2b: // DCX H - Decrement register pair
    answer = (state->h << 8) | (state->l);
    answer--;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 5;
  case 0x2c: // INR L - Increment register
    answer = (int) state->l + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->l & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->l = answer & 0xff;
    state->pc++;
    return 5;
  case 0x2d: // DCR L - Decrement register
    answer = (int) state->l + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->l & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->l = answer & 0xff;
    state->pc++;
    return 5;
  case 0x2e: // MVI L - Move immediate register
    state->l = opcode[1];
    state->pc += 2;
    return 7;
  case 0x2f: // CMA - Complement accumulator
    state->a = ~state->a & 0xff;
    state->pc++;
    return 4;
    //case 0x30: Duplicate of NOP
  case 0x31: // LXI SP - Load immediate register pair B & C
    state->sp = (opcode[2] << 8) + opcode[1];
    state->pc += 3;
    return 10;
  case 0x32: // STA - Store accumulator direct
    offset = (opcode[2] << 8) | opcode[1];
    MEM_WRITE(offset, state->a);
    state->pc += 3;
    return 13;
  case 0x33: // INX SP - Increment register pair
    state->sp++;
    state->pc++;
    return 5;
  case 0x34: // INR M - Increment memory
    offset = (state->h << 8) | (state->l);
    answer = (int) MEM_READ(offset) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((MEM_READ(offset) & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    MEM_WRITE(offset, answer & 0xff);
    state->pc++;
    return 10;
  case 0x35: // DCR M - Decrement memory
    offset = (state->h << 8) | (state->l);
    answer = (int) MEM_READ(offset) + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((MEM_READ(offset) & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    MEM_WRITE(offset, answer & 0xff);
    state->pc++;
    return 10;
  case 0x36: // MVI M - Move immediate memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, opcode[1]);
    state->pc += 2;
    return 10;
  case 0x37: // STC - Set Carry
    state->cc.cy = true;
    state->pc++;
    return 4;
    //case 0x38: Duplicate of NOP
  case 0x39: // DAD SP - Double add
    offset = (state->h << 8) | (state->l);
    answer = state->sp;
    state->cc.cy = (offset + answer > 0xffff);
    answer += offset;
    state->h = (answer >> 8) & 0xff;
    state->l = answer & 0xff;
    state->pc++;
    return 10;
  case 0x3a: // LDA - Load accumulator direct
    offset = (opcode[2] << 8) | opcode[1];
    state->a = MEM_READ(offset);
    state->pc += 3;
    return 13;
  case 0x3b: // DCX SP - Decrement register pair
    state->sp--;
    state->pc++;
    return 5;
  case 0x3c:// INR A - Increment register
    answer = (int) state->a + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 5;
  case 0x3d: // DCR A - Decrement register
    answer = (int) state->a + 0xff;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + 1 > 0x0f);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 5;
  case 0x3e: // MVI A - Move immediate register
    state->a = opcode[1];
    state->pc += 2;
    return 7;
  case 0x3f: // CMC - Complement Carry
    state->cc.cy = !(state->cc.cy);
    state->pc++;
    return 4;
  case 0x40: // MOV B,B = NOP
  case 0x49: // MOV C,C = NOP
  case 0x52: // MOV D,D = NOP
  case 0x5b: // MOV E,E = NOP
  case 0x64: // MOV H,H = NOP
  case 0x6d: // MOV L,L = NOP
  case 0x7f: // MOV A,A = NOP
    state->pc++;
    return 5;
  case 0x41: // MOV B,C - Move register to register
    state->b = state->c;
    state->pc++;
    return 5;
  case 0x42: // MOV B,D - Move register to register
    state->b = state->d;
    state->pc++;
    return 5;
  case 0x43: // MOV B,E - Move register to register
    state->b = state->e;
    state->pc++;
    return 5;
  case 0x44: // MOV B,H - Move register to register
    state->b = state->h;
    state->pc++;
    return 5;
  case 0x45: // MOV B,L - Move register to register
    state->b = state->l;
    state->pc++;
    return 5;
  case 0x46: // MOV B,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->b = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x47: // MOV B,A - Move register to register
    state->b = state->a;
    state->pc++;
    return 5;
  case 0x48: // MOV C,B - Move register to register
    state->c = state->b;
    state->pc++;
    return 5;
    // case 0x49: // MOV C,C = NOP
  case 0x4a: // MOV C,D - Move register to register
    state->c = state->d;
    state->pc++;
    return 5;
  case 0x4b: // MOV C,E - Move register to register
    state->c = state->e;
    state->pc++;
    return 5;
  case 0x4c: // MOV C,H - Move register to register
    state->c = state->h;
    state->pc++;
    return 5;
  case 0x4d: // MOV C,L - Move register to register
    state->c = state->l;
    state->pc++;
    return 5;
  case 0x4e: // MOV C,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->c = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x4f: // MOV C,A - Move register to register
    state->c = state->a;
    state->pc++;
    return 5;
  case 0x50: // MOV D,B - Move register to register
    state->d = state->b;
    state->pc++;
    return 5;
  case 0x51: // MOV D,C - Move register to register
    state->d = state->c;
    state->pc++;
    return 5;
    // case 0x52: // MOV D,D = NOP
  case 0x53: // MOV D,E - Move register to register
    state->d = state->e;
    state->pc++;
    return 5;
  case 0x54: // MOV D,H - Move register to register
    state->d = state->h;
    state->pc++;
    return 5;
  case 0x55: // MOV D,L - Move register to register
    state->d = state->l;
    state->pc++;
    return 5;
  case 0x56: // MOV D,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->d = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x57: // MOV D,A - Move register to register
    state->d = state->a;
    state->pc++;
    return 5;
  case 0x58: // MOV E,B - Move register to register
    state->e = state->b;
    state->pc++;
    return 5;
  case 0x59: // MOV E,C - Move register to register
    state->e = state->c;
    state->pc++;
    return 5;
  case 0x5a: // MOV E,D - Move register to register
    state->e = state->d;
    state->pc++;
    return 5;
    //case 0x5b: // MOV E,E = NOP
  case 0x5c: // MOV E,H - Move register to register
    state->e = state->h;
    state->pc++;
    return 5;
  case 0x5d: // MOV E,L - Move register to register
    state->e = state->l;
    state->pc++;
    return 5;
  case 0x5e: // MOV E,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->e = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x5f: // MOV E,A - Move register to register
    state->e = state->a;
    state->pc++;
    return 5;
  case 0x60: // MOV H,B - Move register to register
    state->h = state->b;
    state->pc++;
    return 5;
  case 0x61: // MOV H,C - Move register to register
    state->h = state->c;
    state->pc++;
    return 5;
  case 0x62: // MOV H,D - Move register to register
    state->h = state->d;
    state->pc++;
    return 5;
  case 0x63: // MOV H,E - Move register to register
    state->h = state->e;
    state->pc++;
    return 5;
    //case 0x64: // MOV H,H = NOP
  case 0x65: // MOV H,L - Move register to register
    state->h = state->l;
    state->pc++;
    return 5;
  case 0x66: // MOV H,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->h = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x67: // MOV H,A - Move register to register
    state->h = state->a;
    state->pc++;
    return 5;
  case 0x68: // MOV L,B - Move register to register
    state->l = state->b;
    state->pc++;
    return 5;
  case 0x69: // MOV L,C - Move register to register
    state->l = state->c;
    state->pc++;
    return 5;
  case 0x6a: // MOV L,D - Move register to register
    state->l = state->d;
    state->pc++;
    return 5;
  case 0x6b: // MOV L,E - Move register to register
    state->l = state->e;
    state->pc++;
    return 5;
  case 0x6c: // MOV L,H - Move register to register
    state->l = state->h;
    state->pc++;
    return 5;
    //case 0x6d: // MOV L,L = NOP
  case 0x6e: // MOV L,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->l = MEM_READ(offset);
    state->pc++;
    return 7;
  case 0x6f: // MOV L,A - Move register to register
    state->l = state->a;
    state->pc++;
    return 5;
  case 0x70: // MOV M,B - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->b);
    state->pc++;
    return 7;
  case 0x71: // MOV M,C - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->c);
    state->pc++;
    return 7;
  case 0x72: // MOV M,D - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->d);
    state->pc++;
    return 7;
  case 0x73: // MOV M,E - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->e);
    state->pc++;
    return 7;
  case 0x74: // MOV M,H - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->h);
    state->pc++;
    return 7;
  case 0x75: // MOV M,L - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->l);
    state->pc++;
    return 7;
  case 0x76: // HLT - Halt
    state->halted = true;
    return 7;
  case 0x77: // MOV M,A - Move register to memory
    offset = (state->h << 8) | (state->l);
    MEM_WRITE(offset, state->a);
    state->pc++;
    return 7;
  case 0x78: // MOV A,B - Move register to register
    state->a = state->b;
    state->pc++;
    return 5;
  case 0x79: // MOV A,C - Move register to register
    state->a = state->c;
    state->pc++;
    return 5;
  case 0x7a: // MOV A,D - Move register to register
    state->a = state->d;
    state->pc++;
    return 5;
  case 0x7b: // MOV A,E - Move register to register
    state->a = state->e;
    state->pc++;
    return 5;
  case 0x7c: // MOV A,H - Move register to register
    state->a = state->h;
    state->pc++;
    return 5;
  case 0x7d: // MOV A,L - Move register to register
    state->a = state->l;
    state->pc++;
    return 5;
  case 0x7e: // MOV A,M - Move memory to register
    offset = (state->h << 8) | (state->l);
    state->a = MEM_READ(offset);
    state->pc++;
    return 7;
    //case 0x7f: // MOV A,A = NOP
  case 0x80: // ADD B - Add register to A
    answer = (int) state->a + (int) state->b;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->b & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x81: // ADD C - Add register to A
    answer = (int) state->a + (int) state->c;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->c & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x82: // ADD D - Add register to A
    answer = (int) state->a + (int) state->d;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->d & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x83: // ADD E - Add register to A
    answer = (int) state->a + (int) state->e;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->e & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x84: // ADD H - Add register to A
    answer = (int) state->a + (int) state->h;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->h & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x85: // ADD L - Add register to A
    answer = (int) state->a + (int) state->l;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->l & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x86: // ADD M - Add memory to A
    offset = (state->h << 8) | (state->l);
    answer = (int) state->a + MEM_READ(offset);
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (MEM_READ(offset) & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 7;
  case 0x87: // ADD A - Add register to A
    answer = (int) state->a + (int) state->a;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->a & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x88: // ADC B - Add register to A with carry
    answer = (int) state->a + (int) state->b + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->b & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x89: // ADC C - Add register to A with carry
    answer = (int) state->a + (int) state->c + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->c & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x8a: // ADC D - Add register to A with carry
    answer = (int) state->a + (int) state->d + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->d & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x8b: // ADC E - Add register to A with carry
    answer = (int) state->a + (int) state->e + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->e & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x8c: // ADC H - Add register to A with carry
    answer = (int) state->a + (int) state->h + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->h & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x8d: // ADC L - Add register to A with carry
    answer = (int) state->a + (int) state->l + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->l & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x8e: // ADC M - Add memory to A with carry
    offset = (state->h << 8) | (state->l);
    answer = (int) state->a + (int) MEM_READ(offset) + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (MEM_READ(offset) & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 7;
  case 0x8f: // ADC A - Add register to A with carry
    answer = (int) state->a + (int) state->a + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (state->a & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x90: // SUB B - Subtract register from A
    answer = (int) state->a + (int) (~state->b & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->b & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x91: // SUB C - Subtract register from A
    answer = (int) state->a + (int) (~state->c & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->c & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x92: // SUB D - Subtract register from A
    answer = (int) state->a + (int) (~state->d & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->d & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x93: // SUB E - Subtract register from A
    answer = (int) state->a + (int) (~state->e & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->e & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x94: // SUB H - Subtract register from A
    answer = (int) state->a + (int) (~state->h & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->h & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x95: // SUB L - Subtract register from A
    answer = (int) state->a + (int) (~state->l & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->l & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x96: // SUB M - Subtract memory from A
    offset = (state->h << 8) | (state->l);
    answer = (int) state->a + (int) (~MEM_READ(offset) & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~MEM_READ(offset) & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 7;
  case 0x97: // SUB A - Subtract register from A
    answer = (int) state->a + (int) (~state->a & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->a & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x98: // SBB B - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->b & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->b & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x99: // SBB C - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->c & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->c & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x9a: // SBB D - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->d & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->d & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x9b: // SBB E - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->e & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->e & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x9c: // SBB H - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->h & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->h & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x9d: // SBB L - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->l & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->l & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0x9e: // SBB M - Subtract memory from A with borrow
    offset = (state->h << 8) | (state->l);
    answer = (int) state->a + (int) (~MEM_READ(offset) & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~MEM_READ(offset) & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 7;
  case 0x9f: // SBB A - Subtract register from A with borrow
    answer = (int) state->a + (int) (~state->a & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->a & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc++;
    return 4;
  case 0xa0: // ANA B - Logical AND register with accumulator
    state->a &= state->b;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa1: // ANA C - Logical AND register with accumulator
    state->a &= state->c;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa2:  // ANA D - Logical AND register with accumulator
    state->a &= state->d;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa3:  // ANA E - Logical AND register with accumulator
    state->a &= state->e;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa4:  // ANA H - Logical AND register with accumulator
    state->a &= state->h;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa5:  // ANA L - Logical AND register with accumulator
    state->a &= state->l;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa6: // ANA M - Logical AND memory with accumulator
    offset = (state->h << 8) | (state->l);
    state->a &= MEM_READ(offset);
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 7;
  case 0xa7: // ANA A - Logical AND register with accumulator
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa8: // XRA B - Logical exclusive-OR register with accumulator
    state->a ^= state->b;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xa9: // XRA C - Logical exclusive-OR register with accumulator
    state->a ^= state->c;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xaa: // XRA D - Logical exclusive-OR register with accumulator
    state->a ^= state->d;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xab: // XRA E - Logical exclusive-OR register with accumulator
    state->a ^= state->e;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xac: // XRA H - Logical exclusive-OR register with accumulator
    state->a ^= state->h;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xad: // XRA L - Logical exclusive-OR register with accumulator
    state->a ^= state->l;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xae: // XRA M - Logical exclusive-OR register with memory
    offset = (state->h << 8) | (state->l);
    state->a ^= MEM_READ(offset);
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 7;
  case 0xaf: // XRA A - Logical exclusive-OR register with accumulator
    state->a ^= state->a;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb0: // ORA B - Logical OR register with accumulator
    state->a |= state->b;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb1: // ORA C - Logical OR register with accumulator
    state->a |= state->c;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb2: // ORA D - Logical OR register with accumulator
    state->a |= state->d;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb3: // ORA E - Logical OR register with accumulator
    state->a |= state->e;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb4: // ORA H - Logical OR register with accumulator
    state->a |= state->h;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb5: // ORA L - Logical OR register with accumulator
    state->a |= state->l;
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb6: // ORA M - Logical OR register with memory
    offset = (state->h << 8) | (state->l);
    state->a |= MEM_READ(offset);
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 7;
  case 0xb7: // ORA A - Logical OR register with accumulator
    state->cc.z = ((state->a & 0xff) == 0);
    state->cc.s = ((state->a & 0x80) != 0);
    state->cc.p = Parity(state->a & 0xff);
    state->cc.cy = false;
    state->cc.ac = false;
    state->pc++;
    return 4;
  case 0xb8: // CMP B - Compare register with accumulator
    answer = (int) state->a + (int) (~state->b & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->b & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xb9: // CMP C - Compare register with accumulator
    answer = (int) state->a + (int) (~state->c & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->c & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xba: // CMP D - Compare register with accumulator
    answer = (int) state->a + (int) (~state->d & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->d & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xbb: // CMP E - Compare register with accumulator
    answer = (int) state->a + (int) (~state->e & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->e & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xbc: // CMP H - Compare register with accumulator
    answer = (int) state->a + (int) (~state->h & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->h & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xbd: // CMP L - Compare register with accumulator
    answer = (int) state->a + (int) (~state->l & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->l & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xbe: // CMP M - Compare memory with accumulator
    offset = (state->h << 8) | (state->l);
    answer = (int) state->a + (int) (~MEM_READ(offset) & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~state->l & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc++;
    return 4;
  case 0xbf: // CMP A - Compare register with accumulator
    state->cc.z = true;
    state->cc.s = false;
    state->cc.ac = ((state->a & 0x0f) + (~state->a & 0x0f) + 1 > 0x0f);
    state->cc.cy = true;
    state->cc.p = Parity(0x00);
    state->pc++;
    return 4;
  case 0xc0: // RNZ - Return if not zero
    if (state->cc.z == false) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xc1: // POP B - Pop data off stack
    state->c = MEM_READ(state->sp);
    state->b = MEM_READ(state->sp + 1);
    state->sp += 2;
    state->pc++;
    return 10;
  case 0xc2: // JNZ - Jump if not zero
    if (state->cc.z == false)  state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xc3: // JMP - Jump
  case 0xcb:
    state->pc = (opcode[2] << 8) | opcode[1];
    return 10;
  case 0xc4: // CNZ - Call if no zero
    if (state->cc.z == false) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
  case 0xc5: // PUSH B - Push data onto stack
    MEM_WRITE(state->sp - 1, state->b);
    MEM_WRITE(state->sp - 2, state->c);
    state->sp -= 2;
    state->pc++;
    return 11;
  case 0xc6: // ADI - Add immediate to A
    answer = (int) state->a + (int) opcode[1];
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (opcode[1] & 0x0f) > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc += 2;
    return 7;
  case 0xc7: // RST - call subroutine at specified location
  case 0xcf:
  case 0xd7:
  case 0xdf:
  case 0xe7:
  case 0xef:
  case 0xf7:
  case 0xff:
    state->pc++;
    MEM_WRITE(state->sp - 2, state->pc & 0xff);
    MEM_WRITE(state->sp - 1, state->pc >> 8);
    state->sp -= 2;
    state->pc = (int) current_opcode & 0x38; // not opcode[0] since may be servicing an interrupt
    return 11;
  case 0xc8: // RZ - Return if zero
    if (state->cc.z) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xc9: // RET - Return
  case 0xd9:
    state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
    state->sp += 2;
    return 10;
  case 0xca: // JZ - Jump if zero
    if (state->cc.z) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
    //case 0xcb: Duplicate of JMP
  case 0xcc: // CZ - Call on zero
    if (state->cc.z) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
  case 0xcd: // CALL - Call
  case 0xdd:
  case 0xed:
  case 0xfd:
    offset = state->pc + 3;
    MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
    MEM_WRITE(state->sp - 2, (offset & 0xff));
    state->sp -= 2;
    state->pc = (opcode[2] << 8) | opcode[1];
    return 17;
  case 0xce: // ACI - Add immediate to A with carry
    answer = (int) state->a + (int) opcode[1] + (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (opcode[1] & 0x0f) + state->cc.cy > 0x0f);
    state->cc.cy = (answer > 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc += 2;
    return 7;
    //case 0xcf: Restart
  case 0xd0: // RC - Return if no carry
    if (state->cc.cy == false) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xd1: // POP D - Pop data off stack
    state->e = MEM_READ(state->sp);
    state->d = MEM_READ(state->sp + 1);
    state->sp += 2;
    state->pc++;
    return 10;
  case 0xd2: // JNC - Jump if no carry
    if (state->cc.cy == false) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xd3: // OUT - output to port
    state->port_op = current_opcode;
    state->port = opcode[1];
    state->pc += 2;
    return 10;
  case 0xd4: // CNC - Call if no carry
    if (state->cc.cy == false) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
  case 0xd5: // PUSH D - Push data onto stack
    MEM_WRITE(state->sp - 1, state->d);
    MEM_WRITE(state->sp - 2, state->e);
    state->sp -= 2;
    state->pc++;
    return 11;
  case 0xd6: // SUI - Subtract immediate from A
    answer = (int) state->a + (int) (~opcode[1] & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~opcode[1] & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc += 2;
    return 7;
    //case 0xd7: Restart
  case 0xd8: // RC - Return if carry
    if (state->cc.cy) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
    //case 0xd9: Duplication of RET
  case 0xda: // JC - Jump if carry
    if (state->cc.cy) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xdb: // IN - Input from port
    state->port_op = current_opcode;
    state->port = opcode[1];
    state->pc += 2;
    return 10;
  case 0xdc: // CC - Call if carry
    if (state->cc.cy) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
    //case 0xdd: Duplicate of CALL
  case 0xde: // SBI - Subtract immediate from A with borrow
    answer = (int) state->a + (int) (~opcode[1] & 0xff) + 1;
    answer -= (int) state->cc.cy;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~opcode[1] & 0x0f) + 1 + state->cc.cy > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->a = answer & 0xff;
    state->pc += 2;
    return 7;
    //case 0xdf: Restart
  case 0xe0: // RPO - Return if parity odd
    if (state->cc.p == false) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xe1: // POP H - Pop data off stack
    state->l = MEM_READ(state->sp);
    state->h = MEM_READ(state->sp + 1);
    state->sp += 2;
    state->pc++;
    return 10;
  case 0xe2: // JPO - Jump if parity odd
    if (state->cc.p == false) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xe3: // XTHL - Exchange stack
    offset = (state->h << 8) | (state->l);
    state->l = MEM_READ(state->sp);
    state->h = MEM_READ(state->sp + 1);
    MEM_WRITE(state->sp + 1, offset >> 8);
    MEM_WRITE(state->sp, offset & 0xff);
    state->pc++;
    return 18;
  case 0xe4: // CPO - Call if parity odd
    if (state->cc.p == false) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
  case 0xe5: // PUSH H - Push data onto stack
    MEM_WRITE(state->sp - 1, state->h);
    MEM_WRITE(state->sp - 2, state->l);
    state->sp -= 2;
    state->pc++;
    return 11;
  case 0xe6: // ANI - AND immediate with accumulator
    state->a &= opcode[1];
    state->cc.cy = false;
    state->cc.z = (state->a == 0x00);
    state->cc.s = ((state->a & 0x80) != 0x00);
    state->cc.p = Parity(state->a);
    state->pc += 2;
    return 7;
    //case 0xe7: Restart
  case 0xe8: // RPE - Return if parity even
    if (state->cc.p) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xe9: // PCHL - Load program counter
    state->pc = (state->h << 8) | state->l;
    return 5;
  case 0xea: // JPE - Jump if parity even
    if (state->cc.p) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xeb: // XCHG - Exchange registers
    offset = (state->h << 8) | (state->l);
    state->h = state->d;
    state->l = state->e;
    state->d = offset >> 8;
    state->e = offset & 0xff;
    state->pc++;
    return 4;
  case 0xec: // CPC - Call if parity even
    if (state->cc.p) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
    //case 0xed: Duplicate of CALL
  case 0xee: // XRI - Exclusive-OR immediate with accumulator
    state->a ^= opcode[1];
    state->cc.cy = false;
    state->cc.z = (state->a == 0x00);
    state->cc.s = ((state->a & 0x80) != 0x00);
    state->cc.p = Parity(state->a);
    state->pc += 2;
    return 7;
    //case 0xef: Restart
  case 0xf0: // RP - Return if plus
    if (state->cc.s == false) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xf1: // POP PSW - Pop data off stack
    state->cc.s = ((MEM_READ(state->sp) & 0x80) > 0);
    state->cc.z = ((MEM_READ(state->sp) & 0x40) > 0);
    state->cc.ac = ((MEM_READ(state->sp) & 0x10) > 0);
    state->cc.p = ((MEM_READ(state->sp) & 0x04) > 0);
    state->cc.cy = ((MEM_READ(state->sp) & 0x01) > 0);
    state->a = MEM_READ(state->sp + 1);
    state->sp += 2;
    state->pc++;
    return 10;
  case 0xf2: // JP - Jump if positive
    if (state->cc.s == false) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xf3: // DI - Disable interrupts
    state->int_enable = false;
    state->pc++;
    return 4;
  case 0xf4: // CP - Call if plus
    if (state->cc.s == false) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
  case 0xf5: // PUSH PSW - Push data onto stack
    answer = state->cc.cy + 0x10;
    answer += state->cc.p << 2;
    answer += state->cc.ac << 4;
    answer += state->cc.z << 6;
    answer += state->cc.s << 7;
    MEM_WRITE(state->sp - 2, answer & 0xff);
    MEM_WRITE(state->sp - 1, state->a);
    state->sp -= 2;
    state->pc++;
    return 11;
  case 0xf6: // ORI - OR immediate with accumulator
    state->a |= opcode[1];
    state->cc.cy = false;
    state->cc.z = (state->a == 0x00);
    state->cc.s = ((state->a & 0x80) != 0x00);
    state->cc.p = Parity(state->a);
    state->pc += 2;
    return 7;
    //case 0xf7: Restart
  case 0xf8: // RM - Return if minus
    if (state->cc.s) {
      state->pc = MEM_READ(state->sp) | (MEM_READ(state->sp + 1) << 8);
      state->sp += 2;
    } else state->pc++;
    return 11;
  case 0xf9: // SPHL - Load SP from H and L
    state->sp = (state->h << 8) | (state->l);
    state->pc++;
    return 5;
  case 0xfa: // JM - Jump if minus
    if (state->cc.s) state->pc = (opcode[2] << 8) | opcode[1];
    else state->pc += 3;
    return 10;
  case 0xfb: // EI - Enable interrupts
    state->int_enable = true;
    state->pc++;
    return 4;
  case 0xfc: // CM - Call if minus
    if (state->cc.s) {
      offset = state->pc + 3;
      MEM_WRITE(state->sp - 1, (offset >> 8) & 0xff);
      MEM_WRITE(state->sp - 2, (offset & 0xff));
      state->sp -= 2;
      state->pc = (opcode[2] << 8) | opcode[1];
    } else state->pc += 3;
    return 11;
    //case 0xfd: Duplicate of CALL
  case 0xfe: // CPI - Compare immediate with accumulator
    answer = (int) state->a + (int) (~opcode[1] & 0xff) + 1;
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.ac = ((state->a & 0x0f) + (~opcode[1] & 0x0f) + 1 > 0x0f);
    state->cc.cy = (answer <= 0xff);
    state->cc.p = Parity(answer & 0xff);
    state->pc += 2;
    return 7;
    //case 0xff: RST 7 
  }
}
