/*
 * Read in kanjidic and edict.
 *
 * This file is almost entirely copyright to Philip Brown, author of kdrill. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"

#define MAXLINELEN 1024
#define NOKANJI 0x2266

static int xtoi(char *s);
static int nextword(char **stringp);
static u_int16_t *ucs2dup(u_int16_t *buf);
static short skipfromthree(int one, int two, int three);
static short parseskip(char *input);
static u_int16_t *read_kanjidic_pronounciation(char **pstring);
static void read_edict_pronounciation(char **pstring, struct trans *trans);
static int strip_slash(char *topdest, char *source);


static int xtoi(char *s)
{
	int out = 0;
	sscanf(s, "%x", &out);
	return out;
}

static int nextword(char **stringp)
{
	while (!isspace(**stringp)) {
		if(stringp == '\0')
			return 0;
		*stringp +=1;
	}
	/* now on space */
	while (isspace(**stringp)) {
		if(stringp == '\0')
			return 0;
		*stringp +=1;
	}
	return 1;
}

u_int16_t *ucs2dup(u_int16_t *buf)
{
	u_int16_t *ret;
	int len;
		
	len = strlen((char *) buf);
		
	ret = malloc(len + 2);
	if (ret == NULL) exit(1);

	memcpy(ret, buf, len);
	ret[len >> 1] = 0;

	return ret;
}

/* lets make sure we have one single unified skip encoding here! */
static short skipfromthree(int one, int two, int three)
{
      unsigned int SKIPnum = (one << 12) | (two << 8) | three;

      if ((one > 0xf) | (two > 0xf) | (three > 0xff) | (SKIPnum < 0))
      {
#ifdef DEBUG
              printf("corrupted SKIP ('Px-x-x') entry: %d-%d-%d\n",
                     one, two, three);
#endif
              return 0;
      }
      return (short) SKIPnum & 0xffff;
}

/* parseskip
 * Take a string pointing to the first char AFTER the "P", in 
 * kanjidic.
 * So we expect a string like "4-5-11 xxx xxx xxx"
 *
 * We will then convert the three numbers into single byte values,
 * and put them in the short we return.
 * In hex, with a full short being [f][f][f][f], that would look like
 * [1][2][3][3], in nibble positions.
 * Although you really shouldn't care what we do with it, just remember that it
 * is a short. We call skipfromthree(), and so should anything else!
 */
static short parseskip(char *input)
{
	int one, two, three;
	
	one = atoi(input);

	input++;
	if (*input != '-') {
#ifdef DEBUG
		puts("corrupted SKIP ('Px-x-x') entry");
#endif		
		return 0;
	}
	input++;
	two = atoi(input);

	input++;
	if (*input != '-')
		input++;
	if (*input != '-') {
#ifdef DEBUG
		puts("corrupted SKIP ('Px-x-x') entry");
#endif		
		return 0;
	}
	input++;
	three = atoi(input);

	return skipfromthree(one, two, three);
}

static u_int16_t *read_kanjidic_pronounciation(char **pstring)
{
	enum {ERROR, READING, OKURIGANA, BLANK, DONE};
	XChar2b kbuff[MAXLINELEN];
	char *parse = *pstring;
	XChar2b *kptr = kbuff;
	int state = BLANK;

	if (*parse == '{'){
		/* only english exists,
		 *  (no kanji, even)
		 *   so set character to be unusable.
		 */
		return 0;
	}
	while(*parse == ' ')
		parse++;

	/* THIS is going to get yeuky.
	 *  We are going to parse a line segment which has
	 *  reading.oku  pairs.
	 * This is REALLY annoying, because the line jumps between
	 * 8 -bit and 16-bit chars
	 */
	/* okay, bad practice... you tell me what would be better :-/ */
	while (1) {
		/* bug in gcc? If we put 
		 *	int state=BLANK;
		 * here, it gets reset each time through the while loop
		 */
		if (kptr > &kbuff[MAXLINELEN]) {
			fprintf(stderr, "ERROR! overflow reading in kanjidic\n");
			fprintf(stderr, "%s\n",*pstring);
			return 0;
		}

		switch(*parse) {
			case '.':
				parse++;
				/* we ALWAYS need to close this off later */
				state = OKURIGANA;
				/* open paren */
				kptr->byte1 = 0x21;
				kptr->byte2 = 0x4a;
				kptr++;
				break;
			case '-':
				parse++;
#ifdef USEEXTRABLANKS
				if(state == BLANK){
					kptr->byte1 = 0x21;
					kptr->byte2 = 0x21;
					kptr++;
				}
#endif
				kptr->byte1 = 0x21;
				kptr->byte2 = 0x41;
				kptr++;
#ifdef USEEXTRABLANKS
				if(state != BLANK){
					kptr->byte1 = 0x21;
					kptr->byte2 = 0x21;
					kptr++;
				}
#endif
				continue;
				/* start at top of while again */

			case '\0':
			case '\n':
			case '\r':
			case '{':
				if(state == OKURIGANA){
					/* close paren */
					kptr->byte1 = 0x21;
					kptr->byte2 = 0x4b;
					kptr++;
				}
				state = DONE;
				break;

			case ' ':
				if(state == OKURIGANA){
					/* close paren */
					kptr->byte1 = 0x21;
					kptr->byte2 = 0x4b;
					kptr++;
				}
				state = BLANK;

				parse++;
				kptr->byte1 = 0x21;
				kptr->byte2 = 0x21;
				kptr++;
				break;

			default:
				if (*parse < 127){
					if (state == OKURIGANA){
						/* close paren */
						kptr->byte1 = 0x21;
						kptr->byte2 = 0x4b;
						kptr++;
						puts("Kdrill.. error on kana read-in... ");
						puts("Expecting high bit char to start after '.'");
						printf("%s\n", *pstring);
						
					}
					state = BLANK;
					parse++;
				} else {
					if(state != OKURIGANA)
						state = READING;
				}
				break;
		}

		if (state == DONE) {
			break;
		}
		if (state == BLANK)
			continue;
		/* else read in another char */
		kptr->byte1 = (*parse++ & 0x7f);
		kptr->byte2 = (*parse++ & 0x7f);
		kptr++;

	} /* while(1) */

	/* copy out to struct, and exit */
	kptr->byte1 = 0;
	kptr->byte2 = 0;

	*pstring = parse;
	return ucs2dup((u_int16_t *) kbuff);
}

/* strip_brackets:
 *	Gets rid of those annoying {english}{english2} brackets.
 *	PRESUMES first char of source is '{'!!
 *      Well, actually, it nicely sets a null string if otherwise.
 *	See also StripSlash, below
 */
static void strip_brackets(char *dest, char *source)
{
	char *parse = &source[1];

	if (source[0] != '{') {
		dest[0] = '\0';
		return;
	}
	/* (*dest) is always assumed to be needing a write */
	do {
		switch (*parse) {
			case '{':
				*dest++ = ':';
				*dest++ = ' ';
				break;
			case '}':
				break;
			case '\n':
				*dest++='\0';
			default:
				*dest++ = *parse;				
		}
		parse++;
	} while((*parse != '\n') && (*parse != '\0'));

	*dest = '\0';

	return;
}

int read_kanjidic(char *kanjidic, struct dictconf *dict)
{
	struct trans *newk, *lastk;
	FILE *kdic, *edic;
	int linelen;
	char *line;

	kdic = fopen(kanjidic, "r");
	if (kdic == NULL) {
		fprintf(stderr, "Kanjidic not found at %s\n", kanjidic);
		return -1;
	}

	dict->total_kanji = dict->lowest_kanji = dict->highest_kanji = 0;
	newk = lastk = NULL;

	line = NULL; linelen = 0;
	while (getline(&line, &linelen, kdic) != -1) {
		int freq, grade, N, U, H, Q, SKIP;
		char strokes, *parse;
		int Kanji, instrlen;

		Kanji = xtoi(&line[2]);
		if (Kanji < MINKANJIALLOWED || Kanji > MAXKANJIALLOWED)
			continue;
		parse = &line[2];
		if (parse == NULL)
			continue;

		/* now parse for grade level, frequency, and english */
		freq = grade = N = U = H = SKIP = 0;
		strokes = 0; Q = -1; /* remember, "0000" IS a valid Qval! */
		
		nextword(&parse);
		
		/* Check for high bit set, which means
		 * start of kana definition of kana.
		 * We cheat a bit, and let this loop skip over
		 * numbers by the fact that they don't match
		 * the case statements.
		 */
		while ((*parse < 127)  && (*parse != '{')) {
			switch(*parse) {
			case 'F': freq = 	atoi(parse + 1); break;
			case 'G': grade =	atoi(parse + 1); break;
			case 'H': H =		atoi(parse + 1); break;
			case 'N': N =		atoi(parse + 1); break;
			case 'P': SKIP =	parseskip(parse + 1);	break;
			case 'Q': Q = 		atoi(parse + 1); break;
			case 'S': strokes =	atoi(parse + 1); break;
			case 'U':
				U = xtoi(parse + 1);
				if (U&0xffff0000)
					printf("got hi U: %x\n", U);
				break;
			default:
				parse++;
				break;
			}
			nextword(&parse);
		} /* while != '{' */
		
		/**********************************************
		 *  Now we know that we have a useable/wanted *
		 *  dictionary definition                     *
		 *********************************************/
		if (dict->lowest_kanji == dict->highest_kanji && dict->highest_kanji == 0) {
			dict->lowest_kanji = dict->highest_kanji = Kanji;
		} else {
			if (Kanji < dict->lowest_kanji)
				dict->lowest_kanji = Kanji;
			if (Kanji > dict->highest_kanji)
				dict->highest_kanji = Kanji;
		}
		
		lastk = newk;
		
		newk = (struct trans *)	malloc(sizeof(struct trans));
		if (newk == NULL) {
			perror("Cannot allocate memory for translation table\n");
			exit(1);
		}
		newk->Sindex = SKIP;
		newk->Qindex = Q;
		newk->Uindex = U;
		newk->Hindex = H;
		newk->Nindex = N;
		newk->frequency = freq;
		newk->grade_level = grade;
		newk->Strokecount = strokes;
		newk->incorrect = 0;
		newk->kanji = 0;
		newk->pronunciation = 0;
		newk->nextk = NULL;
#if 0 //defined(DEBUG)
		printf("Q=%d, U=%d, freq=%d\n", Q, U, freq);
#endif
		
		newk->pronunciation = (XChar2b *) read_kanjidic_pronounciation(&parse);
/* 		if (newk->pronunciation == 0){ */
/* 			free(newk); */
/* 			newk = lastk; */
/* 			continue; */
/* 		} else */ {
			XChar2b buff[2];
			
			buff[0].byte1 = (Kanji & 0xff00) >> 8;
			buff[0].byte2 = (Kanji & 0xff);
			buff[1].byte1 = 0;
			buff[1].byte2 = 0;
			newk->kanji = (XChar2b *) ucs2dup((u_int16_t *) buff);
		}
		if(lastk != NULL)
			lastk->nextk = newk;
		
		instrlen = strlen(parse) + 1;
		newk->english = malloc(instrlen);
		if (newk->english == NULL) {
			perror("Cannot allocate memory for translation table\n");
			exit(1);
		}
		
		strip_brackets(newk->english, parse);
		newk->kdrill_index = Kanji;
		dict->trans[Kanji] = newk;
		dict->total_kanji++;
		if (dict->total_kanji % 1000 == 0) {
			putchar('.');
			fflush(stdout);
		}
	}

	return 0;
}

/* read in kanji/kana part of edictfile line.
 * format is:
 *
 * KANA /english_1/.../
 *
 * or
 *
 * KANJI [KANA] /english_1/english_2/.../
 *
 *
 */
static void read_edict_pronounciation(char **pstring, struct trans *trans)
{
	/* note that MAXLINELEN means we canot possibly run out of space */
	XChar2b kbuff[MAXLINELEN];
	XChar2b *kptr = kbuff;
	char *parse = *pstring;

	/* Read in a 16-bit string.
	 * We dont know if its kana or kanji yet
	 */
	while (*parse && (*parse != '/')) {
		switch (*parse) {
		case ' ':
			/* 0x2121 is ' ' */
			kptr->byte1 = 0x21;
			kptr->byte2 = 0x21;
			kptr++;
			parse++;			
			break;
		case '[':
		        /* oops.. the kanji/kana switch */
			/* save what must be kanji, then start
			 * on kana
			 */
			kptr->byte1 = 0;
			kptr->byte2 = 0;
			
			trans->kanji = (XChar2b *) ucs2dup((u_int16_t *) kbuff);
			kptr = kbuff;
			/* now reset buffer, and read in another char16
			 * string
			 */
			parse++;
			break;
		case ']':
			parse++;
			while(*parse && (*parse != '/'))
				parse++;
			/* and then we will fall out of the top loop  */
			break;
		default:
			kptr->byte1 = (*parse++ & 0x7f);
			kptr->byte2 = (*parse++ & 0x7f);
			kptr++;
		}
	}

	/* when we come out here, we will ALWAYS have kana in
	 * the kbuff
	 */
	kptr->byte1 = 0;
	kptr->byte2 = 0;

	trans->pronunciation = (XChar2b *) ucs2dup((u_int16_t *) kbuff);

	*pstring = parse;
}

/* StripSlash
 *	Gets rid of /enlish/english2/ Slashes.
 *	Copies the cleaned up version of source, to topdest
 *
 *	This is for readedict. Probably nothing else should use it
 *	Modeled directly after StripBrackets
 *	PRESUMES first char of source is '/'!!
 *	Then looks for LAST '/'
 *	(Or will set topdest[0] to '\0')
 *
 *	We USED to translate middle ':' to '/'.
 *
 *	Source is actually assumed to be regular ascii signed char,
 *	but declared as unsigned to stop compiler warnings.
 *
 * return 0 OKAY, 1 bad line
 */
static int strip_slash(char *topdest, char *source)
{
	char *dest, *parse = source;
	int englen;

	if (*parse != '/') {
		topdest = '\0';
		return 1;
	}
	parse = strrchr(source, '/');
	if (parse < &source[2]) {
		fprintf(stderr, "Error: english part too short\n");
		fprintf(stderr, "%s\n", source);
		return 1;
	}
	englen = parse - source - 1;
	strncpy(topdest, &source[1], parse - source - 1);
	topdest[englen] ='\0';

	/* we've copied the relavant part over to topdest. Now rewrite
	 * in-place
	 */
	dest = topdest;
	dest = strchr(dest, '/');
	while (dest != NULL){
		*dest = ':';
		dest = strchr(dest, '/');
	}

	return 0;
}

/* readedict()
 * 
 *	Read in "edict.gz" if it exists
 *	[readstructs handles kanjidic reading]
 *	
 *	We only make very partial entries for edict entries
 *	We just fill out "english" and "pronunciation" entries.
 *
 *	If we cannot extract a kanji entry, the kanji pointer of a
 *	translation will be set to a shared string of '8' on its side
 *
 *	Note that we always start entries at index
 *	translations[MAXKANJIALLOWED+1]. This is to attempt to keep
 *	usefiles working
 *	
 */
int read_edict(char *edict, struct dictconf *dict)
{
	int slashcount, linelen, linecount = 0, nextindex = MAXKANJIALLOWED + 1;
	static XChar2b no_kanji[2] = { {0x0, 0x0}, {0x0, 0x0} };
	struct trans *newk = NULL, *lastk;
	char *line, *parse, *slashparse;
	FILE *edic;

	no_kanji[0].byte1 = (NOKANJI >> 8);
	no_kanji[0].byte2 = (NOKANJI & 0xff);

	/* the following will be NULL if kanjidic not read in */
	lastk = dict->trans[dict->highest_kanji];

	edic = fopen(edict, "r");
	if (edic == NULL)
		return -1;
	if (dict->highest_kanji == 0)
		dict->lowest_kanji = nextindex;

	line = NULL; linelen = 0;
	while (getline(&line, &linelen, edic) != -1) {
		int instrlen;

		linecount++;
		if(newk == NULL) {
			newk = (struct trans *) malloc(sizeof(struct trans));
			if (newk == NULL) {
				perror("OOM");
				exit(1);
			}
		}
		bzero(newk, sizeof(*newk));

		/* 1- read first part
		 * 2- read optional [part]
		 * 3- read english part
		 */
		parse = line;
		newk->kanji = no_kanji;

		read_edict_pronounciation(&parse, newk);
		if (newk->pronunciation == NULL) {
			fprintf(stderr,"Error reading edict\n");
			newk = NULL;
			continue;
		}

		while ((*parse != '/') && *parse)
			parse++;
		slashcount = 1;
		slashparse = parse;
		while (*slashparse++)
			if(*slashparse =='/')
				slashcount++;

		newk->english = malloc(strlen(parse) + 1 + slashcount * 4);
		if (newk->english == NULL) {
			perror("Cannot allocate memory for translation table\n");
			exit(1);		
		}
		
		if (strip_slash(newk->english, parse) != 0)
			fprintf(stderr, "bad line: %s\n", line);

		/* Success! Set pointers appropriately */
		newk->kdrill_index = nextindex;
		dict->trans[nextindex++] = newk;
		if (lastk != NULL)
			lastk->nextk = newk;
		lastk = newk;
		newk = NULL;

		dict->total_kanji++;
		if (dict->total_kanji % 1000 == 0) {
			putchar('.');
			fflush(stdout);
		}
	}

	fclose(edic);
	if (nextindex != MAXKANJIALLOWED + 1)
		dict->highest_kanji = nextindex - 1;

	return 0;
}
