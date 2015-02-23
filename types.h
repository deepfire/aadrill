/*
 * aadrill - ascii-art based Kanji drill
 * 
 * Copyright (C) 2005  Serge Kosyrev aka Samium Gromoff <_deepfire@mail.ru>
 *
 * Licensed under the Open Software License version 2.0
 * 
 */

#ifndef _TYPES_H_
#define _TYPES

#include <aalib.h>
#include <VFlib-3_6.h>

#include "xdefs.h"


/* Backend-specific information. */
struct visual {
	char *drvname;
	struct aa_driver *drv;
	struct aa_kbddriver *kbddrv;
	struct aa_context *ctx;
	struct aa_renderparams render;
	struct aa_hardware_params hw;
	double dimfactor;
};

/*                       
 *  +-------+-------+-----------------+
 *  |       |       |                 |
 *  |   C   |   C   |                 |
 *  |       |       |                 |
 *  +-------+-------+     CMain       |
 *  |       |       |                 |
 *  |   C   |   C   |                 |
 *  |       |       |                 |
 *  +-------+-------+-----------------+
 *  | Pad                             |
 *  +---------------------------------+
 *
 */
#define UI_CONST_HPAD_HEIGHT 6
#define UI_CONST_CHOICE_NUM 5
#define UI_CONST_DIM_FACTOR 1.3
#define UI_CONST_MIN_HEIGHT (UI_CONST_HPAD_HEIGHT + 2 * 5)
#define UI_CONST_MIN_WIDTH 80

struct frame {
	int w, h, ox, oy;
};
/* the w, h, ox, oy are in virtual pixels; tw, th, otx, oty are in characters */
struct root {
	unsigned char *buf, *tbuf, *abuf;
	int w, h, tw, th, hr, wr;
	struct {
		int w, h, ox, oy, tw, th, otx, oty;
	} pad;
	struct frame choice[4];
	struct frame main;
};

#define MINKANJIALLOWED 0x2000
#define MAXKANJIALLOWED 0x8000
#define MAXTRANSLATIONSALLOWED 0x30000

struct trans {
	int kdrill_index; /* position in internal array */

	char *english;	/* short english translation. */

	u_int16_t Uindex;	/* "Unicode" index */
	u_int16_t Qindex;	/* for the "four corner" lookup method */
	u_int16_t Sindex;	/* for SKIP: 2 bytes, see skipfromthree() */
	u_int16_t Nindex;	/* for annoying "See Nxxxx" references */
				/* Nelson dictionary */
	u_int16_t Hindex;		/* Halpern dictionary indexes */

	u_int16_t frequency;	/* frequency that kanji is used */
	u_int8_t grade_level;	/* akin to  school class level */
	u_int8_t Strokecount;
	u_int16_t incorrect;

	XChar2b *pronunciation;	/* kana, actually */
	XChar2b *kanji;		/* can be pointer to pronunciation */

	struct trans *nextk;
};
struct drillconf;
struct ui {
	int fontid;
	struct trans *kanji[UI_CONST_CHOICE_NUM];
	int selected_kanji;
	char desc[UI_CONST_HPAD_HEIGHT][1024];
	struct drillconf *drill;
};

struct dictconf {
	char *kanjidic;
	char *edict;

	int total_kanji;
	int lowest_kanji, highest_kanji;

	struct trans *trans[MAXTRANSLATIONSALLOWED];
};

enum lang_target {
	LANG_KANJI, LANG_KATAKANA, LANG_HIRAGANA, LANG_ROMAJI, LANG_ENGLISH, LAST_LANG = LANG_ENGLISH
};
enum ui_mode {
	GRAPH_FROM_TEXT, TEXT_FROM_GRAPH
};

struct drillconf {
	struct dictconf dict;

	int lowfreq, highfreq; /* frequency corridor for random kanji selection */
	int enabled_grades;
	int usable_kanjis;
	/* only a subset of combinations of lang_targets is valid */
	enum lang_target from, to;
	enum ui_mode mode;

	int total_tested, missed, last_was_correct, last_answer, last_correct_answer;

	int correct;
};


#endif
