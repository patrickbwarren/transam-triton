# This file is part of a demonstrator for Map/Reduce Monte-Carlo
# methods.

# This is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

# Copyright (c) 2021 Patrick B Warren <patrickbwarren@gmail.com>.

# You should have received a copy of the GNU General Public License
# along with this file.  If not, see <http://www.gnu.org/licenses/>.

FLAGS = -O2 -Wall
OBJS = 8080.o triton.o
LIBS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

default: all

all: codes roms tapes

codes: triton trimcc tridat

triton: $(OBJS)
	g++ $(FLAGS) -o $@ $^ $(LIBS)

%.o : %.cpp 8080.hpp
	g++ $(FLAGS) -c -o $@ $<

tridat : tridat.c
	gcc -O -Wall tridat.c -o tridat

trimcc : trimcc.c
	gcc -O -Wall trimcc.c -o trimcc

roms:
	./trimcc mona72_rom.tri  -o MONA72_ROM
	./trimcc monb72_rom.tri  -o MONB72_ROM
	./trimcc trap_rom.tri    -o TRAP_ROM
	./trimcc basic72_rom.tri -o BASIC72_ROM
	./trimcc fastvdu_rom.tri -o FASTVDU_ROM

tape:
	./trimcc hex2dec_tape.tri  -o HEX2DEC_TAPE
	./trimcc kbdtest_tape.tri  -o KBDTEST_TAPE
	./trimcc tapeout_tape.tri  -o TAPEOUT_TAPE
	./trimcc rawsave_tape.tri  -o RAWSAVE_TAPE
	./trimcc galaxian_tape.tri -o GALAXIAN_TAPE
	./trimcc invaders_tape.tri -o INVADERS_TAPE
	./trimcc fastvdu_tape.tri  -o FASTVDU_TAPE
	cat *_TAPE > TAPE

clean : 
	rm -f *~ *.o
	rm -f $(OBJS)

pristine: clean
	rm -f *~
	rm -f triton tridat trimcc

rom-clean: 
	rm -f *_ROM
