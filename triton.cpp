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

/* Emulator for the Transam Triton
 * Created using only information available online - so expect bugs!
 * Central processor is Intel 8080A
 * VDU is based on Thomson-CSD chip (SFC96364) for which I can find no documentation
 * Most of the rest of the machine is 74 logic ICs
 *
 * Only available ROMS (version 7.2) are currently hard-wired
 */

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "8080.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>
#include <string>
#include <unistd.h>

#define _1K 0x400
#define _8K 0x2000
#define _64K 0x10000

uint16_t mem_top;

typedef enum {INPUT, OUTPUT} direction_t;

typedef struct StateEPROM {
  char *file = NULL;
  uint8_t a, b, c, ctl;
  uint8_t rom[_1K];
  int write_count[_1K];
  bool chip_select = false;
  bool write_enable = false;
  direction_t portA_dirn = OUTPUT;
} StateEPROM;

// #define PRINTF_HI(byte) for (int i=7; i>=4; i--) printf("%c", (byte >> i) & 1 ? '1' : '0')
// #define PRINTF_LO(byte) for (int i=3; i>=0; i--) printf("%c", (byte >> i) & 1 ? '1' : '0')
// #define PRINTF_BIN(byte) printf("%02X (", byte); PRINTF_HI(byte); PRINTF_LO(byte); printf(")")

// void WriteStatusEPROM(FILE *fp, StateEPROM *eprom) {
//   fprintf(fp, "A="); PRINTF_BIN(eprom->a);
//   fprintf(fp, " B="); PRINTF_BIN(eprom->b);
//   fprintf(fp, " C="); PRINTF_BIN(eprom->c);
//   fprintf(fp, " @="); PRINTF_BIN(eprom->ctl);
// }

using namespace std;

class IOState {
public:
  int  key_buffer;
  uint8_t led_buffer;
  int  vdu_buffer;
  unsigned int port6_bit_count;
  uint8_t print_byte;
  bool oscillator;
  bool tape_relay;
  int  cursor_position;
  int  tape_status;
  int  uart_status;
  int  vdu_startrow;
  void vdu_strobe(State8080 *state, uint8_t *memory);
  void key_press(sf::Event::EventType event, int key, bool shifted, bool ctrl);
};

const int mem_top_default = 0x2000;

char *tape_file = NULL;
char *user_rom = NULL;

void system_halt(State8080 *state) { // jam HLT instruction and force interrupt service
  state->interrupt = 0x76; state->int_enable = true;
  fprintf(stderr, "System halted (HLT) - press F3 (RESET) to restart, or F9 to exit\n");
}

// Takes input from port 5 buffer (IC 51) and attempts to duplicate
// Thomson-CSF VDU controller (IC 61) interface with video RAM

void IOState::vdu_strobe(State8080 *state, uint8_t *memory) {
  int i;
  int input = vdu_buffer & 0x7f;
  switch(input) {
  case 0x00: break; // NUL
  case 0x04: break; // EOT (End of Text)
  case 0x08: // Backspace
    if (--cursor_position < 0) cursor_position += 1024;
    break;
  case 0x09: // Step cursor RIGHT
    if (++cursor_position >= 1024) cursor_position -= 1024;
    break;
  case 0x0a: // Line feed
    cursor_position += 64;
    if (cursor_position >= 1024) {
      cursor_position -= 64;
      if (++vdu_startrow > 15) vdu_startrow = 0;
      for (i=0; i<64; i++) {
	memory[0x1000 + (((64 * vdu_startrow) + cursor_position + i) % 1024)] = 0x20;
      }
    }
    break;
  case 0x0b: // Step cursor UP
    cursor_position -=64;
    if (cursor_position < 0) cursor_position += 1024;
    break;
  case 0x0c: // Clear screen/reset cursor
    for (i=0; i<1024; i++) memory[0x1000 + i] = 0x20;
    cursor_position = 0;
    vdu_startrow = 0;
    break;
  case 0x0d: // Carriage return / clear line
    if (cursor_position % 64 != 0) {
      while(cursor_position % 64 != 0) {
	memory[0x1000 + (((64 * vdu_startrow) + cursor_position++) % 1024)] = 0x20;
      }
      cursor_position -= 64;
    }
    break;
  case 0x1b: // Screen roll (changes which memory location represents top of screen)
    if (++vdu_startrow > 15) vdu_startrow = 0;
    cursor_position -= 64;
    if (cursor_position < 0) cursor_position += 1024;
    break;
  case 0x1c: // Reset cursor
    cursor_position = 0;
    break;
  case 0x1d: // Carriage return (no clear)
    cursor_position -= (cursor_position % 64);
    break;
  default:
    memory[0x1000 + (((64 * vdu_startrow) + cursor_position) % 1024)] = input;
    if (++cursor_position >= 1024) {
      cursor_position -= 64;
      if (++vdu_startrow > 15) vdu_startrow = 0;
      for (i=0; i<64; i++) {
	memory[0x1000 + (((64 * vdu_startrow) + cursor_position + i) % 1024)] = 0x20;
      }
    }
    break;
  }
}

void IOState::key_press(sf::Event::EventType event, int key, bool shifted, bool ctrl) {
  // Handles keyboard input, placing data in port 0 (IC 49)
  // Assumes PC has UK keyboard - because that's all I have to test it with!
  uint8_t byte = 0xFF;
  if (ctrl == false) {
    switch(key) {
    case sf::Keyboard::Escape: byte = 0x1B; break; // escape
    case sf::Keyboard::Space: byte = 0x20; break; // space
    case sf::Keyboard::Enter: byte = 0x0D; break; // carriage return
    case sf::Keyboard::Backspace: byte = 0x08; break; // backspace
    case sf::Keyboard::Left: byte = 0x08; break; // Ctrl+H
    case sf::Keyboard::Right: byte = 0x09; break; // Ctrl+I
    case sf::Keyboard::Down: byte = 0x0A; break; // Ctrl+J
    case sf::Keyboard::Up: byte = 0x0B; break; // Ctrl+K
    }
    if (shifted == false) { // No shift
      if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) byte = key + 0x61; // Letters A - Z
      if ((key >= sf::Keyboard::Num0) && (key <= sf::Keyboard::Num9)) byte = key + 0x16; // Numbers 0 to 9
      switch (key) {
      case sf::Keyboard::LBracket: byte = 0x5B; break; // left bracket
      case sf::Keyboard::RBracket: byte = 0x5D; break; // right bracket
      case sf::Keyboard::Semicolon: byte = 0x3B; break; // semicolon
      case sf::Keyboard::Comma: byte = 0x2C; break; // comma
      case sf::Keyboard::Period: byte = 0x2E; break; // stop
      case sf::Keyboard::Quote: byte = 0x27; break; // quote
      case sf::Keyboard::Slash: byte = 0x2F; break; // slash
      case sf::Keyboard::Backslash: byte = 0x5C; break; // backslash
      case sf::Keyboard::Equal: byte = 0x3D; break; // equal
      case sf::Keyboard::Hyphen: byte = 0x2D; break; // hyphen
      }
    } else {   // Shift key pressed
      if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) byte = key + 0x41; // Graphic 34-59
      switch (key) {
      case sf::Keyboard::Num0: byte = 0x29; break; // close brace
      case sf::Keyboard::Num1: byte = 0x21; break; // exclamation
      case sf::Keyboard::Num2: byte = 0x22; break; // double quote
      case sf::Keyboard::Num3: byte = 0x23; break; // hash
      case sf::Keyboard::Num4: byte = 0x24; break; // dollar
      case sf::Keyboard::Num5: byte = 0x25; break; // percent
      case sf::Keyboard::Num6: byte = 0x5E; break; // carrat
      case sf::Keyboard::Num7: byte = 0x26; break; // ampusand
      case sf::Keyboard::Num8: byte = 0x2A; break; // asterisk
      case sf::Keyboard::Num9: byte = 0x28; break; // open brace
      case sf::Keyboard::LBracket: byte = 0x7B; break; // graphic 60 - arrow up
      case sf::Keyboard::RBracket: byte = 0x7D; break; // graphic 62 - arrow left
      case sf::Keyboard::Semicolon: byte = 0x3A; break; // colon
      case sf::Keyboard::Comma: byte = 0x3C; break; // less than
      case sf::Keyboard::Period: byte = 0x3E; break; // greater than
      case sf::Keyboard::Quote: byte = 0x40; break; // at
      case sf::Keyboard::Slash: byte = 0x3F; break; // question
      case sf::Keyboard::Backslash: byte = 0x7C; break; // graphic 61 - arrow down
      case sf::Keyboard::Equal: byte = 0x2B; break; // plus
      case sf::Keyboard::Hyphen: byte = 0x5F; break; // underscore
      }
    }
  } else {   // Control characters
    if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) byte = key + 0x01; // CTRL A - Z
    switch (key) {
    case sf::Keyboard::Quote: byte = 0x00; break; // control + at
    case sf::Keyboard::Backslash: byte = 0x1C; break; // control + backslash
    case sf::Keyboard::LBracket: byte = 0x1B; break; // control + left bracket
    case sf::Keyboard::RBracket: byte = 0x1D; break; // control + right bracket
    }
  }
  if (byte != 0xFF) { // check if the key press was recognised
    key_buffer = byte; // set the key buffer
    if (event == sf::Event::KeyPressed) key_buffer |= 0x80; // set the strobe bit
  }
}

// Ports 0xfc to 0xff belong to the Intel 8255 in the EPROM programmer.
// The implementation is not a generic 8255 emulation however.

void MachineInOut(State8080 *state, uint8_t *memory, IOState *io,
		  fstream &tape, StateEPROM *eprom) {
  bool io_error = false;
  switch(state->port) {
  case 0: // Keyboard buffer (IC 49)
    state->a = io->key_buffer;
    break;
  case 1: // Get UART status
    state->a = io->uart_status;
    break;
  case 2: // Output data to tape
    if (io->tape_relay) {
      if (io->tape_status == ' ') {
	if (tape_file != NULL) {
	  tape.open(tape_file, ios::out | ios::app | ios::binary);
	  if (tape.is_open()) io->tape_status = 'w';
	  else { // failed to open file for writing
	    io_error = true; io->tape_relay = false;
	    fprintf(stderr, "Tape interface: unable to open %s for writing\n", tape_file);
	  }
	} else { // tape_file was NULL
	  io_error = true; io->tape_relay = false;
	  fprintf(stderr, "Tape interface: no tape file specified (-t missing)\n");
	}
      }
      if (io->tape_status == 'w') {
	char c;
	c = (char)state->a;
	tape.put(c);
      }
    }
    break;
  case 3: // LED buffer (IC 50)
    io->led_buffer = state->a;
    break;
  case 4: // Input data from tape
    if (io->tape_relay) {
      if (io->tape_status == ' ') {
	if (tape_file != NULL) {
	  tape.open(tape_file, ios::in | ios::binary);
	  if (tape.is_open()) io->tape_status = 'r';
	  else {
	    io_error = true; io->tape_relay = false;
	    fprintf(stderr, "Unable to open tape file %s for reading\n", tape_file);
	  }
	} else {
	  io_error = true; io->tape_relay = false;
	  fprintf(stderr, "Tape interface: no tape file specified (-t missing)\n");
	}
      }
      if ((io->tape_status == 'r') && (tape.eof() == false)) {
	char c;
	tape.get(c);
	state->a = (uint8_t)c;
      } else {
	state->a = 0xff; // return 0xff as bad data
      }
    }
    break;
  case 5: // VDU buffer (IC 51)
    if (io->vdu_buffer != state->a) {
      io->vdu_buffer = state->a;
      if (state->a >= 0x80) io->vdu_strobe(state, memory);
    }
    break;
  case 6: // port 6 latches (IC 52) -- printer emulation
    uint8_t byte;
    byte = state->a & 0x80; // keep only bit 8 of the output
    if (io->port6_bit_count == 0) {
      if (byte == 0x80) { // start bit
	io->print_byte = 0x00; // keep track of bit-banged output
	io->port6_bit_count = 1;
      }
    } else {
      if (io->port6_bit_count < 9) { // seven data bits, with eighth (fake parity) bit always set
	io->print_byte = (io->print_byte >> 1) | byte;
	io->port6_bit_count++;
      } else { // stop bit - process captured output to ASCII character
	byte = ~io->print_byte; // complement; fake parity bit is now unset
	printf("%c", (char)byte); // this is now an ASCII character and can be printed
	io->port6_bit_count = 0; // reset counter and look out for next start bit
      }
    }
    break;
  case 7: // port 7 latches (IC 52) and tape power switch (RLY 1)
    io->oscillator = ((state->a & 0x40) != 0);
    if (((state->a & 0x80) != 0) && (io->tape_relay == false)) io->tape_relay = true;
    if (((state->a & 0x80) == 0) && io->tape_relay) {
      if ((io->tape_status == 'w') || (io->tape_status == 'r')) {
	tape.close();
	io->tape_status = ' ';
      }
      io->tape_relay = false;
    }
    break;
  case 0xfc: // 8255 port A (IN or OUT selected by control word - see below)
    if (state->port_op == 0xd3 && eprom->portA_dirn == OUTPUT) eprom->a = state->a; // output, if port A direction is OUT
    else { // input if port A direction is IN, EPROM CS is enabled, and there is a ROM loaded
      if (eprom->portA_dirn == INPUT && eprom->chip_select && eprom->rom != NULL) {
	uint8_t upper = eprom->c & 0x03; // the least two bits of C are the top two bits of the address
	uint16_t address = ((upper << 8) | (eprom->b)) & 0x03ff; // form the full address from these and B
	eprom->a = eprom->rom[address]; // read from ROM
      } else eprom->a = 0xff; // failed to meet test to read from ROM, return 0xff
      state->a = eprom->a;
    }
    break;
  case 0xfd: // 8255 port B (always OUT)
    if (state->port_op == 0xd3) eprom->b = state->a;
    break;
  case 0xfe: // 8255 port C (lower 4 bits always OUT; upper 4 bits always IN)
    if (state->port_op == 0xd3) { // output
      eprom->c = (eprom->c & 0xf0) | (state->a & 0x0f); // latch only lower 4 bits
      eprom->chip_select = ((eprom->c & 0x0c) == 0x04); // implement the hardware logic that..
      eprom->write_enable = ((eprom->c & 0x0c) == 0x08); // connects C bits 2, 3 to 2708 CS/WE
      // write to EPROM if port A direction is OUT, the EPROM is write-enabled, and there is a EPROM loaded
      if (eprom->portA_dirn == OUTPUT && eprom->write_enable && eprom->rom != NULL) {
	uint8_t upper = eprom->c & 0x03; // the least two bits of C are the top two bits of the address
	uint16_t address = ((upper << 8) | (eprom->b)) & 0x03ff; // form the full address from these and B
	eprom->rom[address] &= eprom->a; // can only _unset_ bits, 1 --> 0, hence '&='
	eprom->write_count[address]++; // increment the write count for that memory location
	eprom->c &= 0xef; // clear the high bit in C to show successful write sequence
      }
    } else { // input
      state->a = (eprom->c & 0xf0) & 0x0f; // just read the upper 4 bits
    }
    break;
  case 0xff: // 8255 control word; bit 4 (& 0x10) sets the direction of port A
    if (state->port_op == 0xd3) {
      eprom->ctl = state->a;
      eprom->portA_dirn = (eprom->ctl & 0x10) == 0x00 ? OUTPUT : INPUT;
    }
    break;
  }
  //  if (state->port > 0xfb) {
  //    printf("8255: %02X ", state->a);
  //   if (state->port_op == 0xd3) printf("-->");
  //   else printf("<--");
  //   printf(" port ");
  //   switch (state->port) {
  //   case 0xfc: printf("A"); break;
  //   case 0xfd: printf("B"); break;
  //   case 0xfe: printf("C"); break;
  //   case 0xff: printf("@"); break;
  //   }
  //   printf(" | "); WriteStatusEPROM(stdout, eprom);
  //   printf(" | "); WriteStatus8080(stdout, state); printf("\n");
  // }
  state->port_op = 0x00;
  if (io_error) system_halt(state);
}

void load_rom(uint8_t *memory, const char *rom_name, uint16_t rom_start, uint16_t rom_size) {
  ifstream rom;
  rom.open(rom_name, ios::in | ios::binary);
  if (rom.is_open()) {
    rom.read((char *) &memory[rom_start], rom_size);
    rom.close();
    fprintf(stderr, "0x%04x-x%04x: %s loaded\n", rom_start, rom_start+rom_size-1, rom_name);
  }
}

// Set all bytes to 0xff and (re)initialise write counts.

void UV_erase(StateEPROM *eprom) {
  int i;
  for (i=0; i<_1K; i++) {
      eprom->rom[i] = 0xff;
      eprom->write_count[i] = 0;
  }
}

// Check to see if any write counts were less than 100.

bool check_write_counts(StateEPROM *eprom) {
  int i;
  for (i=0; i<_1K; i++) {
    if (eprom->write_count[i] < 100) return true;
  }
  return false;
}

int main(int argc, char** argv) {
  uint8_t main_memory[_64K];
  int cursor_count = 0;
  int i;
  IOState io;
  int xpos, ypos;
  uint8_t mask, byte;
  int framerate = 25;
  int ops, ops_per_frame;
  int glyph;
  int vdu_rolloffset;
  bool inFocus = true;
  bool shifted = false;
  bool ctrl = false;
  bool pause = false;
  bool cursor_on = true;
  char *mem_top_opt = NULL;
  char *pend;
  int c;

  fstream tape;

  State8080 state;
  StateEPROM eprom;

  // Shut GetOpt error messages down (return '?'):
  // From the docs: You don’t ordinarily need to copy the optarg
  // string, since it is a pointer into the original argv array, not
  // into a static area that might be overwritten.

  opterr = 0;
  while ((c = getopt(argc, argv, "hm:t:u:z:")) != -1) switch (c) {
    case 'm': mem_top_opt = optarg; break;
    case 't': tape_file = optarg; break;
    case 'u': user_rom = optarg; break;
    case 'z': eprom.file = optarg; break;
    case 'h': case '?':
      printf("SFML-based Triton emulator\n");
      printf("%s -m <mem_top> -t <tape_file> -u <user_rom(s)> -z <user_eprom> -h -?\n", argv[0]);
      printf("-h or -? (help): print this help\n");
      printf("-m sets the top of memory, for example -m 0x4000, defaults to 0x2000\n");
      printf("-t specifies a tape binary, eg -t TAPE\n");
      printf("-u installs user ROM(s); to install two ROMS separate the filenames by a comma\n");
      printf("-z specifies a file to write the EPROM to, with F8\n");
      printf("F1: interrupt 1 (RST 1) - clear screen\n");
      printf("F2: interrupt 2 (RST 2) - save and dump registers\n");
      printf("F3: reset (RST 0)\n");
      printf("F4: halt system (jam HLT instruction using interrupt)\n");
      printf("F5: toggle emulator pause\n");
      printf("F6: write 8080 status to command line\n");
      printf("F7: EPROM programmer: UV erase the EPROM (set all bytes to 0xff)\n");
      printf("F8: EPROM programmer: write the EPROM to the file specified by -z\n");
      printf("F9: exit emulator\n");
    default:
      exit(0);
    }

  // One microcycle is 1.25uS = effective clock rate of 800kHz

  ops_per_frame = 800000 / framerate;

  mem_top = (mem_top_opt == NULL) ? mem_top_default : strtoul(mem_top_opt, &pend, 0);

  sf::Int16 wave[11025]; // Quarter of a second at 44.1kHz

  const double increment = 1000./44100;
  double x = 0;

  for (i=0; i<11025; i++) {
    wave[i] = 10000 * sin(x * 6.28318);
    x += increment;
  }

  sf::SoundBuffer Buffer;
  Buffer.loadFromSamples(wave, 11025, 1, 44100);
  sf::Sound beep;
  beep.setBuffer(Buffer);
  beep.setLoop(true);

  io.vdu_startrow = 0;

  io.uart_status = 0x11;
  io.tape_status = ' ';

  io.print_byte = 0x00;
  io.port6_bit_count = 0;

  // Initialise memory to 0xFF then load ROMs

  for (i=0; i<_64K; i++) main_memory[i] = 0xff;

  load_rom(main_memory, "MONA72_ROM",  0x0000, _1K);
  load_rom(main_memory, "MONB72_ROM",  0x0c00, _1K);
  load_rom(main_memory, "TRAP_ROM",    0xc000, _8K);
  load_rom(main_memory, "BASIC72_ROM", 0xe000, _8K);

  if (user_rom != NULL) {
    if (char *s = strchr(user_rom, ',')) { // check for a comma
      s[0] = '\0'; // if found, truncate the user_rom string at the comma
      load_rom(main_memory, ++s, 0x0800, _1K); // use the remainder to load the second user ROM
    }
    load_rom(main_memory, user_rom, 0x0400, _1K); // load the first user ROM
  }

  UV_erase(&eprom);

  if (eprom.file != NULL) load_rom(eprom.rom, eprom.file, 0x0000, _1K);

  Reset8080(&state);

  // Initialise window

  sf::RenderWindow window(sf::VideoMode(512, 414), "Transam Triton");
  window.setFramerateLimit(framerate);
  sf::Texture fontmap;
  if (!fontmap.loadFromFile("font.png")) {
    fprintf(stderr, "Error loading font file\n");
    exit(1);
  }
  sf::Texture tapemap;
  if (!tapemap.loadFromFile("tape.png")) {
    fprintf(stderr, "Error loading tape image\n");
    exit(1);
  }
  sf::Sprite sprite[1024];
  sf::CircleShape led[8];
  sf::Color ledoff = sf::Color(100,0,0);
  sf::Color ledon = sf::Color(250,0,0);
  sf::Sprite tape_indicator;
  sf::RectangleShape cursor(sf::Vector2f(8.0f, 2.0f));
  for (i=0; i<1024; i++) {
    sprite[i].setTexture(fontmap);
    ypos = ((i - (i % 8)) / 64) * 24;
    xpos = (i % 64) * 8;
    sprite[i].setPosition(sf::Vector2f((float) xpos,(float) ypos));
  }
  for (i=0; i<8; i++) {
    led[i].setRadius(7.0f);
    led[i].setPosition(15.0f + (i * 15), 396.0f);
  }
  tape_indicator.setTexture(tapemap);
  tape_indicator.setPosition(sf::Vector2f(462.0f, 386.0f));

  while (window.isOpen()) {
    sf::Event event;
    while (window.pollEvent(event)) {
      // Close application on request
      if (event.type == sf::Event::Closed) window.close();
      // Don't react to keyboard input when not in focus
      if (event.type == sf::Event::LostFocus) inFocus = false;
      if (event.type == sf::Event::GainedFocus) inFocus = true;
      // Keep track of shift and control keys
      if (event.type == sf::Event::KeyPressed) {
        if ((event.key.code == sf::Keyboard::LShift) || (event.key.code == sf::Keyboard::RShift)) shifted = true;
        if ((event.key.code == sf::Keyboard::LControl) || (event.key.code == sf::Keyboard::RControl)) ctrl = true;
      }
      if (event.type == sf::Event::KeyReleased) {
        if ((event.key.code == sf::Keyboard::LShift) || (event.key.code == sf::Keyboard::RShift)) shifted = false;
        if ((event.key.code == sf::Keyboard::LControl) || (event.key.code == sf::Keyboard::RControl)) ctrl = false;
      }
      if (inFocus) { // Respond to key presses
        if (event.type == sf::Event::KeyPressed) {
          switch(event.key.code) {
	  case sf::Keyboard::F1: // jam RST 1 instruction (clear screen)
	    state.interrupt = 0xcf;
	    break;
	  case sf::Keyboard::F2: // jam RST 2 instruction (print registers and flags)
	    state.interrupt = 0xd7;
	    break;
	  case sf::Keyboard::F3: // Perform a hardware reset
	    Reset8080(&state);
	    break;
	  case sf::Keyboard::F4: // Halt system by jamming HLT as an interrupt
	    system_halt(&state);
	    break;
	  case sf::Keyboard::F5: // Toggle emulator pause
	    pause = !pause;
	    if (pause) fprintf(stderr, "Emulation paused - press F5 to resume, or F9 to exit\n");
	    else fprintf(stderr, "Emulation resumed\n");
	    break;
	  case sf::Keyboard::F6: // Write status
	    WriteStatus8080(stderr, &state); fprintf(stderr, "\n");
	    break;
	  case sf::Keyboard::F7: // UV erase EPROM
	    UV_erase(&eprom);
	    fprintf(stderr, "EPROM programmer: UV erased EPROM\n");
	    break;
	  case sf::Keyboard::F8: // Save EPROM to file
	    if (eprom.file != NULL) {
	      fstream fs;
	      fs.open(eprom.file, ios::out | ios::binary);
	      if (fs.is_open()) {
		fs.write((char *)eprom.rom, _1K);
		fs.close();
		fprintf(stderr, "EPROM programmer: saved EPROM to %s\n", eprom.file);
		if (check_write_counts(&eprom)) {
		  fprintf(stderr, "EPROM programmer: one or more write counts < 100\n");
		}
	      }	else fprintf(stderr, "EPROM programmer: file %s could not be opened for writing\n", eprom.file);
	    } else fprintf(stderr, "EPROM programmer: no file specified (-z missing)\n");
	    break;
	  case sf::Keyboard::F9: // Exit emulator
	    window.close();
	    break;
	  default:
	    io.key_press(event.type, event.key.code, shifted, ctrl);
	    break;
	  }
	}
	if (event.type == sf::Event::KeyReleased) {
	  io.key_press(event.type, event.key.code, shifted, ctrl);
	}
      }
    }

    if (pause) beep.pause();
    else {
      // Send as many clock pulses to the CPU as would happen between screen frames
      for (ops = 0; ops < ops_per_frame; ops += SingleStep8080(&state, main_memory)) {
	if (state.halted) break;
	if (state.port_op) MachineInOut(&state, main_memory, &io, tape, &eprom);
      }
      cursor_count++;
      // Draw screen from VDU memory - font texture acts as ROMs (IC 69 and 70)
      window.clear();
      vdu_rolloffset = 64 * io.vdu_startrow;
      for (i=0; i<1024; i++) {
        glyph = main_memory[0x1000 + ((vdu_rolloffset + i) % 1024)] & 0x7f;
        xpos = (glyph % 16) * 8;
        ypos = ((glyph / 16) * 24);
        sprite[i].setTextureRect(sf::IntRect(xpos, ypos, 8, 24));
        window.draw(sprite[i]);
      }
      for (i=0, mask=0x80; i<8; i++) {
	byte = io.led_buffer & mask; mask = mask >> 1;
	led[i].setFillColor(byte == 0x00 ? ledon : ledoff); // note LEDs are on for "0"
        window.draw(led[i]);
      }
      if (io.tape_relay == false) {
	tape_indicator.setTextureRect(sf::IntRect(0, 0, 45, 30));
      } else {
        switch (io.tape_status) {
	case ' ':
	  tape_indicator.setTextureRect(sf::IntRect(45, 0, 45, 30));
	  break;
	case 'r':
	  tape_indicator.setTextureRect(sf::IntRect(90, 0, 45, 30));
	  break;
	case 'w':
	  tape_indicator.setTextureRect(sf::IntRect(135, 0, 45, 30));
	  break;
	}
      }
      window.draw(tape_indicator);
      if (cursor_count > (framerate / 2)) {
        if (cursor_on) {
          cursor.setFillColor(sf::Color(0, 0, 0));
          cursor_on = false;
	} else {
          cursor.setFillColor(sf::Color(255, 255, 255));
          cursor_on = true;
	}
        cursor_count = 0;
      }
      i = io.cursor_position;
      ypos = (((i - (i % 8)) / 64) * 24) + 18;
      xpos = (i % 64) * 8;
      cursor.setPosition(sf::Vector2f((float) xpos,(float) ypos));
      window.draw(cursor);
      window.display();
      // if (io.oscillator) beep.play(); else beep.pause();
      io.oscillator ? beep.play() : beep.pause();
    }
  }
  return 0;
}
