/* mpf_set_ui() -- Assign a float from an unsigned int.

Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#include "gmp.h"
#include "gmp-impl.h"

void
#if __STDC__
mpf_set_ui (mpf_ptr x, unsigned long int val)
#else
mpf_set_ui (x, val)
     mpf_ptr x;
     unsigned long int val;
#endif
{
  if (val != 0)
    {
      x->_mp_d[0] = val;
      x->_mp_size = 1;
      x->_mp_exp = 1;
    }
  else
    {
      x->_mp_size = 0;
      x->_mp_exp = 0;
    }
}
