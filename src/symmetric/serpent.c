/* serpent.c
 *
 * $Id$
 *
 * For more details on this algorithm, see the Serpent website at
 * http://www.cl.cam.ac.uk/~rja14/serpent.html
 */

/* Copyright (C) 1998 Ross Anderson, Eli Biham, Lars Knudsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* I've modified this code a bit so that it interoperates with lsh
 * properly. 2000-9-5, Rafael R. Sevilla <dido@pacific.net.ph>
 */

/* NOTE: The copyright notice for the original version of this code
 * said "All rights reserved. This code is freely distributed for AES
 * selection process. No other use is allowed." However, the authors
 * later decided to GPL the code. /nisse */


#include "serpent.h"
#include "serpentsboxes.h"

#include <assert.h>

/*  The functions  */
void
serpent_setup(SERPENT_context *ctx, UINT32 key_size, const UINT8 *key)
{
#if ALLOCA_68K_BUG
  ALLOCA_START(alloca_ref);
#endif
  unsigned i, j;
  UINT32 w[132], k[132];
  /* UINT32 kd[8]; */
  /* const UINT8 *kptr; */

  assert(key_size >= SERPENT_MIN_KEYSIZE);
  assert(key_size <= SERPENT_MAX_KEYSIZE);

  for (i = key_size, j = 0;
       (i >= 4);
       i-=4, key +=4, j++)
    {
      assert(j<8);
      w[j] = key[0] | (key[1] << 8) | (key[2] << 16) | (key[3] << 24);
    }

    if (j < 8)
    {
      /* Pad key, "aabbcc" -> "aabbcc0100...00" */
      UINT32 partial = 0x01;
      while (i)
	partial = (partial << 8 ) | key[--i];
      w[j++] = partial;

      while (j < 8)
	w[j++] = 0;
    }  
#if 0
  if (key_size == SERPENT_KEYSIZE)
    kptr = key;
  else
    {
      /* Expand key by appending bits 1000...00. */
      UINT8 *ekey = alloca(SERPENT_KEYSIZE);
      unsigned i = key_size;
      
      memcpy(ekey, key, i);
      ekey[i++] = 0x01;

      while (i < SERPENT_KEYSIZE)
	ekey[i++] = 0;

      kptr = ekey;
    }
      
  for (i=0; i<8; i++) {
    kd[i] = 0;
    for (j=0; j<4; j++)
      kd[i] |= (*kptr++) << j*8;
  }

  for(i=0; i<8; i++)
    w[i]=kd[i];

  for(i++; i<8; i++)
    w[i]=0;
#endif

  for(i=8; i<16; i++)
    w[i]=ROL(w[i-8]^w[i-5]^w[i-3]^w[i-1]^PHI^(i-8),11);
  for(i=0; i<8; i++)
    w[i]=w[i+8];
  for(i=8; i<132; i++)
    w[i]=ROL(w[i-8]^w[i-5]^w[i-3]^w[i-1]^PHI^i,11);

  RND03(w[  0], w[  1], w[  2], w[  3], k[  0], k[  1], k[  2], k[  3]);
  RND02(w[  4], w[  5], w[  6], w[  7], k[  4], k[  5], k[  6], k[  7]);
  RND01(w[  8], w[  9], w[ 10], w[ 11], k[  8], k[  9], k[ 10], k[ 11]);
  RND00(w[ 12], w[ 13], w[ 14], w[ 15], k[ 12], k[ 13], k[ 14], k[ 15]);
  RND31(w[ 16], w[ 17], w[ 18], w[ 19], k[ 16], k[ 17], k[ 18], k[ 19]);
  RND30(w[ 20], w[ 21], w[ 22], w[ 23], k[ 20], k[ 21], k[ 22], k[ 23]);
  RND29(w[ 24], w[ 25], w[ 26], w[ 27], k[ 24], k[ 25], k[ 26], k[ 27]);
  RND28(w[ 28], w[ 29], w[ 30], w[ 31], k[ 28], k[ 29], k[ 30], k[ 31]);
  RND27(w[ 32], w[ 33], w[ 34], w[ 35], k[ 32], k[ 33], k[ 34], k[ 35]);
  RND26(w[ 36], w[ 37], w[ 38], w[ 39], k[ 36], k[ 37], k[ 38], k[ 39]);
  RND25(w[ 40], w[ 41], w[ 42], w[ 43], k[ 40], k[ 41], k[ 42], k[ 43]);
  RND24(w[ 44], w[ 45], w[ 46], w[ 47], k[ 44], k[ 45], k[ 46], k[ 47]);
  RND23(w[ 48], w[ 49], w[ 50], w[ 51], k[ 48], k[ 49], k[ 50], k[ 51]);
  RND22(w[ 52], w[ 53], w[ 54], w[ 55], k[ 52], k[ 53], k[ 54], k[ 55]);
  RND21(w[ 56], w[ 57], w[ 58], w[ 59], k[ 56], k[ 57], k[ 58], k[ 59]);
  RND20(w[ 60], w[ 61], w[ 62], w[ 63], k[ 60], k[ 61], k[ 62], k[ 63]);
  RND19(w[ 64], w[ 65], w[ 66], w[ 67], k[ 64], k[ 65], k[ 66], k[ 67]);
  RND18(w[ 68], w[ 69], w[ 70], w[ 71], k[ 68], k[ 69], k[ 70], k[ 71]);
  RND17(w[ 72], w[ 73], w[ 74], w[ 75], k[ 72], k[ 73], k[ 74], k[ 75]);
  RND16(w[ 76], w[ 77], w[ 78], w[ 79], k[ 76], k[ 77], k[ 78], k[ 79]);
  RND15(w[ 80], w[ 81], w[ 82], w[ 83], k[ 80], k[ 81], k[ 82], k[ 83]);
  RND14(w[ 84], w[ 85], w[ 86], w[ 87], k[ 84], k[ 85], k[ 86], k[ 87]);
  RND13(w[ 88], w[ 89], w[ 90], w[ 91], k[ 88], k[ 89], k[ 90], k[ 91]);
  RND12(w[ 92], w[ 93], w[ 94], w[ 95], k[ 92], k[ 93], k[ 94], k[ 95]);
  RND11(w[ 96], w[ 97], w[ 98], w[ 99], k[ 96], k[ 97], k[ 98], k[ 99]);
  RND10(w[100], w[101], w[102], w[103], k[100], k[101], k[102], k[103]);
  RND09(w[104], w[105], w[106], w[107], k[104], k[105], k[106], k[107]);
  RND08(w[108], w[109], w[110], w[111], k[108], k[109], k[110], k[111]);
  RND07(w[112], w[113], w[114], w[115], k[112], k[113], k[114], k[115]);
  RND06(w[116], w[117], w[118], w[119], k[116], k[117], k[118], k[119]);
  RND05(w[120], w[121], w[122], w[123], k[120], k[121], k[122], k[123]);
  RND04(w[124], w[125], w[126], w[127], k[124], k[125], k[126], k[127]);
  RND03(w[128], w[129], w[130], w[131], k[128], k[129], k[130], k[131]);

  for(i=0; i<=32; i++)
    for(j=0; j<4; j++)
      ctx->keys[i][j] = k[4*i+j];
#if ALLOCA_68K_BUG
  ALLOCA_FREE(alloca_ref);
#endif
}

void
serpent_encrypt(SERPENT_context *ctx,
		const UINT8 *plaintext,
		UINT8 *ciphertext)
{
  register UINT32 x0, x1, x2, x3;
  register UINT32 y0, y1, y2, y3;
  int i;
  UINT8 *cptr;

  x0=plaintext[0]|(plaintext[1]<<8)|(plaintext[2]<<16)|(plaintext[3]<<24);
  x1=plaintext[4]|(plaintext[5]<<8)|(plaintext[6]<<16)|(plaintext[7]<<24);
  x2=plaintext[8]|(plaintext[9]<<8)|(plaintext[10]<<16)|(plaintext[11]<<24);
  x3=plaintext[12]|(plaintext[13]<<8)|(plaintext[14]<<16)|(plaintext[15]<<24);

  /* Start to encrypt the plaintext x */
  keying(x0, x1, x2, x3, ctx->keys[ 0]);
  RND00(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 1]);
  RND01(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 2]);
  RND02(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 3]);
  RND03(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 4]);
  RND04(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 5]);
  RND05(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 6]);
  RND06(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 7]);
  RND07(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 8]);
  RND08(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[ 9]);
  RND09(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[10]);
  RND10(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[11]);
  RND11(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[12]);
  RND12(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[13]);
  RND13(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[14]);
  RND14(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[15]);
  RND15(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[16]);
  RND16(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[17]);
  RND17(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[18]);
  RND18(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[19]);
  RND19(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[20]);
  RND20(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[21]);
  RND21(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[22]);
  RND22(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[23]);
  RND23(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[24]);
  RND24(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[25]);
  RND25(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[26]);
  RND26(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[27]);
  RND27(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[28]);
  RND28(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[29]);
  RND29(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[30]);
  RND30(x0, x1, x2, x3, y0, y1, y2, y3);
  transform(y0, y1, y2, y3, x0, x1, x2, x3);
  keying(x0, x1, x2, x3, ctx->keys[31]);
  RND31(x0, x1, x2, x3, y0, y1, y2, y3);
  x0 = y0; x1 = y1; x2 = y2; x3 = y3;
  keying(x0, x1, x2, x3, ctx->keys[32]);
  /* The ciphertext is now in x */

  cptr = ciphertext;

  for (i=0; i<4; i++)
    *cptr++ = (x0 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *cptr++ = (x1 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *cptr++ = (x2 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *cptr++ = (x3 >> i*8) & 0xff;
}

void
serpent_decrypt(SERPENT_context *ctx,
		const UINT8 *ciphertext,
		UINT8 *plaintext)
{
  register UINT32 x0, x1, x2, x3;
  register UINT32 y0, y1, y2, y3;
  int i;
  UINT8 *pptr;

  x0=ciphertext[0]|(ciphertext[1]<<8)|(ciphertext[2]<<16)|(ciphertext[3]<<24);
  x1=ciphertext[4]|(ciphertext[5]<<8)|(ciphertext[6]<<16)|(ciphertext[7]<<24);
  x2=ciphertext[8]|(ciphertext[9]<<8)|(ciphertext[10]<<16)|(ciphertext[11]<<24);
  x3=ciphertext[12]|(ciphertext[13]<<8)|(ciphertext[14]<<16)|(ciphertext[15]<<24);

  /* Start to decrypt the ciphertext x */
  keying(x0, x1, x2, x3, ctx->keys[32]);
  InvRND31(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[31]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND30(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[30]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND29(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[29]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND28(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[28]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND27(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[27]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND26(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[26]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND25(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[25]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND24(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[24]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND23(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[23]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND22(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[22]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND21(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[21]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND20(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[20]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND19(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[19]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND18(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[18]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND17(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[17]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND16(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[16]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND15(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[15]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND14(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[14]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND13(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[13]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND12(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[12]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND11(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[11]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND10(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[10]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND09(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 9]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND08(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 8]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND07(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 7]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND06(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 6]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND05(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 5]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND04(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 4]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND03(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 3]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND02(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 2]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND01(x0, x1, x2, x3, y0, y1, y2, y3);
  keying(y0, y1, y2, y3, ctx->keys[ 1]);
  inv_transform(y0, y1, y2, y3, x0, x1, x2, x3);
  InvRND00(x0, x1, x2, x3, y0, y1, y2, y3);
  x0 = y0; x1 = y1; x2 = y2; x3 = y3;
  keying(x0, x1, x2, x3, ctx->keys[ 0]);
  /* The plaintext is now in x */

  pptr = plaintext;

  for (i=0; i<4; i++)
    *pptr++ = (x0 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *pptr++ = (x1 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *pptr++ = (x2 >> i*8) & 0xff;
  for (i=0; i<4; i++)
    *pptr++ = (x3 >> i*8) & 0xff;
}
