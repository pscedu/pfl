/* $Id$ */
/*
 * %PSC_START_COPYRIGHT%
 * -----------------------------------------------------------------------------
 * Copyright (c) 2006-2009, Pittsburgh Supercomputing Center (PSC).
 *
 * Permission to use, copy, and modify this software and its documentation
 * without fee for personal use or non-commercial use within your organization
 * is hereby granted, provided that the above copyright notice is preserved in
 * all copies and that the copyright and this permission notice appear in
 * supporting documentation.  Permission to redistribute this software to other
 * organizations or individuals is not permitted without the written permission
 * of the Pittsburgh Supercomputing Center.  PSC makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * -----------------------------------------------------------------------------
 * %PSC_END_COPYRIGHT%
 */

#define _PFL_ASM(code, ...)		__asm__ __volatile__("lock; " code, ## __VA_ARGS__)
#define _PFL_NL_ASM(code, ...)		__asm__ __volatile__(code, ## __VA_ARGS__)

struct psc_i386_atomic64 {
	volatile int64_t value64;
	psc_spinlock_t lock64;
} __packed;

#undef psc_atomic64_t
#define psc_atomic64_t struct psc_i386_atomic64

#define PSC_ATOMIC16_INIT(i)		{ i }
#define PSC_ATOMIC32_INIT(i)		{ i }
#define PSC_ATOMIC64_INIT(i)		{ (i), LOCK_INITIALIZER }

#define _PFL_GETA16(v)			((v)->value16)
#define _PFL_GETA32(v)			((v)->value32)
#define _PFL_GETA64(v)			((v)->value64)

#undef psc_atomic16_read
static __inline int16_t
psc_atomic16_read(const psc_atomic16_t *v)
{
	return (_PFL_GETA16(v));
}

#undef psc_atomic32_read
static __inline int32_t
psc_atomic32_read(const psc_atomic32_t *v)
{
	return (_PFL_GETA32(v));
}

#undef psc_atomic64_read
static __inline int64_t
psc_atomic64_read(const psc_atomic64_t *v)
{
	uint64_t val;

	spinlock(&v->lock64);
	val = _PFL_GETA64(v);
	freelock(&v->lock64);
	return (val);
}

#undef psc_atomic16_set
static __inline void
psc_atomic16_set(psc_atomic16_t *v, int64_t i)
{
	_PFL_GETA16(v) = i;
}

#undef psc_atomic32_set
static __inline void
psc_atomic32_set(psc_atomic32_t *v, int64_t i)
{
	_PFL_GETA32(v) = i;
}

#undef psc_atomic64_set
static __inline void
psc_atomic64_set(psc_atomic64_t *v, int64_t i)
{
	_PFL_GETA64(v) = i;
}

#undef psc_atomic16_add
static __inline void
psc_atomic16_add(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("addw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

#undef psc_atomic32_add
static __inline void
psc_atomic32_add(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("addl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

#if 0
#undef psc_atomic64_add
static __inline void
psc_atomic64_add(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("addq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}
#endif

#undef psc_atomic16_sub
static __inline void
psc_atomic16_sub(psc_atomic16_t *v, int16_t i)
{
	_PFL_ASM("subw %1,%0" : "=m" _PFL_GETA16(v) : "ir" (i), "m" _PFL_GETA16(v));
}

#undef psc_atomic32_sub
static __inline void
psc_atomic32_sub(psc_atomic32_t *v, int32_t i)
{
	_PFL_ASM("subl %1,%0" : "=m" _PFL_GETA32(v) : "ir" (i), "m" _PFL_GETA32(v));
}

#undef psc_atomic64_sub
static __inline void
psc_atomic64_sub(psc_atomic64_t *v, int64_t i)
{
	_PFL_ASM("subq %1,%0" : "=m" _PFL_GETA64(v) : "ir" (i), "m" _PFL_GETA64(v));
}

#undef psc_atomic16_sub_and_test0
static __inline int
psc_atomic16_sub_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("subw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_sub_and_test0
static __inline int
psc_atomic32_sub_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("subl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_sub_and_test0
static __inline int
psc_atomic64_sub_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("subq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_add_and_test0
static __inline int
psc_atomic16_add_and_test0(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("addw %2, %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_add_and_test0
static __inline int
psc_atomic32_add_and_test0(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("addl %2, %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_add_and_test0
static __inline int
psc_atomic64_add_and_test0(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("addq %2, %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_inc
static __inline void
psc_atomic16_inc(psc_atomic16_t *v)
{
	_PFL_ASM("incw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

#undef psc_atomic32_inc
static __inline void
psc_atomic32_inc(psc_atomic32_t *v)
{
	_PFL_ASM("incl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

#if 0
#undef psc_atomic64_inc
static __inline void
psc_atomic64_inc(psc_atomic64_t *v)
{
	_PFL_ASM("incq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}
#endif

#undef psc_atomic16_dec
static __inline void
psc_atomic16_dec(psc_atomic16_t *v)
{
	_PFL_ASM("decw %0" : "=m" _PFL_GETA16(v) : "m" _PFL_GETA16(v));
}

#undef psc_atomic32_dec
static __inline void
psc_atomic32_dec(psc_atomic32_t *v)
{
	_PFL_ASM("decl %0" : "=m" _PFL_GETA32(v) : "m" _PFL_GETA32(v));
}

#if 0
#undef psc_atomic64_dec
static __inline void
psc_atomic64_dec(psc_atomic64_t *v)
{
	_PFL_ASM("decq %0" : "=m" _PFL_GETA64(v) : "m" _PFL_GETA64(v));
}
#endif

#undef psc_atomic16_inc_and_test0
static __inline int
psc_atomic16_inc_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("incw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_inc_and_test0
static __inline int
psc_atomic32_inc_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("incl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_inc_and_test0
static __inline int
psc_atomic64_inc_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("incq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_dec_and_test0
static __inline int
psc_atomic16_dec_and_test0(psc_atomic16_t *v)
{
	unsigned char c;

	_PFL_ASM("decw %0; sete %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_dec_and_test0
static __inline int
psc_atomic32_dec_and_test0(psc_atomic32_t *v)
{
	unsigned char c;

	_PFL_ASM("decl %0; sete %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_dec_and_test0
static __inline int
psc_atomic64_dec_and_test0(psc_atomic64_t *v)
{
	unsigned char c;

	_PFL_ASM("decq %0; sete %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_add_and_test_neg
static __inline int
psc_atomic16_add_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("addw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_add_and_test_neg
static __inline int
psc_atomic32_add_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("addl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_add_and_test_neg
static __inline int
psc_atomic64_add_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("addq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_sub_and_test_neg
static __inline int
psc_atomic16_sub_and_test_neg(psc_atomic16_t *v, int16_t i)
{
	unsigned char c;

	_PFL_ASM("subw %2, %0; sets %1" : "=m" _PFL_GETA16(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA16(v) : "memory");
	return (c);
}

#undef psc_atomic32_sub_and_test_neg
static __inline int
psc_atomic32_sub_and_test_neg(psc_atomic32_t *v, int32_t i)
{
	unsigned char c;

	_PFL_ASM("subl %2, %0; sets %1" : "=m" _PFL_GETA32(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA32(v) : "memory");
	return (c);
}

#undef psc_atomic64_sub_and_test_neg
static __inline int
psc_atomic64_sub_and_test_neg(psc_atomic64_t *v, int64_t i)
{
	unsigned char c;

	_PFL_ASM("subq %2, %0; sets %1" : "=m" _PFL_GETA64(v),
	    "=qm" (c) : "ir" (i), "m" _PFL_GETA64(v) : "memory");
	return (c);
}

#undef psc_atomic16_add_getnew
static __inline int16_t
psc_atomic16_add_getnew(psc_atomic16_t *v, int16_t i)
{
	int16_t adj = i;

	_PFL_ASM("xaddw %0, %1" : "+r" (i),
	    "+m" _PFL_GETA16(v) : : "memory");
	return (i + adj);
}

#undef psc_atomic32_add_getnew
static __inline int32_t
psc_atomic32_add_getnew(psc_atomic32_t *v, int32_t i)
{
	int32_t adj = i;

	_PFL_ASM("xaddl %0, %1" : "+r" (i),
	    "+m" _PFL_GETA32(v) : : "memory");
	return (i + adj);
}

#undef psc_atomic64_add_getnew
static __inline int64_t
psc_atomic64_add_getnew(psc_atomic64_t *v, int64_t i)
{
	int64_t adj = i;

	_PFL_ASM("xaddq %0, %1" : "+r" (i),
	    "+m" _PFL_GETA64(v) : : "memory");
	return (i + adj);
}

#undef psc_atomic16_clearmask
static __inline void
psc_atomic16_clearmask(psc_atomic16_t *v, int16_t mask)
{
	mask = ~mask;
	_PFL_ASM("andw %0, %1" : : "r" (mask),
	    "m" _PFL_GETA16(v) : "memory");
}

#undef psc_atomic32_clearmask
static __inline void
psc_atomic32_clearmask(psc_atomic32_t *v, int32_t mask)
{
	mask = ~mask;
	_PFL_ASM("andl %0, %1" : : "r" (mask),
	    "m" _PFL_GETA32(v) : "memory");
}

#undef psc_atomic64_clearmask
static __inline void
psc_atomic64_clearmask(psc_atomic64_t *v, int64_t mask)
{
	mask = ~mask;
	_PFL_ASM("andq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic16_setmask
static __inline void
psc_atomic16_setmask(psc_atomic16_t *v, int16_t mask)
{
	_PFL_ASM("orw %0, %1" : : "r" (mask),
	    "m" _PFL_GETA16(v) : "memory");
}

#undef psc_atomic32_setmask
static __inline void
psc_atomic32_setmask(psc_atomic32_t *v, int32_t mask)
{
	_PFL_ASM("orl %0, %1" : : "r" (mask),
	    "m" _PFL_GETA32(v) : "memory");
}

#undef psc_atomic64_setmask
static __inline void
psc_atomic64_setmask(psc_atomic64_t *v, int64_t mask)
{
	_PFL_ASM("orq %0, %1" : : "r" (mask),
	    "m" _PFL_GETA64(v) : "memory");
}

#undef psc_atomic16_clearmask_getnew
static __inline int16_t
psc_atomic16_clearmask_getnew(psc_atomic16_t *v, int16_t mask)
{
	int16_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andw %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA16(v), "0" (oldv));
	return (oldv & ~mask);
}

#undef psc_atomic32_clearmask_getnew
static __inline int32_t
psc_atomic32_clearmask_getnew(psc_atomic32_t *v, int32_t mask)
{
	int32_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andl %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA32(v), "0" (oldv));
	return (oldv & ~mask);
}

#undef psc_atomic64_clearmask_getnew
static __inline int64_t
psc_atomic64_clearmask_getnew(psc_atomic64_t *v, int64_t mask)
{
	int64_t oldv = mask;

	mask = ~mask;
	_PFL_ASM("andq %0, %1;" : "=r" (mask)
	    : "m" _PFL_GETA64(v), "0" (oldv));
	return (oldv & ~mask);
}

#undef psc_atomic16_setmask_getnew
static __inline int16_t
psc_atomic16_setmask_getnew(psc_atomic16_t *v, int16_t i)
{
	int16_t oldv = i;

	_PFL_ASM("orw %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA16(v), "0" (oldv));
	return (oldv | i);
}

#undef psc_atomic32_setmask_getnew
static __inline int32_t
psc_atomic32_setmask_getnew(psc_atomic32_t *v, int32_t i)
{
	int32_t oldv = i;

	_PFL_ASM("orl %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA32(v), "0" (oldv));
	return (oldv | i);
}

#undef psc_atomic64_setmask_getnew
static __inline int64_t
psc_atomic64_setmask_getnew(psc_atomic64_t *v, int64_t i)
{
	int64_t oldv = i;

	_PFL_ASM("orq %0, %1;" : "=r" (i)
	    : "m" _PFL_GETA64(v), "0" (oldv));
	return (oldv | i);
}

#undef psc_atomic16_xchg
static __inline int16_t
psc_atomic16_xchg(psc_atomic16_t *v, int16_t i)
{
	_PFL_NL_ASM("xchgw %0, %1" : "=r" (i)
	    : "m" _PFL_GETA16(v), "0" (i) : "memory");
	return (i);
}

#undef psc_atomic32_xchg
static __inline int32_t
psc_atomic32_xchg(psc_atomic32_t *v, int32_t i)
{
	_PFL_NL_ASM("xchgl %0, %1" : "=r" (i)
	    : "m" _PFL_GETA32(v), "0" (i) : "memory");
	return (i);
}

#undef psc_atomic64_xchg
static __inline int64_t
psc_atomic64_xchg(psc_atomic64_t *v, int64_t i)
{
	_PFL_NL_ASM("xchgq %0, %1" : "=r" (i)
	    : "m" _PFL_GETA64(v), "0" (i) : "memory");
	return (i);
}

#undef psc_atomic16_cmpxchg
static __inline int16_t
psc_atomic16_cmpxchg(psc_atomic16_t *v, int16_t cmpv, int16_t newv)
{
	int16_t oldv;

	_PFL_ASM("cmpxchgw %w1, %2" : "=a" (oldv) : "r" (newv),
	    "m" _PFL_GETA16(v), "0" (cmpv) : "memory");
	return (oldv);
}

#undef psc_atomic32_cmpxchg
static __inline int32_t
psc_atomic32_cmpxchg(psc_atomic32_t *v, int32_t cmpv, int32_t newv)
{
	int32_t oldv;

	_PFL_ASM("cmpxchgl %k1,%2" : "=a" (oldv) : "r" (newv),
	    "m" _PFL_GETA32(v), "0" (cmpv) : "memory");
	return (oldv);
}

#undef psc_atomic64_cmpxchg
static __inline int64_t
psc_atomic64_cmpxchg(psc_atomic64_t *v, int64_t cmpv, int64_t newv)
{
	int64_t oldv;

#if cmpxchg8b
	_PFL_ASM("cmpxchg8b %3" : "=A" (oldv) : "b" ((int32_t)newv),
	    "c" ((uint64_t)newv >> 32), "m" _PFL_GETA64(v),
	    "0" (cmpv) : "memory");
#else
	spinlock(&v->lock64);
	oldv = _PFL_GETA64(v);
	if (oldv == cmpv)
		_PFL_GETA64(v) = newv;
	freelock(&v->lock64);
#endif
	return (oldv);
}
