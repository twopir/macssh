/*
 *	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `descore.README' for the complete copyright notice.
 */

#include "des.h"

#include "RCSID.h"
RCSID2(desKerb_cRcs, "$Id$");

/* permit the default style of des functions to be changed */

DesFunc *DesCryptFuncs[2] = { DesSmallFipsDecrypt, DesSmallFipsEncrypt };

/* kerberos-compatible key schedule function */

int
des_key_sched(const UINT8 *k, UINT32 *s)
{
	return DesMethod(s, k);
}

/* kerberos-compatible des coding function */

int
des_ecb_encrypt(const UINT8 *s, UINT8 *d, const UINT32 *r, int e)
{
	(*DesCryptFuncs[e])(d, r, s);
	return 0;
}
