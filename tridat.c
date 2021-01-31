/* This file is part of my Transam Triton code repository.

This is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your
option) any later version.

This is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Copyright (c) 1995-2021 Patrick B Warren <patrickbwarren@gmail.com>.

You should have received a copy of the GNU General Public License
along with this file.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Compile with gcc -O -Wall tridat.c -o tridat */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

int fd;                     /* port id number for fileio*/
FILE *fp = NULL;            /* write data to a file as well */
struct termios oldtio;      /* original port settings */
struct termios newtio;      /* Triton required port settings */
char *port = "/dev/ttyS0";  /* name of port to read data from */
char *tri_ext = ".tri";     /* file name extension to store data */

void startio();
void finishio();
void sighook(int);

void *emalloc(size_t size) {
  void *p;
  if ((p = malloc(size)) == NULL) {
    fprintf(stderr, "error: out of memory in emalloc\n"); exit(1);
  }
  return p;
}

int main(int argc, char *argv[]) {
  int count = 0;
  char *filename = "";
  char *s;
  char c;
  char buf[1];
  signal(SIGINT, sighook); /* catch ctrl-c */
  while ((c = getopt(argc, argv, "ho:")) != -1)
    switch (c) {
    case 'o': filename = strdup(optarg); break;
    case 'h':
      printf("%s [-o file] : receive Triton RS232 data from %s\n",
	     argv[0], port);
      exit(0);
    }
  if (strlen(filename) > 0) { /* set up output to a file */
    if (strchr(filename, '.') == NULL) {
      s = strdup(filename); free(filename);
      filename = (char *)emalloc(strlen(s) + strlen(tri_ext) + 1);
      strcpy(filename, s); free(s);
      strcat(filename, tri_ext);
    }
    if ((fp = fopen(filename, "w")) == NULL) {
      fprintf(stderr, "error: couldn't open %s\n", filename); exit(1);
    }
    printf("Writing data to %s\n",filename);
  }
  printf("Press ctrl-C to exit\n\n");
  startio();
  for (;;) {
    while (read(fd, buf, 1) != 1);
    printf(" %02X", ((int)buf[0]&255));
    if (fp != NULL) fprintf(fp, " %02X", ((int)buf[0]&255));
    if (++count % 16 == 0) {
      printf("\n"); if (fp != NULL) fprintf(fp, "\n");
    }
  }
}

/* Initialise io port settings */
void startio() {
/* Open port for reading, and not as controlling tty */
  if ((fd = open(port, O_RDONLY | O_NOCTTY )) < 0) {
    fprintf(stderr, "error: couldn't open %s for writing\n", port); exit(1);
  }
/* save current serial port settings */
  tcgetattr(fd, &oldtio); 
/* clear struct for new port settings */
  bzero(&newtio, sizeof(newtio)); 
/* Set to 300 baud, 8 bits, odd parity, 2 stop bits, hardware control */
  newtio.c_cflag = B300 | CS8 | CSTOPB | PARENB | PARODD | CRTSCTS | CREAD;
/* Make device raw (no other input processing) */
  newtio.c_iflag = 0;
/* Similarly, raw output */
  newtio.c_oflag = 0;
/* No character interpretation on input */
  newtio.c_lflag = 0;
/* Clean the modem line and activate the settings for the port */
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &newtio);
}

/* Restore port settings */
void finishio() {
 tcsetattr(fd, TCSANOW, &oldtio);
}

/* Handle a SIGINT such as generated by control-C */
void sighook(int sig) {
  finishio();
  if (fp != NULL) { fprintf(fp, "\n"); fclose(fp); }
  printf("\n\nOK, finished\n\n"); 
  exit(0);
}

/* End of file */
