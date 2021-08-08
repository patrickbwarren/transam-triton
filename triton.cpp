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

#define MEM_SIZE 0x10000
#define ASCII_SPACE 0x20

extern uint16_t mem_top; /* declared in 8080.cpp */

using namespace std;

class IOState {
public:
  int  key_buffer;
  uint8_t led_buffer;
  int  vdu_buffer;
  int  print_bit_count;
  uint8_t print_byte;
  bool oscillator;
  bool tape_relay;
  int  cursor_position;
  int  tape_status;
  int  uart_status;
  int  vdu_startrow;
  void vdu_strobe(State8080* state);
  void key_press(sf::Event::EventType event, int key, bool shifted, bool ctrl);
};

const int mem_top_default = 0x2000;
const char *tape_file_default = "TAPE";

char *tape_file = NULL;
char *user_rom = NULL;

void IOState::vdu_strobe(State8080* state) {
  // Takes input from port 5 buffer (IC 51) and attempts to duplicate
  // Thomson-CSF VDU controller (IC 61) interface with video RAM
  int i;
  int input = vdu_buffer & 0x7f;
  switch(input) {
  case 0x00:
    // NUL
    break;
  case 0x04:
    // EOT (End of Text)
    break;
  case 0x08:
    // Backspace
    cursor_position--;
    if (cursor_position < 0) cursor_position += 1024;
    break;
  case 0x09:
    // Step cursor RIGHT
    cursor_position++;
    if (cursor_position >= 1024) cursor_position -= 1024;
    break;
  case 0x0a:
    // Line feed
    cursor_position += 64;
    if (cursor_position >= 1024) {
      cursor_position -= 64;
      vdu_startrow++;
      if (vdu_startrow > 15) vdu_startrow = 0;
      for (i = 0; i < 64; i++) {
	state->memory[0x1000 + (((64 * vdu_startrow) + cursor_position + i) % 1024)] = ASCII_SPACE;
      }
    }
    break;
  case 0x0b:
    // Step cursor UP
    cursor_position -=64;
    if (cursor_position < 0) cursor_position += 1024;
    break;
  case 0x0c:
    // Clear screen/reset cursor
    for (i = 0; i < 1024; i++) state->memory[0x1000 + i] = ASCII_SPACE;
    cursor_position = 0;
    vdu_startrow = 0;
    break;
  case 0x0d:
    // Carriage return / clear line
    if (cursor_position % 64 != 0) {
      while(cursor_position % 64 != 0) {
	state->memory[0x1000 + (((64 * vdu_startrow) + cursor_position) % 1024)] = ASCII_SPACE;
	cursor_position++;
      }
      cursor_position -= 64;
    }
    break;
  case 0x1b:
    // Screen roll (changes which memory location represents top of screen)
    vdu_startrow++;
    if (vdu_startrow > 15) vdu_startrow = 0;
    cursor_position -= 64;
    if (cursor_position < 0) cursor_position += 1024;
    break;
  case 0x1c:
    // Reset cursor
    cursor_position = 0;
    break;
  case 0x1d:
    // Carriage return (no clear)
    cursor_position -= (cursor_position % 64);
    break;
  default:
    state->memory[0x1000 + (((64 * vdu_startrow) + cursor_position) % 1024)] = input;
    cursor_position++;
    if (cursor_position >= 1024) {
      cursor_position -= 64;
      vdu_startrow++;
      if (vdu_startrow > 15) vdu_startrow = 0;
      for (i = 0; i < 64; i++) {
	state->memory[0x1000 + (((64 * vdu_startrow) + cursor_position + i) % 1024)] = ASCII_SPACE;
      }
    }
    break;
  }
}

void IOState::key_press(sf::Event::EventType event, int key, bool shifted, bool ctrl) {
  // Handles keyboard input, placing data in port 0 (IC 49)
  // Assumes PC has UK keyboard - because that's all I have to test it with!
  int temp = 0xFF;
  if (ctrl == false) {
    switch(key) {
    case sf::Keyboard::Escape:
      temp = 0x1B;
      break; // escape
    case sf::Keyboard::Space:
      temp = 0x20;
      break; // space
    case sf::Keyboard::Enter:
      temp = 0x0D;
      break; // carriage return
    case sf::Keyboard::Backspace:
      temp = 0x08;
      break; // backspace
    case sf::Keyboard::Left:
      temp = 0x08;
      break; // Ctrl+H
    case sf::Keyboard::Right:
      temp = 0x09;
      break; // Ctrl+I
    case sf::Keyboard::Down:
      temp = 0x0A;
      break; // Ctrl+J
    case sf::Keyboard::Up:
      temp = 0x0B;
      break; // Ctrl+K
    }
    if (shifted == false) { // No shift
      if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) temp = key + 0x61; // Letters A - Z
      if ((key >= sf::Keyboard::Num0) && (key <= sf::Keyboard::Num9)) temp = key + 0x16; // Numbers 0 to 9
      switch (key) {
      case sf::Keyboard::LBracket:
	temp = 0x5B;
	break; // left bracket
      case sf::Keyboard::RBracket:
	temp = 0x5D;
	break; // right bracket
      case sf::Keyboard::Semicolon:
	temp = 0x3B;
	break; // semicolon
      case sf::Keyboard::Comma:
	temp = 0x2C;
	break; // comma
      case sf::Keyboard::Period:
	temp = 0x2E;
	break; // stop
      case sf::Keyboard::Quote:
	temp = 0x27;
	break; // quote
      case sf::Keyboard::Slash:
	temp = 0x2F;
	break; // slash
      case sf::Keyboard::Backslash:
	temp = 0x5C;
	break; // backslash
      case sf::Keyboard::Equal:
	temp = 0x3D;
	break; // equal
      case sf::Keyboard::Hyphen:
	temp = 0x2D;
	break; // hyphen
      }
    } else {   // Shift key pressed
      if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) temp = key + 0x41; // Graphic 34-59
      switch (key) {
      case sf::Keyboard::Num0:
	temp = 0x29;
	break; // close brace
      case sf::Keyboard::Num1:
	temp = 0x21;
	break; // exclamation
      case sf::Keyboard::Num2:
	temp = 0x22;
	break; // double quote
      case sf::Keyboard::Num3:
	temp = 0x23;
	break; // hash
      case sf::Keyboard::Num4:
	temp = 0x24;
	break; // dollar
      case sf::Keyboard::Num5:
	temp = 0x25;
	break; // percent
      case sf::Keyboard::Num6:
	temp = 0x5E;
	break; // carrat
      case sf::Keyboard::Num7:
	temp = 0x26;
	break; // ampusand
      case sf::Keyboard::Num8:
	temp = 0x2A;
	break; // asterisk
      case sf::Keyboard::Num9:
	temp = 0x28;
	break; // open brace
      case sf::Keyboard::LBracket:
	temp = 0x7B;
	break; // graphic 60 - arrow up
      case sf::Keyboard::RBracket:
	temp = 0x7D;
	break; // graphic 62 - arrow left
      case sf::Keyboard::Semicolon:
	temp = 0x3A;
	break; // colon
      case sf::Keyboard::Comma:
	temp = 0x3C;
	break; // less than
      case sf::Keyboard::Period:
	temp = 0x3E;
	break; // greater than
      case sf::Keyboard::Quote:
	temp = 0x40;
	break; // at
      case sf::Keyboard::Slash:
	temp = 0x3F;
	break; // question
      case sf::Keyboard::Backslash:
	temp = 0x7C;
	break; // graphic 61 - arrow down
      case sf::Keyboard::Equal:
	temp = 0x2B;
	break; // plus
      case sf::Keyboard::Hyphen:
	temp = 0x5F;
	break; // underscore
      }
    }
  } else {   // Control characters
    if ((key >= sf::Keyboard::A) && (key <= sf::Keyboard::Z)) temp = key + 0x01; // CTRL A - Z
    switch (key) {
    case sf::Keyboard::Quote:
      temp = 0x00;
      break; // control + at
    case sf::Keyboard::Backslash:
      temp = 0x1C;
      break; // control + backslash
    case sf::Keyboard::LBracket:
      temp = 0x1B;
      break; // control + left bracket
    case sf::Keyboard::RBracket:
      temp = 0x1D;
      break; // control + right bracket
    }
  }
  if (temp != 0xFF) { // check if the key press was recognised
    key_buffer = temp; // set the key buffer
    if (event == sf::Event::KeyPressed) key_buffer |= 0x80; // set the strobe bit
  }
}

void MachineInOut(State8080* state, IOState* io, fstream &tape) {
  if (state->port_in) { // input
    state->port_in = false;
    switch(state->port) {
    case 0:
      // Keyboard buffer (IC 49)
      state->a = io->key_buffer;
      // printf("%02X\n", io->key_buffer); // for debugging
      break;
    case 1:
      // Get UART status
      state->a = io->uart_status;
      break;
    case 4:
      // Input data from tape
      if (io->tape_relay) {
	if (io->tape_status == ' ') {
	  tape.open(tape_file, ios::in | ios::binary);
	  if (tape.is_open()) {
	    // printf("Opened tape file " << tape_file << " for reading\n");
	    io->tape_status = 'r';
	  } else {
	    fprintf(stderr, "Unable to open tape file %s for reading\n", tape_file);
	    io->tape_relay = false;
	  }
	}
	if ((io->tape_status == 'r') && (tape.eof() == false)) {
	  char c;
	  tape.get(c);
	  state->a = (uint8_t)c;
	} else state->a = 0x00;
      }
      break;
    }
  } else if (state->port_out) { // output
    state->port_out = false;
    switch(state->port) {
    case 2:
      // Output data to tape
      if (io->tape_relay) {
	if (io->tape_status == ' ') {
	  tape.open(tape_file, ios::out | ios::app | ios::binary);
	  if (tape.is_open()) {
	    io->tape_status = 'w';
	  } else {
	    fprintf(stderr, "Unable to open tape file %s for writing\n", tape_file);
	  }
	}
	if (io->tape_status == 'w') {
	  char c;
	  c = (char)state->a;
	  tape.put(c);
	}
      }
      break;
    case 3:
      // LED buffer (IC 50)
      // printf("LED port: %02X\n", state->a);
      io->led_buffer = state->a;
      break;
    case 5:
      // VDU buffer (IC 51)
      if (io->vdu_buffer != state->a) {
	io->vdu_buffer = state->a;
	if (state->a >= 0x80) io->vdu_strobe(state);
      }
      break;
    case 6:
      // Port 6 latches (IC 52)
      uint8_t byte;
      byte = state->a & 0x80;
      if (io->print_bit_count == 0) {
	if (byte == 0x80) { // start bit
	  io->print_byte = 0x00;
	  io->print_bit_count = 1;
	}
      } else {
	if (io->print_bit_count < 8) { // reading data
	  io->print_byte = (io->print_byte | byte) >> 1; // OR, and shift right 
	  io->print_bit_count++;
	} else { // stop bit - this is a single output byte but delayed 
	  byte = ~io->print_byte & 0x7f; // complement and drop bit 8
	  printf("%c", (char)(byte)); // this is now a character and can be printed
	  io->print_bit_count = 0;
	}
      }
      break;
    case 7:
      // Port 7 latches (IC 52) and tape power switch (RLY 1)
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
    }
  }
}

void load_rom(uint8_t *memory, const char *rom_name, uint16_t rom_start, uint16_t rom_size) {
  ifstream rom;
  rom.open(rom_name, ios::in | ios::binary);
  if (rom.is_open()) {
    rom.read ((char *) &memory[rom_start], rom_size);
    rom.close();
  } else {
    fprintf(stderr, "Unable to load %s\n", rom_name);
    exit(1);
  }
  printf("%04X - %04X : %s\n", rom_start, rom_start+rom_size-1, rom_name);
}

int hardware_interrupt(State8080 *state, uint8_t opcode) {
  int cycles;
  state->int_enable = false; // disable interrupts after a hardware interrupt
  switch (opcode) {
  case 0xc7: // handle only the RST opcodes
  case 0xcf:
  case 0xd7:
  case 0xdf:
  case 0xe7:
  case 0xef:
  case 0xf7:
  case 0xff:
    set_memory(state, state->sp - 2, state->pc & 0xff);
    set_memory(state, state->sp - 1, state->pc >> 8);
    state->sp -= 2;
    state->pc = (int) opcode & 0x38;
    cycles = 11;
    break;
  default:
    fprintf(stderr, "Error: hardware interrupt with unimplemented opcode %02X\n", opcode);
  }
  // printf("hardware_interrupt: opcode %02X, PC = %04X\n", opcode, state->pc);
  return cycles;
}

int main(int argc, char** argv) {
  uint8_t main_memory[MEM_SIZE];
  int time;
  int running_time;
  int cursor_count = 0;
  int i;
  IOState io;
  int xpos, ypos;
  uint8_t mask, byte;
  int framerate = 25;
  int operations_per_frame;
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

  // Shut GetOpt error messages down (return '?'):
  // From the docs: You don’t ordinarily need to copy the optarg
  // string, since it is a pointer into the original argv array, not
  // into a static area that might be overwritten.

  opterr = 0;
  while ((c = getopt (argc, argv, "hm:t:u:")) != -1) switch (c) {
    case 'u':
      user_rom = optarg;
      break;
    case 'm':
      mem_top_opt = optarg;
      break;
    case 't':
      tape_file = optarg;
      break;
    case 'h':
    case '?':
      printf("Usage: %s -m <mem_top> -t <tape_file> -u <user_rom>\n", argv[0]);
    default:
      exit(0);
    }

  if (tape_file == NULL) tape_file = strdup(tape_file_default);

  // One microcycle is 1.25uS = effective clock rate of 800kHz

  operations_per_frame = 800000 / framerate;

  State8080 state;
  fstream tape;

  mem_top = (mem_top_opt == NULL) ? mem_top_default : strtoul(mem_top_opt, &pend, 0);

  sf::Int16 wave[11025]; // Quarter of a second at 44.1kHz

  const double increment = 1000./44100;
  double x = 0;

  for (i = 0; i < 11025; i++) {
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
  io.print_bit_count = 0;

  // Initialise memory to 0xFF
  for (i=0; i<MEM_SIZE; i++) main_memory[i] = 0xff;

  load_rom(main_memory, "MONA72.ROM", 0x0000, 0x400);
  if (user_rom != NULL) load_rom(main_memory, user_rom, 0x0400, 0x400);
  load_rom(main_memory, "MONB72.ROM", 0x0c00, 0x400);
  load_rom(main_memory, "TRAP.ROM", 0xc000, 0x2000);
  load_rom(main_memory, "BASIC72.ROM", 0xe000, 0x2000);

  state.memory = main_memory;
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
  sf::Color ledoff = sf::Color(150,0,0);
  sf::Color ledon = sf::Color(250,0,0);
  sf::Sprite tape_indicator;
  sf::RectangleShape cursor(sf::Vector2f(8.0f, 2.0f));
  for (i = 0; i < 1024; i++) {
    sprite[i].setTexture(fontmap);
    ypos = ((i - (i % 8)) / 64) * 24;
    xpos = (i % 64) * 8;
    sprite[i].setPosition(sf::Vector2f((float) xpos,(float) ypos));
  }
  for (i = 0; i < 8; i++) {
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
      if (inFocus) {
        // Respond to key presses
        if (event.type == sf::Event::KeyPressed) {
          switch(event.key.code) {
	  case sf::Keyboard::F1: // jam RST 1 instruction (clear screen)
	    if (state.int_enable) hardware_interrupt(&state, 0xcf);
	    // state.interrupt = 0xcf;
	    break;
	  case sf::Keyboard::F2: // jam RST 2 instruction (print registers and flags)
	    if (state.int_enable) hardware_interrupt(&state, 0xd7);
	    // state.interrupt = 0xd7;
	    break;
	  case sf::Keyboard::F3: // Perform a hardware reset
	    Reset8080(&state);
	    break;
	  case sf::Keyboard::F4: // Toggle pause
	    pause = !pause;
	    break;
	  case sf::Keyboard::F5: // Print status
	    printStatus(stdout, &state);
	    break;
	  case sf::Keyboard::F9: // Exit application
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
      // Send as many clock pulses to the CPU as would happen
      // between screen frames
      running_time = 0;
      while (running_time < operations_per_frame) {
	time = SingleStep8080(&state);
	if (state.port_in | state.port_out) MachineInOut(&state, &io, tape);
        running_time += time;
      }
      cursor_count++;

      // Draw screen from VDU memory
      // Font texture acts as ROMs (IC 69 and 70)
      window.clear();
      vdu_rolloffset = 64 * io.vdu_startrow;
      for (i = 0; i < 1024; i++) {
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
      if (io.oscillator) beep.play();
      else beep.pause();
    }
  }
  return 0;
}
