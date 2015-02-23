/*
 * aadrill - ascii-art based Kanji drill
 * 
 * Copyright (C) 2005  Serge Kosyrev aka Samium Gromoff <_deepfire@mail.ru>
 *
 * Licensed under the Open Software License version 2.0
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <asm/termios.h>
#include <sys/time.h>
#include <time.h>

#include "types.h"

#define MIN_USABLE_KANJIS 20

int read_kanjidic(char *kanjidic, struct dictconf *dict);

struct ui *aui;

int kanji_is_usable(struct trans *trans, struct drillconf *drill)
{
	if (trans == NULL)
		return -1;
	return (trans->frequency > drill->highfreq) &&
		(trans->frequency < drill->lowfreq) &&
		(drill->enabled_grades == 0xff ||
		 ((trans->grade_level != 0) && ((1 << trans->grade_level - 1) & drill->enabled_grades != 0)))
		? 0 : -1;
}

void drill_update_usable_kanji_count(struct drillconf *drill)
{
	struct trans *trans;
	int count;

	drill->usable_kanjis = 0;
	for (count = drill->dict.lowest_kanji; count <= drill->dict.highest_kanji; count++) {
		trans = drill->dict.trans[count];
		if (trans == NULL)
			continue;
		if (kanji_is_usable(trans, drill) == 0)
			drill->usable_kanjis++;
	}
}

void update_stats(struct drillconf *drill)
{
	int i;

	for(i = drill->dict.lowest_kanji; i <= drill->dict.highest_kanji; i++) {
		if(drill->dict.trans[i] == NULL)
			continue;
		
		drill->missed += drill->dict.trans[i]->incorrect;
	}
}

int init_drill(struct ui *ui, struct drillconf *drill)
{
	struct dictconf *dict = &drill->dict;
	int kanjidic, edict;
	struct timeval time;

	gettimeofday(&time, NULL);
	srand48(time.tv_sec);

	printf("Reading dictionaries: ");
	kanjidic = read_kanjidic(drill->dict.kanjidic, &drill->dict);
	printf("after kanjidic, low, high, total: 0x%04x-0x%04x %d\n",
	       dict->lowest_kanji, dict->highest_kanji, dict->total_kanji);
	if (dict->total_kanji == 0)
		dict->lowest_kanji = dict->highest_kanji = 0;

/* 	edict = read_edict(drill->dict.edict, &drill->dict); */
/* 	printf("\n"); */
/* 	printf("after edict, low, high, total: %d %d %d\n", */
/* 	       dict->lowest_kanji, dict->highest_kanji, dict->total_kanji); */

	printf("Kanjidic %s\n", kanjidic == 0 ? "OK" : "Failed");
	printf("Edict %s\n", edict == 0 ? "OK" : "Failed");
	printf("Total kanjis: %d\n", drill->dict.total_kanji);

	return kanjidic == 0 || edict == 0 ? 0 : -1;
}

int recalc_ui(struct root *r, struct visual *aa)
{
	struct winsize termsize;
	int cth, i;

	printf("NOTICE: resizing all frames, main window is: %dx%d...\n", r->w, r->h);
	r->pad.tw = r->tw;
	r->pad.th = UI_CONST_HPAD_HEIGHT;
	r->pad.w = r->pad.tw * r->wr;
	r->pad.h = r->pad.th * r->hr;
	r->pad.otx = 0;
	r->pad.oty = r->th - r->pad.th;
	r->pad.ox = 0;
	r->pad.oy = r->pad.oty * r->hr;

	cth = (r->th - r->pad.th) / 2;
	printf("\tpad: ox/oy %d/%d, w/h: %d/%d, cth %d\n",
	       r->pad.ox, r->pad.oy, r->pad.w, r->pad.h, cth);
	
	for (i = 0; i < 4; i++) {
		r->choice[i].w = cth * r->wr * UI_CONST_DIM_FACTOR * aa->dimfactor;
		r->choice[i].h = cth * r->hr;
	}
	r->choice[0].ox = r->choice[0].w;
	r->choice[0].oy = r->choice[0].h;
	r->choice[1].ox = 0;
	r->choice[1].oy = r->choice[0].h;
	r->choice[2].ox = 0;
	r->choice[2].oy = 0;
	r->choice[3].ox = r->choice[0].w;
	r->choice[3].oy = 0;
	for (i = 0; i < 4; i++)
		printf("\tchoice[%d]: ox/oy %d/%d, w/h: %d/%d\n",
		       i, r->choice[i].ox, r->choice[i].oy, r->choice[i].w, r->choice[i].h);

	r->main.ox = r->choice[0].ox + r->choice[0].w;
	r->main.oy = 0;
	r->main.w = (r->w - r->main.ox);
	r->main.h = r->h - r->pad.h;
	printf("\tmain: ox/oy %d/%d, w/h: %d/%d\n",
	       r->main.ox, r->main.oy, r->main.w, r->main.h);

	return 0;
}

void resize_aa(struct root *r, struct visual *aa)
{
	aa_resize(aa->ctx);
	r->w = aa_imgwidth(aa->ctx);
	r->h = aa_imgheight(aa->ctx);
	r->tw = aa_scrwidth(aa->ctx);
	r->th = aa_scrheight(aa->ctx);
	r->wr = r->w / r->tw;
	r->hr = r->h / r->th;
	r->buf = aa_image(aa->ctx);
	r->tbuf = aa_text(aa->ctx);
	r->abuf = aa_attrs(aa->ctx);
	recalc_ui(r, aa);
}

int init_aa(int *argc, char **argv, struct root *r, struct visual *aa)
{
	struct aa_kbddriver *kbddrv;
	struct aa_driver *drv;
	int i;	

	if (aa_parseoptions(&aa->hw, &aa->render, argc, argv) != 1) {
		fprintf(stderr, "ERROR: aalib choked on params\n");
		return -1;
	}

	if (getenv("SSH_CONNECTION"))
		aa->hw.supported = AA_NORMAL_MASK;
	else
		aa->hw.supported = AA_NORMAL_MASK | AA_DIM_MASK;

	aa->hw.minheight = UI_CONST_MIN_HEIGHT;
	aa->hw.minwidth = UI_CONST_MIN_WIDTH;

	aa->render.bright = 0;
	aa->render.contrast = 0;
	aa->render.gamma = 1;
	aa->render.dither = AA_FLOYD_S;
	aa->render.inversion = 0;
	aa->render.randomval = 0;

	for (drv = aa_drivers[0], i = 0; drv != NULL; i++, drv = aa_drivers[i]) {
		if (strcmp(aa->drvname, drv->shortname) != 0)
			continue;

		aa->drv = drv;
		aa->ctx = aa_init(drv, &aa->hw, NULL);
		if (aa->ctx == NULL) {
			fprintf(stderr, "ERROR: aalib driver \"%s\" returned error upon init\n", drv->name);
			return -1;
		}
		if (aa_autoinitkbd(aa->ctx, 0) != 1) {
			aa_close(aa->ctx);
			fprintf(stderr, "ERROR: aalib kbd driver \"%s\" returned error upon init\n", kbddrv->name);
			return -1;
		}
		if (strcmp(aa->drv->shortname, "X11") == 0)
			aa->dimfactor = 1.0;
		else
			aa->dimfactor = 1.35;

		resize_aa(r, aa);
		aa_hidecursor(aa->ctx);

		return 0;
	}

	fprintf(stderr, "ERROR: aalib failed to find a driver\n");

	return -1;
}

int init_vf(char *fontname, double fontsize, char *vflib_path, struct ui *ui)
{
	if (VF_Init(vflib_path, NULL) < 0) {
		switch (vf_error) {
		case VF_ERR_INTERNAL:
			printf("ERROR: internal vflib error\n");
			return -1;
		case VF_ERR_NO_MEMORY:
			printf("ERROR: server runs out of memory for vflib\n");
			return -1;
		case VF_ERR_NO_VFLIBCAP:
			printf("ERROR: no vflibcap\n");
			return -1;
		default:
			printf("ERROR: vflib error code %d\n", vf_error);
			return -1;
		}
	}

	ui->fontid = VF_OpenFont2(fontname, -1, fontsize, fontsize);
	if (ui->fontid < 0) {
		printf("ERROR: vflib font %s not found\n", fontname);
		return -1;
	}

	return 0;
}

int vf_draw_char(long character, int fontid, struct root *r, struct frame *f, struct visual *aa)
{
	int testbit(unsigned char *arry, int offt)
	{
		int byteno = offt >> 3, delta = offt - (byteno << 3);
		return (arry[byteno] & (1 << (7 - delta))) != 0;
	}
	double accscalex = 1.0 * aa->dimfactor, accscaley = 1.0;
	VF_METRIC2 metric;
	VF_BITMAP vfbm;
	int x, y;

	/* below lies a kanji scale factor fitting procedure */
/* 	printf("drawing %04x into %dx%d\n", character, f->w, f->h); */
 retry:
	metric = VF_GetMetric2(fontid, character, NULL, accscalex, accscaley);
	if (metric == NULL)
		return -1;

	if (metric->bbx_width > f->w) {	/* didn`t fit  by width */
		accscalex /= (double) metric->bbx_width / (double) f->w;
		accscaley /= (double) metric->bbx_width / (double) f->w;
		goto retry;
	} else if (metric->bbx_height > f->h) {	/* didn`t fit by height */
		accscalex /= (double) metric->bbx_height / (double) f->h;
		accscaley /= (double) metric->bbx_height / (double) f->h;
		goto retry;
	} else if (metric->bbx_width < f->w - 1 && metric->bbx_height < f->h - 1) {
		double wsc, hsc, winsc;
		wsc = (double) f->w / (double) metric->bbx_width;
		hsc = (double) f->h / (double) metric->bbx_height;
		winsc = wsc > hsc ? hsc : wsc;
		accscalex *= winsc;
		accscaley *= winsc;
		goto retry;
	}

/* 	printf("fitting successful: %dx%d, %f/%f\n", */
/* 	       metric->bbx_width, metric->bbx_height, accscalex, accscaley); */
	vfbm = VF_GetBitmap2(fontid, character, accscalex, accscaley);
	if (vfbm == NULL) {
		fprintf(stderr, "ERROR: no bitmap found for character %08x\n", character);
		return -1;
	}
	for (y = 0; y < vfbm->bbx_height; y++) {
		for (x = 0; x < vfbm->bbx_width; x++) {
			r->buf[x + f->ox + (y + f->oy) * r->w] =
				testbit(vfbm->bitmap, x + ((vfbm->bbx_width + 7) & 0xfff8) * y) ?
				255 : 0;
		}
	}

/* 			if (testbit(vfbm->bitmap, x + ((vfbm->bbx_width + 7) & 0xfff8) * y)) */
/* 				printf("#"); */
/* 			else */
/* 				printf("."); */
/* 		} */
/* 		printf("\n"); */
/* 	} */
/* 	printf("\n"); */
	VF_FreeBitmap(vfbm);

	return 0;
}

int sync_aa(int sx, int sy, int ex, int ey, struct visual *aa)
{
	aa_render(aa->ctx, &aa->render, sx, sy, ex, ey);
	aa_flush(aa->ctx);
}

int redraw_ui(struct drillconf *drill, struct ui *ui, struct root *r, struct visual *aa)
{
	int i;

	memset(r->buf, 0, r->w * r->h);
	memset(r->tbuf, ' ', r->tw * r->th);

	if (drill->mode == GRAPH_FROM_TEXT) {
		vf_draw_char(ui->kanji[ui->selected_kanji]->Uindex, ui->fontid, r, &r->main, aa);
		for (i = 1; i < UI_CONST_CHOICE_NUM; i++)
			vf_draw_char(ui->kanji[(ui->selected_kanji + i) % 5]->Uindex,
				     ui->fontid, r, &r->choice[i - 1], aa);
	} else if (drill->mode == TEXT_FROM_GRAPH) {
		vf_draw_char(ui->kanji[drill->correct]->Uindex, ui->fontid, r, &r->main, aa);
		for (i = 0; i < UI_CONST_CHOICE_NUM; i++)
			aa_printf(aa->ctx, 2, r->pad.oty + i + 1, AA_NORMAL,
				  "%d. %s", i + 1, ui->kanji[i]->english);
	}

	aa_printf(aa->ctx, 1, r->pad.oty, AA_NORMAL, "%s:%d:%d.  total/correct/missed: %d/%d/%d",
		  drill->last_was_correct ? "Correct" : "Incorrect",
		  drill->last_answer, drill->last_correct_answer,
		  drill->total_tested, drill->total_tested - drill->missed, drill->missed);

	for (i = 1; i < UI_CONST_HPAD_HEIGHT; i++)
		aa_puts(aa->ctx, 1, i + r->pad.oty, AA_NORMAL, ui->desc[i]);
	sync_aa(0, 0, r->tw, r->th - r->pad.th, aa);
}

struct trans *random_kanji(struct drillconf *drill)
{
	int kanji, i;
	
	/* at which one do we stop */
	kanji = lrand48() % drill->usable_kanjis;

	for (i = drill->dict.lowest_kanji; i <= drill->dict.highest_kanji; i++) {
/* 		printf("checking kanji %04x at %p\n", i, drill->dict.trans[i]); */
		if (kanji_is_usable(drill->dict.trans[i], drill) != 0)
			continue;
		kanji--;
		if (kanji == 0) {
/* 			sprintf(aui->desc[5], "moo"); */
			return drill->dict.trans[i];
		}
	}
	
/* 	sprintf(aui->desc[5], "ERROR: picked %d of usable %d", kanji, drill->usable_kanjis); */
	
	return random_kanji(drill);
}

void prepare_new_questions(struct ui *ui, struct drillconf *drill)
{
	struct trans *k;
	int i;

	for (i = 0; i < 5; i++)
		ui->kanji[i] = random_kanji(drill);
	drill->correct = lrand48() % 5;

	if (drill->mode == GRAPH_FROM_TEXT)
		strcpy(ui->desc[2], ui->kanji[drill->correct]->english);
}

void test_answer_and_update(int answer, struct ui *ui, struct drillconf *drill)
{
	int right = answer == drill->correct;

	drill->last_answer = answer;
	drill->last_correct_answer = drill->correct;
	drill->last_was_correct = right;
	drill->total_tested++;
	if (!right)
		drill->missed++;
}

int main(int argc, char **argv)
{
	struct drillconf drill = {
		.dict = {
			.kanjidic = "/usr/share/edict/kanjidic",
			.edict = "/usr/share/edict/edict"
		},
		.mode = TEXT_FROM_GRAPH,
		.from = LANG_ENGLISH,
		.to = LANG_KANJI,
		.lowfreq = 300,
		.highfreq = 0,
		.enabled_grades = 0x1,
		.total_tested = 0,
		.missed = 0
	};
	struct visual aa = {
		.drvname = argc < 2 ? "X11" : argv[1]
	};
	struct root root;
	struct ui ui;
	int retval = 1, i;
	aui = &ui;

	if (init_drill(&ui, &drill) != 0) {
		fprintf(stderr, "FATAL: drill configuration failed\n");
		goto bye;
	}
	drill_update_usable_kanji_count(&drill);
	printf("%d usable kanjis out of total %d\n", drill.usable_kanjis, drill.dict.total_kanji);
	if (drill.usable_kanjis < MIN_USABLE_KANJIS) {
		fprintf(stderr, "FATAL: not enough usable kanjis: %d\n", drill.usable_kanjis);
		exit(1);
	}
	getc(stdin);

	if (init_aa(&argc, argv, &root, &aa) != 0) {
		fprintf(stderr, "FATAL: aalib failed to initialise\n");
		goto bye;
	}
	if (init_vf("bkai00mp.ttf", 1.0, NULL, &ui) != 0) {
		fprintf(stderr, "FATAL: VFlib failed to initialise\n");
		goto bye_aa;
	}

	for (i = 0; i < UI_CONST_HPAD_HEIGHT; i++)
		ui.desc[i][0] = 0;

	update_stats(&drill);
	prepare_new_questions(&ui, &drill);
	while (1) {
		int evt;
		
		redraw_ui(&drill, &ui, &root, &aa);

		evt = aa_getevent(aa.ctx, 1);
		if (evt == 0 || evt == 400)
			continue;

		switch (evt) {
		case AA_RESIZE:
			resize_aa(&root, &aa);
			break;
		case AA_LEFT:
			ui.selected_kanji = (ui.selected_kanji + 1) % 5;
			break;
		case AA_RIGHT:
			ui.selected_kanji = (ui.selected_kanji + 4) % 5;
			break;
		/* either a number for translation of a displayed kanji,
		 * or enter for kanji selection, given a translation */
		case 113: goto bye_clean;
		default:
			if (drill.mode == TEXT_FROM_GRAPH && evt >= '1' && evt <= '5') {
				test_answer_and_update(evt - '1', &ui, &drill);
				prepare_new_questions(&ui, &drill);
				ui.selected_kanji = 0;
			} else if (drill.mode == GRAPH_FROM_TEXT && evt == 13) {
				test_answer_and_update(ui.selected_kanji, &ui, &drill);
				prepare_new_questions(&ui, &drill);
				ui.selected_kanji = 0;
			}
		}
	}

 bye_clean:
	retval = 0;
 bye_complete:
	VF_CloseFont(ui.fontid);
 bye_aa:
	aa_showcursor(aa.ctx);
	aa_close(aa.ctx);
 bye:
	
	return retval;
}
