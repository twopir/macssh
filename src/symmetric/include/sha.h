/* 
 * $Id$
 */

#include "crypto_types.h"

/* The SHA block size and message digest sizes, in bytes */

#define SHA_DATASIZE    64
#define SHA_DATALEN     16
#define SHA_DIGESTSIZE  20
#define SHA_DIGESTLEN    5
/* The structure for storing SHA info */

struct sha_ctx {
  UINT32 digest[SHA_DIGESTLEN];  /* Message digest */
  UINT32 count_l, count_h;       /* 64-bit block count */
  UINT8 block[SHA_DATASIZE];     /* SHA data buffer */
  int index;                     /* index into buffer */
};

void sha_init(struct sha_ctx *ctx);
void sha_update(struct sha_ctx *ctx, const UINT8 *buffer, UINT32 len);
void sha_final(struct sha_ctx *ctx);
void sha_digest(struct sha_ctx *ctx, UINT8 *s);
void sha_copy(struct sha_ctx *dest, struct sha_ctx *src);

/* The core compression function, mapping 5 + 16 32-bit words to 5
 * words. Destroys the data in the process. */
void sha_transform(UINT32 *state, UINT32 *data);
