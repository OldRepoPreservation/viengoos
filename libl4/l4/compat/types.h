/*********************************************************************
 *                
 * Copyright (C) 2001, 2002, 2003,  Karlsruhe University
 *                
 * File path:     l4/types.h
 * Description:   Commonly used L4 types
 *                
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *                
 * $Id: types.h,v 1.1 2003/07/26 17:26:09 marcus Exp $
 *                
 ********************************************************************/

#if defined(__cplusplus)
static inline L4_Clock_t operator + (const L4_Clock_t &l, const int r)
{
  return (L4_Clock_t) { raw : l.raw + r };
}

static inline L4_Clock_t operator + (const L4_Clock_t & l, const L4_Uint64 t)
{
    return (L4_Clock_t) { raw : l.raw + t };
}

static inline L4_Clock_t operator - (const L4_Clock_t & c, const int r)
{
    return (L4_Clock_t) { raw : c.raw - r };
}

static inline L4_Clock_t operator - (const L4_Clock_t & c, const L4_Uint64 r)
{
    return (L4_Clock_t) { raw : c.raw - r };
}
#endif /* __cplusplus */


#if defined(__cplusplus)
static inline L4_Bool_t operator < (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw < r.raw;
}

static inline L4_Bool_t operator > (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw > r.raw;
}

static inline L4_Bool_t operator <= (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw <= r.raw;
}

static inline L4_Bool_t operator >= (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw >= r.raw;
}

static inline L4_Bool_t operator == (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw == r.raw;
}

static inline L4_Bool_t operator != (const L4_Clock_t &l, const L4_Clock_t &r)
{
    return l.raw != r.raw;
}
#endif /* __cplusplus */
