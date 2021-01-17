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

default: all

all: codes roms

codes: trimcc tridat

tridat : tridat.c
	gcc -O -Wall tridat.c -o tridat

trimcc : trimcc.c
	gcc -O -Wall trimcc.c -o trimcc

roms:
	./trimcc L72_0000-03ff.tri -o MONA72.ROM
	./trimcc L72_0c00-0fff.tri -o MONB72.ROM
	./trimcc L72_e000-ffff.tri -o BASIC72.ROM

clean : 
	rm -f *~ *.o

pristine: clean
	rm -f  tridat trimcc
	rm -f MONA72.ROM MONB72.ROM BASIC72.ROM
