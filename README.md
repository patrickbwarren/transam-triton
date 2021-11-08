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
another [web site](https://sites.google.com/view/transam-triton/) has sprung up, and
a Facebook group ('ETI Triton 8080 Vintage Computer') has been started.
I have a small web page detailing some of the history
[here](https://sites.google.com/site/patrickbwarren/electronics/transam-triton).

Robin Stuart has posted some of the original Triton
documentation and code for a superb Triton emulator that uses
the [SFML library](https://www.sfml-dev.org/) on a [GitHub
site](https://github.com/woo-j/triton).  A fork of this
emulator is included in the present repository, to which a few small
improvements have been made to reflect better the actual hardware.
Details of the emulator are provided in [EMULATOR.md](EMULATOR.md).

On my Triton, the tape cassette interface was hacked (ca 1995) to
drive an [RS-232](https://en.wikipedia.org/wiki/RS-232) interfaceQ.
To manage this, a serial data receiver (`tridat.c`) and transmitter
(`trimcc.c`) were written. The transmitter implements a hybrid
assembly / machine code 'minilanguage' ,used by the `.tri` codes in
the present repository.  More details are given in
[TRIMCC.md](TRIMCC.md).

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

The file [`invaders_tape.tri`](invaders_tape.tri) is modified from a
hex dump in Computing Today (March 1980; page 32).  Copyright &copy; is
claimed in the magazine (page 3) but the copyright holder is not
identified.  No license terms were given and the code is hereby
presumed to be reproducible and modifiable under the equivalent of a
modern open source license.  The changes made to this code are
copyright &copy; 2021 Patrick B Warren, and released into the public
domain.
