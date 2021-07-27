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

/* Compile with gcc -O -Wall trimcc.c -o trimcc */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#define MAXTOK 200    /* Max token length (note include strings) */
#define MAXREG 10     /* Max register (pair) name length */
#define MAXNNV 200    /* Max # of name, value pairs */
#define MAXRPT 0x1000 /* Max repeat value for error trapping */
#define NMN    78     /* # mnemonic codes */
#define DELAY  10     /* Delay in ms after a byte transmitted */
#define NOVAL -1      /* Signals no value assigned in name, value pair */
#define MAXFP 5       /* Max level of file inclusion */

int nparse = 0;       /* count number of times have parsed file */
int transmit = 0;     /* Signals intention to transmit */
int verbose = 0;      /* Verbosity in reporting results */
int zcount;           /* Keeps track of number of bytes printed out */
int extra_space = 0;  /* Whether to print a space after the 8th byte */
char cc;              /* Keeps track of the current character */

int origin, end_prog; /* Position variables */
int byte_count;       /* Byte counter */

FILE *fsp = NULL;     /* File pointer for save binary data */

int fd;                     /* port id number */
struct termios oldtio;      /* original port settings */
struct termios newtio;      /* Triton required port settings */
char *port = "/dev/ttyS0";  /* name of port */
char *tri_ext = ".tri";     /* file name extension (source files) */

/* 8080 mnemonic codes follow: see 8080 bugbook */
/* Note that 'CC' has been replaced by 'CCC' to avoid a clash with 0xCC */

char *mnemonic[NMN] =
{ "ACI",  "ADC",  "ADD",  "ADI",  "ANA",  "ANI",  "CALL", "CCC",  "CM",
  "CMA",  "CMC",  "CMP",  "CNC",  "CNZ",  "CP",   "CPE",  "CPI",  "CPO",
  "CZ",   "DAA",  "DAD",  "DCR",  "DCX",  "DI",   "EI",   "HLT",  "IN",
  "INR",  "INX",  "JC",   "JM",   "JMP",  "JNC",  "JNZ",  "JP",   "JPE",
  "JPO",  "JZ",   "LDA",  "LDAX", "LHLD", "LXI",  "MVI",  "MOV",  "NOP",
  "ORA",  "ORI",  "OUT",  "PCHL", "POP",  "PUSH", "RAL",  "RAR",  "RC",
  "RET",  "RLC",  "RM",   "RNC",  "RNZ",  "RP",   "RPE",  "RPO",  "RRC",
  "RST",  "RZ",   "SBB",  "SBI",  "SHLD", "SPHL", "STA",  "STAX", "STC",
  "SUB",  "SUI",  "XCHG", "XRA",  "XRI",  "XTHL" };

char *mncode[NMN] =
{ "316", "21S", "20S", "306", "24S", "346", "315", "334", "374",
  "057", "077", "27S", "324", "304", "364", "354", "376", "344",
  "314", "047", "0V1", "0D5", "0V3", "363", "373", "166", "333",
  "0D4", "0U3", "332", "372", "303", "322", "302", "362", "352",
  "342", "312", "072", "0V2", "052", "0U1", "0D6", "1DS", "000",
  "26S", "366", "323", "351", "3U1", "3U5", "027", "037", "330",
  "311", "007", "370", "320", "300", "360", "350", "340", "017",
  "3N7", "310", "23S", "336", "042", "371", "062", "0U2", "067",
  "22S", "326", "353", "25S", "356", "343" };

int mntype[NMN], mnval[NMN];

/* Storage for name, value pairs */
int nnv = 0;
int value[MAXNNV];
char *name[MAXNNV];

/* Function prototypes */

void parse(char *);
int regin(FILE *);
int pairin(FILE *);
int rstnin(FILE *);
void zinit();
void mninit();
void word_out(int);
void byte_out(int);
int nvlistok();
void printnvlist();
int tokval(char *);
int addval(char *, int);
void newnv(char *, int);
int eval(char *);
int split(char *, char *, char);
int myscmp(char *, char *);
int tokin(char *, FILE *, int);
int whitespace(char);
void startio();
void finishio();
int ctoi(char c) { return (int)c - '0'; }

void warn(char *s) {
  fprintf(stderr, "warning: %s\n", s);
}

void error(char *s) {
  fprintf(stderr, "error: %s\n", s); exit(1);
}

void fprint_help(FILE *fp, char *name) {
  fprintf(fp, "Usage: %s [-v] [-t] [-s] [-o tapefile] srcfile\n", name);
  fprintf(fp, "-v (verbose): print the byte stream and variables\n");
  fprintf(fp, "-t (transmit): attemp to transmit to %s\n", port);
  fprintf(fp, "-s (spaced): add a column of spaces after the 7th byte\n");
  fprintf(fp, "-o <file>: write the byte stream in binary to a file\n");
  fprintf(fp, "-o pipe: write the byte stream in binary to stdout\n");
  exit(0);
}

void *emalloc(size_t size) {
  void *p;
  if ((p = malloc(size)) == NULL) error("out of memory in emalloc");
  return p;
}

int main(int argc, char *argv[]) {
  char c;
  char *s;
  char *src_file, *tape_file = NULL;
  /* Sort out command line options */
  while ((c = getopt(argc, argv, "vhtso:")) != -1) {
    switch (c) {
    case 'v': verbose = 1; break;
    case 't': transmit = 1; break;
    case 's': extra_space = 1; break;
    case 'o': tape_file = strdup(optarg); break;
    case 'h': fprint_help(stdout, argv[0]);
    }
  }
  if (optind == argc) { /* missing non-option argument (source file) */
    fprintf(stderr, "missing source file\n");
    fprint_help(stderr, argv[0]);
  } else src_file = strdup(argv[optind]);
  if (strchr(src_file, '.') == NULL) { /* append file extension */
    s = strdup(src_file); free(src_file);
    src_file = (char *)emalloc(strlen(s) + strlen(tri_ext) + 1);
    strcpy(src_file, s); free(s);
    strcat(src_file, tri_ext);
  }
  mninit();
  if (verbose) {
    printf("\nTriton Relocatable Machine Code Compiler\n\n");
    printf("Parsing tokens from %s\n", src_file);
  }
  parse(src_file); /* First pass through */
  if (verbose && tape_file) printf("Writing to %s\n", tape_file);
  if (!nvlistok()) { printnvlist(); error("undefined variables"); }
  if (tape_file) {
    if (strcmp(tape_file, "pipe") == 0) fsp = stdout;
    else if ((fsp = fopen(tape_file, "wb")) == NULL) {
      error("Couldn't open file for saving");
    }
  }
  if (transmit) {
    printf("Transmitting down the wires...\n");
    startio();
  }
  parse(src_file); /* Second pass through */
  if (transmit) {
    printf("\nFinished transmitting down the wires\n");
    finishio();
  }
  if (fsp && fsp != stdout) fclose(fsp);
  if (verbose) {
    printf("\nVariable list\n\n"); printnvlist();
  }
  return 0;
}

/* Reads in tokens from src_file and generates 8080 machine code */

void parse(char *file) {
  int i, j, len, val, valhi, vallo, nrpt, wasplit, found;
  int fpstackpos = 0;
  char tok[MAXTOK] = "";
  char mod[MAXTOK] = "";
  char *punkt, *tempfile;
  FILE *fp, *fp2, *fpstack[MAXFP];
  if ((fp = fopen(file, "r")) == NULL) error("couldn't open file");
  byte_count = 0;
  origin = addval("ORG", 0);
  if (nparse == 0) end_prog = addval("END", 0);
  do { /* Outer do loop around file inclusion levels */
    cc = ' ';    /* set initial character properly */
    while (tokin(tok, fp, MAXTOK) != EOF) {
      if (myscmp("include", tok)) {     /* Process include files */
        if (tokin(tok, fp, MAXTOK) == EOF) error("Expected a file name");
        if ((punkt = strchr(tok, '.')) == NULL) {
          tempfile = (char *)emalloc(strlen(tok) + strlen(tri_ext) + 1);
          strcpy(tempfile, tok); strcat(tempfile, tri_ext);
        } else tempfile = strdup(tok);
        if (verbose && (nparse == 0)) {
	  printf("Including tokens from %s\n",tempfile);
	}
        if ((fp2 = fopen(tempfile, "r")) == NULL) {
          error("I couldn't find the file.");
        } else {
          if (fpstackpos < MAXFP) { fpstack[fpstackpos++] = fp; fp = fp2; }
          else error("I'm out of file pointer stack space");
        }
      } else { /* Process tokens normally - first extract any modifiers */
        if (split(tok, mod, '=')) { addval(mod, eval(tok) & 0xFFFF); continue; }
        if (split(tok, mod, ':')) addval(mod, value[origin] + byte_count);
        if (split(tok, mod, '*')) sscanf(mod, "%i", &nrpt); else nrpt = 1;
	if (split(tok, mod, '>')) { /* here tok is the modifier */
	  if (tok[0] == '!') val = tokval(&tok[1]);
	  else val = eval(tok) & 0xFFFF;
	  nrpt = val - value[origin] - byte_count;
	  strcpy(tok, mod); /* to recover the token to be repeated */
	}
        if (nrpt < 0) {
          warn("negative repeat number, setting to zero"); nrpt = 0;
        } else if (nrpt > MAXRPT) {
          warn("repeat number too large, ignoring"); nrpt = 0;
        }
        switch (tok[0]) {
	case '\0': break; /* Case where there's nothing left of token */
	case '"': /* Encountered a string in double quotes */
	  if ((len = strlen(tok)) < 2 || tok[len-1] != '"') {
	    warn("invalid string"); break;
	  }
	  for (i=0; i<nrpt; i++) {
	    for (j=0; j<len-1; j++) {
	      if (tok[j] == '"') continue; else byte_out((int)tok[j]);
	    }
	  }
	  break;
	case '\'': /* Encountered a character in single quotes */
	  if (strlen(tok) != 3 || tok[2] != '\'') {
	    warn("invalid character"); break;
	  }
	  for (i=0; i<nrpt; i++) byte_out((int)tok[1]);
	  break;
	case '%': /* Encountered a decimal number */
	  if ((val = eval(tok)) < 0x100) for (i=0; i<nrpt; i++) byte_out(val);
	  else warn("decimal number too large, should be < 256");
	  break;
	case '!': /* Encountered a variable, dereference it therefore */
	  wasplit = split(tok, mod, '.'); val = tokval(&mod[1]);
	  if (!wasplit) for (i=0; i<nrpt; i++) word_out(val);
	  else {
	    valhi = val / 0x100; vallo = val - 0x100*valhi;
	    switch (tok[0]) {
	    case 'H': val = valhi; break;
	    case 'L': val = vallo; break;
	    default: warn("invalid byte specification"); val = 0;
	    }
	    for (i=0; i<nrpt; i++) byte_out(val);
	  }
	  break;
	default: /* See if it's a mnemonic or a piece of hex */
	  for (i=0, found=0; i<NMN; i++) {
	    if (strcmp(tok, mnemonic[i]) == 0) { found = 1; break; }
	  }
	  if (found) { /* Encountered a mnemonic */
	    val = mnval[i];
	    switch (mntype[i]) {
	    case 0: break;
	    case 1: val |= regin(fp); break;
	    case 2: val |= regin(fp) << 3; break;
	    case 3: val |= regin(fp) << 3; val |= regin(fp); break;
	    case 4: val |= pairin(fp) << 3; break;
	    case 5: val |= rstnin(fp) << 3; break;
	    }
	    for (i=0; i<nrpt; i++) byte_out(val);
	  } else { /* Encountered hex code */
	    if ((val = eval(tok)) < 0x100) for (i=0; i<nrpt; i++) byte_out(val);
	    else for (i=0; i<nrpt; i++) word_out(val & 0xFFFF); /* remove 16-bit word flag */
	  }
        }
      }
    }
    if (fclose(fp) != 0) error("I couldn't close the file.");
    if (--fpstackpos >= 0) fp = fpstack[fpstackpos];
  } while (fpstackpos >= 0);
  value[end_prog] = value[origin] + byte_count; /* capture end point */
  if (verbose && (nparse > 0)) printf("\n"); /* Catch trailing printout */
  nparse++;
}

/* Reads next token and returns code for register B,C,D,E,H,L,M, or A */

int regin(FILE*fp) {
  char reg[MAXREG];
  if (tokin(reg, fp, MAXREG) == EOF) error("unexpected end of file");
  if (reg[1] == '\0') switch (reg[0]) {
    case 'B': return 0;
    case 'C': return 1;
    case 'D': return 2;
    case 'E': return 3;
    case 'H': return 4;
    case 'L': return 5;
    case 'M': return 6;
    case 'A': return 7;
  }
  warn("invalid register specification"); return 0;
}

/* Reads next token and returns code for register pair B,D,H, or SP/PSW */

int pairin(FILE*fp) {
  char reg[MAXREG];
  if (tokin(reg, fp, MAXREG) == EOF) error("unexpected end of file");
  if (reg[1] == '\0') switch (reg[0]) {
    case 'B': return 0;
    case 'D': return 2;
    case 'H': return 4;
  }
  if (strcmp(reg, "SP") == 0 || strcmp(reg, "PSW") == 0) return 6;
  warn("invalid register specification"); return 0;
}

/* Reads next token after RST and returns value */

int rstnin(FILE*fp) {
  int val = NOVAL;
  char reg[MAXREG];
  if (tokin(reg, fp, MAXREG) == EOF) error("unexpected end of file");
  if (reg[1] == '\0') val = ctoi(reg[0]);
  if (val >= 0 && val <= 7) return val;
  warn("invalid number in RST N"); return 0;
}

/* (Re)initialises printed byte counter, printing program counter */

void zinit() {
  int pc;
  pc = value[origin] + byte_count;
  if (verbose && (nparse > 0) && pc < value[end_prog]) printf("\n%04X ", pc);
  zcount = 0; 
}

/* Initialises mnemonic codes */

void mninit() {
  int i, v, t;
  for (i=0; i<NMN; i++) {
    v = ctoi(mncode[i][0]) << 6;
    switch (mncode[i][1]) {
      case 'D': t = 2; break;
      case 'U': t = 4; break;
      case 'V': t = 4; v |= 8; break;
      case 'N': t = 5; break;
      default:  t = 0; v |= ctoi(mncode[i][1]) << 3;
    }
    if (mncode[i][2] == 'S') t++; else v |= ctoi(mncode[i][2]);
    mntype[i] = t; mnval[i] = v;
  }
}

/* Save, print, and/or transmit a 16-bit word as a pair of bytes in
   little-endian order */

void word_out(int v) {
  int hi, lo;
  hi = v / 0x100; lo = v - 0x100*hi;
  byte_out(lo); byte_out(hi);
}

/* Save, print, and/or transmit a byte */

void byte_out(int v) {
  char buf[1], c;
  if (v<0 || v>0xff) { warn("invalid byte crept in somehow"); v = 0; }
  c = buf[0] = (char)v;
  if (transmit) { write(fd, buf, 1); usleep(50000); }
  if (fsp) fwrite(&c, sizeof(char), 1, fsp);
  if (verbose && (nparse > 0)) printf(" %02X", v);
  byte_count++; zcount++;
  if (extra_space && zcount == 8) printf(" ");
  else if (zcount == 16) zinit();
}

/* Check name, value list for undefined names */

int nvlistok() {
  int i;
  for (i=0; i<nnv; i++) if (value[i] == NOVAL) return 0;
  return 1;
}

/* Prints out name, value list */

void printnvlist() {
  int i, v;
  for (i=0; i<nnv; i++) {
    if ((v = value[i]) == NOVAL) printf("%8s * undefined *   ", name[i]);
    else printf("%8s = %04X = %%%-6i ", name[i], v, v);
    if (i % 3 == 2) printf("\n");
  }
  if (nnv % 3 != 0) printf("\n");
}

/* Looks up the name in the name, value list and returns value, or 0 */
/* If not in list, then entered with NOVAL, but 0 returned */

int tokval(char *s) {
  int i;
  for (i=0; i<nnv; i++) if (strcmp(name[i], s) == 0) {
    return (value[i] == NOVAL) ? 0 : value[i];
  }
  newnv(s, NOVAL); return 0;
}

/* Adds the name, value to the list, returning position of entry */
/* if name is already there, then just alter value */
/* if name = ORG then reset byte count and restart printing */

int addval(char *s, int v) {
  int i;
  for (i=0; i<nnv; i++) if (strcmp(name[i], s) == 0) {
    value[i] = v;
    if (i == origin) { byte_count = 0; zinit(); }
    return i;
  }
  newnv(s, v);
  return nnv-1;
}

/* Adds a new name, value pair to the list */

void newnv(char *s, int v) {
  if (nnv == MAXNNV) error("exceeded storage for name,value pairs");
  if ((name[nnv] = strdup(s)) == NULL) error("out of heap space");
  value[nnv++] = v;
}

/* Returns value of string, or 0 and a warning if invalid */
/* To indicate s is a 16-bit word in the range 0x0000 - 0x00FF, */
/* (and not decimal) 0x10000 is added setting the 17th bit. */
/* This flag can be silently stripped off by v & 0xFFFF */

int eval(char *s) {
  int v = 0;
  if (sscanf(s, (s[0] == '%') ? "%%%i" : "%X", &v) != 1) {
    warn("unrecognised value for, using 0"); v = 0;
  }
  if (v<0 || v>0xffff) { warn("invalid number, using 0"); v = 0; }
  return (s[0] == '%' || s[2] == '\0') ? v : 0x10000 + v; 
}

/* Return 0, 1 depending on occurence of character c in s1 */
/* If it occurs, split into s1[] + c + s2[] */
/* If it does not occur, copy s1[] into s2[] */

int split(char *s1, char *s2, char c) {
  int i, j;
  for (i=0; (s2[i] = s1[i]) != c; i++) if (s1[i] == '\0') return 0;
  s2[i++] = '\0';
  for (j=0; (s1[j] = s1[i]) != '\0'; i++, j++) ;
  return 1;
}

/* Just a little routine to compare two strings */

int myscmp(char *s, char *t) {
  do {
    if (*s == '\0') return 1;
    if (*t == '\0') return 0;
  } while (*s++ == *t++);
  return 0;
}

/* Read in next token in s, return length of token, or EOF */
/* An error occurs if the token is longer than maxlen */

int tokin(char *s, FILE *fp, int maxlen) {
  int i = 0;
  int verbatim = 0;
  if (cc == EOF) return EOF;
  while (whitespace(cc)) {
    if (cc == '#') while ((cc = getc(fp)) != '#' && cc != EOF) ;
    cc = getc(fp);
  }
  if (cc == EOF) return EOF;
  while (!whitespace(cc) || verbatim) {
    if (cc == '"' || cc == '\'') verbatim = !verbatim;
    if (i == maxlen) error("token too long, probable syntax error");
    s[i++] = cc;
    if ((cc = getc(fp)) == EOF) break;
  }
  s[i++] = '\0';
  return i;
}

/* Return true if character c is whitespace */

int whitespace(char c) {
  return ((c > EOF && c <= ' ') || c == ',' || c == ';' || c == '#');
}

/* Initialise io port settings */

void startio() {
/* Open port for writing, and not as controlling tty */
  if ((fd = open(port, O_WRONLY | O_NOCTTY )) < 0) {
    fprintf(stderr, "Couldn't open %s for writing", port);
    error("there was an error");
  }
/* save current serial port settings */
  tcgetattr(fd, &oldtio);
/* clear struct for new port settings */
  bzero(&newtio, sizeof(newtio));
/* Set to 300 baud, 8 bits, odd parity, 2 stop bits, no hardware control */
  newtio.c_cflag = B300 | CS8 | CSTOPB | PARENB | PARODD | CLOCAL;
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

/* End of file */
