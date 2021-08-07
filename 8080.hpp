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
 * Copyright (c) Patrick B Warren <patrickbwarren@gmail.com>.
 */

typedef struct ConditionCodes {
  bool    z;
  bool    s;
  bool    p;
  bool    cy;
  bool    ac;
} ConditionCodes;

typedef struct State8080 {
  uint8_t     a;
  uint8_t     b;
  uint8_t     c;
  uint8_t     d;
  uint8_t     e;
  uint8_t     h;
  uint8_t     l;
  uint16_t   sp;
  uint16_t   pc;
  uint8_t     *memory;
  struct  ConditionCodes   cc;
  bool    int_enable;
} State8080;

void printStatus(FILE *fp, State8080* state);
int Emulate8080Op(State8080* state);
void set_memory(State8080 *state, int address, uint8_t data);
uint8_t get_memory(State8080* state, int address);
