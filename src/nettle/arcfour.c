/* arcfour.c
 *
 * The arcfour/rc4 stream cipher.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2001 Niels M�ller
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "arcfour.h"

#include <assert.h>

#define SWAP(a,b) do { int _t = a; a = b; b = _t; } while(0)

void
arcfour_set_key(struct arcfour_ctx *ctx,
		unsigned length, const uint8_t *key)
{
  unsigned i, j, k;
  
  assert(length >= ARCFOUR_MIN_KEY_SIZE);
  assert(length <= ARCFOUR_MAX_KEY_SIZE);

  /* Initialize context */
  for (i = 0; i<256; i++)
    ctx->S[i] = i;

  for (i = j = k = 0; i<256; i++)
    {
      j += ctx->S[i] + key[k]; j &= 0xff;
      SWAP(ctx->S[i], ctx->S[j]);
      /* Repeat key as needed */
      k = (k + 1) % length;
    }
  ctx->i = ctx->j = 0;
}


void
arcfour_crypt(struct arcfour_ctx *ctx,
	      unsigned length, uint8_t *dst,
	      const uint8_t *src)
{
  register uint8_t i, j;

  i = ctx->i; j = ctx->j;
  while(length--)
    {
      i++; i &= 0xff;
      j += ctx->S[i]; j &= 0xff;
      SWAP(ctx->S[i], ctx->S[j]);
      *dst++ = *src++ ^ ctx->S[ (ctx->S[i] + ctx->S[j]) & 0xff ];
    }
  ctx->i = i; ctx->j = j;
}

void
arcfour_stream(struct arcfour_ctx *ctx,
	       unsigned length, uint8_t *dst)
{
  register uint8_t i, j;

  i = ctx->i; j = ctx->j;
  while(length--)
    {
      i++; i &= 0xff;
      j += ctx->S[i]; j &= 0xff;
      SWAP(ctx->S[i], ctx->S[j]);
      *dst++ = ctx->S[ (ctx->S[i] + ctx->S[j]) & 0xff ];
    }
  ctx->i = i; ctx->j = j;
}

