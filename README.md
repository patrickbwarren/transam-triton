## Transam Triton 

The
[Triton](https://sites.google.com/site/patrickbwarren/electronics/transam-triton)
was an [8080-based](https://en.wikipedia.org/wiki/Intel_8080)
microcomputer released in late 1978 by Transam Components Ltd, a
British company based at 12 Chapel Street, off the Edgeware Road in
north London.  Some basic information can be found in the online
[Centre for Computing History](http://www.computinghistory.org.uk/).
Recently a [YouTube
video](https://www.youtube.com/watch?v=0cSRgJ68_tM) has appeared,
another [web site](https://sites.google.com/view/transam-triton/) has
sprung up, and a Facebook group ('ETI Triton 8080 Vintage Computer')
has been started.  I have a small web page detailing some of the
history
[here](https://sites.google.com/site/patrickbwarren/electronics/transam-triton).
Much documentation can be found on the Facebook group, and the
original Triton articles in [Electronics Today
International](https://en.wikipedia.org/wiki/Electronics_Today_International)
(ETI) can be found
[here](https://worldradiohistory.com/ETI_Magazine.htm)

Robin Stuart has posted some of the original Triton documentation and
code for a superb Triton emulator that uses the [SFML
library](https://www.sfml-dev.org/) on a [GitHub
site](https://github.com/woo-j/triton).  A fork of this emulator is
included in the present repository, to which a few small improvements
have been made to reflect better the actual hardware and some features
have been added such as printing and an emulation of the EPROM
programmer.

On my Triton, the tape cassette interface was hacked (ca 1995) to
drive an [RS-232](https://en.wikipedia.org/wiki/RS-232) interface.  To
manage this, a serial data receiver (`tridat.c`) and transmitter
(`trimcc.c`) were written. The transmitter implements a hybrid
assembly / machine code TriMCC 'minilanguage' which it converts into
binary to send to a physically connected TRITON through a serial
interface; the byte stream can also be saved to a file to be used with
the emulator.  To partner this, `disasm8080.py` is included.  This is
a simplified version of an 8080 disassembler written by Jeff Tranter,
available on his [GitHub site](https://github.com/jefftranter/8080).

- [EMULATOR.md](EMULATOR.md) - more details of the emulator fork,
  and the Triton Level 7.2 code.
- [TRIMCC.md](TRIMCC.md) - more details of the TriMCC codes and disassembler.

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

The Triton Level 7.2 Monitor and BASIC here are the originals,
extracted from the ROMs on my own personal machine a number of years
ago now, and are copyright &copy; Transam Components Limited and/or
T.C.L. software (1979) or thereabouts.

The version of TRAP included here and the original fast VDU code come
from Gerald Sommariva's [web
site](https://sites.google.com/view/transam-triton/home), as
`TRAP.F86B` and `FASTVDU.ED82` on the
[downloads](https://sites.google.com/view/transam-triton/downloads)
page.  Modifications to the fast VDU code are copyright (c) 2021
Patrick B Warren <patrickbwarren@gmail.com> and are released into the
public domain.

The original SFML-based emulator is copyright &copy; 2020 Robin Stuart
<rstuart114@gmail.com>, with modifications copyright &copy; 2021
Patrick B Warren <patrickbwarren@gmail.com>.

The 8080 disassembler `disasm8080.py` is released under an Apache
License, Version 2.0 and is copyright &copy; 2013-2015 by Jeff Tranter
<tranter@pobox.com>, with modifications copyright &copy; 2021
Patrick B Warren <patrickbwarren@gmail.com>.

The file [`invaders_tape.tri`](invaders_tape.tri) is modified from a
hex dump in Computing Today (March 1980; page 32).  Copyright &copy; is
claimed in the magazine (page 3) but the copyright holder is not
identified.  No license terms were given and the code is hereby
presumed to be reproducible and modifiable under the equivalent of a
modern open source license.  The changes made to this code are
copyright &copy; 2021 Patrick B Warren and are released into the public
domain.

All other material here is copyright &copy; 1979-2024 Patrick B Warren
<patrickbwarren@gmail.com>

