/*
 *	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `descore.README' for the complete copyright notice.
 *
 * Slightly edited by Niels M�ller, 1997
 */

#include "des.h"

#include "RCSID.h"
RCSID2(desQuick_cRcs, "$Id$");

extern UINT32 des_keymap[];


/* static information */

static int depth = 0;		/* keep track of the request depth */
UINT32 des_bigmap[0x4000];	/* big lookup table */

/* fill in the 64k table used by the `quick' option */

void
DesQuickInit(void)
{
	int s1, s3, x;
	UINT32 * t0, * t1, * t2, * t3;

	if ( depth++ )
		return;

	t0 = des_bigmap;
	t1 = t0 + 64;
	t2 = t1 + 64;
	t3 = t2 + 64;

	for ( s3 = 63; s3 >= 0; s3-- ) {
		for ( s1 = 63; s1 >= 0; s1-- ) {
			x = (s3 << 8) | s1;
			t0[x] = des_keymap[s3+128] ^ des_keymap[s1+192];
			t1[x] = des_keymap[s3    ] ^ des_keymap[s1+ 64];
			t2[x] = des_keymap[s3+384] ^ des_keymap[s1+448];
			t3[x] = des_keymap[s3+256] ^ des_keymap[s1+320];
		}
	}
}

/* free the 64k table, if necessary */

void
DesQuickDone(void)
{
}
