/*
 * aadrill - ascii-art based Kanji drill
 * 
 * Copyright (C) 2005  Serge Kosyrev aka Samium Gromoff <_deepfire@mail.ru>
 *
 * Licensed under the Open Software License version 2.0
 * 
 */

/*
 * Here are located the externally-forced definitions, so as not to depend on them.
 */

#ifndef _XDEFS_H_
#define _XDEFS_H_


typedef struct {                /* normal 16 bit characters are two bytes */
	unsigned char byte1;
	unsigned char byte2;
} XChar2b;


#endif
