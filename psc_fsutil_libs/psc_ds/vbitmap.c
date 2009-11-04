/* $Id$ */

/*
 * Variable-sized bitmaps.  Internally, bitmaps are arrays
 * of chars that are realloc(3)'d to different lengths.
 *
 * This API is not thread-safe!
 */

#include <sys/param.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "psc_ds/vbitmap.h"
#include "psc_util/alloc.h"
#include "psc_util/bitflag.h"
#include "pfl/cdefs.h"
#include "psc_util/log.h"

#define CLEAR_UNUSED(vb, p)		*(p) &= ~(0xff << (vb)->vb_lastsize)

/**
 * psc_vbitmap_new - create a new variable-sized bitmap.
 * @nelems: number of entries in bitmap.
 * @flags: operational flags.
 */
struct psc_vbitmap *
psc_vbitmap_newf(size_t nelems, int flags)
{
	struct psc_vbitmap *vb;
	size_t bytes;

	vb = PSCALLOC(sizeof(*vb));
	vb->vb_flags = flags;

	bytes = howmany(nelems, NBBY);
	vb->vb_start = PSCALLOC(bytes);
	vb->vb_pos = vb->vb_start;
	if (bytes)
		vb->vb_end = vb->vb_start + bytes - 1;
	else
		vb->vb_end = vb->vb_start;
	vb->vb_lastsize = nelems % NBBY;
	if (nelems && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (vb);
}

/**
 * psc_vbitmap_attach - Initialize a variable bitmap from a chunk of memory.
 * @buf: memory where to read bitmap from.
 * @size: length of memory buffer.
 */
struct psc_vbitmap *
psc_vbitmap_attach(unsigned char *buf, size_t size)
{
	struct psc_vbitmap *vb;

	vb = PSCALLOC(sizeof(*vb));
	vb->vb_flags |= PVBF_EXTALLOC;
	vb->vb_pos = vb->vb_start = buf;
	vb->vb_end = buf + (size - 1);
	vb->vb_lastsize = NBBY;
	return (vb);
}

void
_psc_vbitmap_free(struct psc_vbitmap *vb)
{
	if ((vb->vb_flags & PVBF_EXTALLOC) == 0)
		free(vb->vb_start);
	vb->vb_start = NULL;
	vb->vb_end = NULL;
	vb->vb_pos = NULL;
	if ((vb->vb_flags & PVBF_STATIC) == 0)
		free(vb);
}

/**
 * psc_vbitmap_unset - unset a bit of a vbitmap.
 * @vb: variable bitmap.
 * @pos: position to unset.
 */
void
psc_vbitmap_unset(struct psc_vbitmap *vb, size_t pos)
{
	size_t shft, bytes;

	bytes = pos / NBBY;
	shft = pos % NBBY;
	vb->vb_start[bytes] &= ~(1 << shft);
}

/**
 * psc_vbitmap_set - set a bit of a vbitmap.
 * @vb: variable bitmap.
 * @pos: position to set.
 */
void
psc_vbitmap_set(struct psc_vbitmap *vb, size_t pos)
{
	size_t shft, bytes;

	bytes = pos / NBBY;
	shft = pos % NBBY;
	vb->vb_start[bytes] |= (1 << shft);
}

/**
 * psc_vbitmap_setrange - set bits of a vbitmap.
 * @vb: variable bitmap.
 * @pos: starting position to set.
 * @n: length of region (# of bits) to set.
 */
int
psc_vbitmap_setrange(struct psc_vbitmap *vb, size_t pos, size_t size)
{
	size_t shft, bytes;
	unsigned char *p;

	if (pos + size > psc_vbitmap_getsize(vb))
		return (EINVAL);

	bytes = pos / NBBY;
	shft = pos % NBBY;

	p = &vb->vb_start[bytes];

	/* set bits in first byte */
	if (shft)
		*p++ |= (~(0xff << (MAX(size, 8) + shft))) &
		    (~(1 << MAX(size, 8)) << shft);

	/* set whole bytes */
	for (; size >= 8; p++, size -= 8)
		*p = 0xff;

	/* set bits in last byte */
	if (size)
		*p |= (~(0xff << size)) & (~(1 << size));
	return (0);
}

/**
 * psc_vbitmap_xset - exclusively set a bit of a vbitmap.
 * @vb: variable bitmap.
 * @elem: element# to set.
 *
 * Returns -1 if already set.
 */
int
psc_vbitmap_xset(struct psc_vbitmap *vb, size_t elem)
{
	if (psc_vbitmap_get(vb, elem))
		return (-1);
	psc_vbitmap_set(vb, elem);
	return (0);
}

/**
 * psc_vbitmap_get - get bit for an element of a vbitmap.
 * @vb: variable bitmap.
 * @elem: element # to get.
 */
int
psc_vbitmap_get(const struct psc_vbitmap *vb, size_t elem)
{
	size_t pos, bytes;

	bytes = elem / NBBY;
	pos = elem % NBBY;
	return (vb->vb_start[bytes] & (1 << pos));
}

__static __inline int
bs_nfree(int b, int m)
{
	int i, n;

	if (b == 0)
		return (NBBY);
	if (b == 0xff)
		return (0);
	for (i = n = 0; i < m; i++)
		if ((b & (1 << i)) == 0)
			n++;
	return (n);
}

/**
 * psc_vbitmap_nfree - Get the number of free (i.e. unset) bits
 *	in a variable bitmap.
 * @vb: variable bitmap.
 * Returns: number of free bits.
 */
int
psc_vbitmap_nfree(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	int n;

	for (n = 0, p = vb->vb_start; p < vb->vb_end; p++)
		n += bs_nfree(*p, NBBY);
	n += bs_nfree(*p, vb->vb_lastsize);
	return (n);
}

/**
 * psc_vbitmap_invert - invert the state of all bits in a vbitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_invert(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		*p ^= 1;
}

/**
 * psc_vbitmap_setall - toggle on all bits in a vbitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_setall(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		*p = 0xff;
}

/**
 * psc_vbitmap_clearall - toggle off all bits in a vbitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_clearall(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		*p = 0;
}

/**
 * psc_vbitmap_isfull - Determine if there are any
 *	empty slots in a vbitmap.
 * @vb: variable bitmap.
 */
int
psc_vbitmap_isfull(struct psc_vbitmap *vb)
{
	unsigned char *p;

	for (p = vb->vb_start; p < vb->vb_end; p++)
		if (*p != 0xff)
			return (0);
	return (ffs(~*p) > vb->vb_lastsize);
}

/**
 * psc_vbitmap_lcr - report the largest contiguous region in the bitmap.
 * @vb: variable bitmap.
 * Returns: size of the region.
 */
int
psc_vbitmap_lcr(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	int i, n=0, r=0;

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		/* ensure unused bits are masked off */
		if (p == vb->vb_end && vb->vb_lastsize)
			CLEAR_UNUSED(vb, p);

		if (*p == 0x00)
			n += NBBY;
		else if (*p == 0xff) {
			if (n > r)
				r = n;
			n = 0;
		} else {
			for (i = 0; i < NBBY; i++) {
				if ((*p & (1 << i)) == 0)
					n++;
				else {
					if (n > r)
						r = n;
					n = 0;
				}
			}
		}
	if (n > r)
		r = n;
	return (r);
}

/**
 * psc_vbitmap_getncontig - try to get 'N' contiguous slots (or bits)
 * @vb: variable bitmap.
 * @nslots:  as an input parameter, requests 'N' number of slots.
 *	On output, informs the caller of the starting slot.
 * Returns: number of slots assigned, 0 for none.
 *
 * XXX: adjust this to take into account vb_lastsize.
 */
int
psc_vbitmap_getncontig(struct psc_vbitmap *vb, int *nslots)
{
	unsigned char *p;
	int i=0, sbit=0, ebit=0, t1=0, t2=0;

#define TEST_AND_SET					\
	do {						\
		if ((t2 - t1) > (ebit - sbit)) {	\
			sbit = t1;			\
			ebit = t2;			\
		}					\
		t1 = t2 + 1;				\
	} while (0)

	for (p = vb->vb_start; p <= vb->vb_end; p++)
		for (i = 0; i < NBBY; i++, t2++) {
			if (*p & (1 << i))
				TEST_AND_SET;
			else if (t2 - t1 == *nslots)
				goto mark_bits;
		}
 mark_bits:
	TEST_AND_SET;

	if (ebit - sbit) {
		for (i = sbit; i < ebit; i++)
			vb->vb_start[i / NBBY] |= 1 << (i % NBBY);
		/* Inform the caller of the start bit */
		*nslots = sbit;
	}
	return (ebit - sbit);
}

/**
 * psc_vbitmap_next - return next unused slot from a vbitmap.
 * @vb: variable bitmap.
 * @elem: pointer to element#.
 * Returns: true on success.
 */
int
psc_vbitmap_next(struct psc_vbitmap *vb, size_t *elem)
{
	unsigned char *start, *pos;
	int bytepos;

 retry:
	pos = start = vb->vb_pos;
	do {
		/* Check if byte is completely full. */
		if (pos == vb->vb_end) {
			if (vb->vb_lastsize == NBBY) {
				if (*pos != 0xff)
					goto found;
			} else if (pos && *pos != ~(char)(0x100 -
			    (1 << vb->vb_lastsize)))
				goto found;
			pos = vb->vb_start;	/* byte is full, advance */
		} else {
			if (*pos != 0xff)
				goto found;
			pos++;			/* byte is full, advance */
		}
	} while (pos != start);

	if ((vb->vb_flags & (PVBF_AUTO | PVBF_EXTALLOC)) == PVBF_AUTO) {
		int newsiz;

		newsiz = psc_vbitmap_getsize(vb) + 1;
		if (psc_vbitmap_resize(vb, newsiz) == -1)
			return (-1);
		psc_vbitmap_setnextpos(vb, newsiz);
		goto retry;
	}
	return (0);

 found:
	/* We now have a byte from the bitmap that has a zero. */
	vb->vb_pos = pos;
	bytepos = ffs(~*pos) - 1;
	*pos |= 1 << bytepos;
	*elem = NBBY * (pos - vb->vb_start) + bytepos;
	return (1);
}

/**
 * psc_vbitmap_setnextpos - Set position where psc_vbitmap_next() looks
 *	for next unset bit.
 * @vb: variable bitmap.
 * @slot: bit position where searching will continue from.
 * Returns zero on success or errno on error.
 */
int
psc_vbitmap_setnextpos(struct psc_vbitmap *vb, int slot)
{
	if (slot)
		slot >>= 3;
	if (slot < 0 || vb->vb_start + slot > vb->vb_end)
		return (EINVAL);
	vb->vb_pos = vb->vb_start + slot;
	return (0);
}

/**
 * psc_vbitmap_resize - resize a bitmap.
 * @vb: variable bitmap.
 * @newsize: new size the bitmap should take on.
 */
int
psc_vbitmap_resize(struct psc_vbitmap *vb, size_t newsize)
{
	unsigned char *start;
	ptrdiff_t pos, end;
	size_t siz;

	pos = vb->vb_pos - vb->vb_start;
	end = vb->vb_end - vb->vb_start;

	siz = howmany(newsize, NBBY);
	start = psc_realloc(vb->vb_start, siz, 0);
	/* special case for resizing NULL vbitmaps */
	if (vb->vb_start == NULL)
		memset(start, 0, siz);
	vb->vb_start = start;
	if (siz)
		vb->vb_end = start + siz - 1;
	else
		vb->vb_end = start;
	vb->vb_lastsize = newsize % NBBY;
	vb->vb_pos = start + pos;
	if (vb->vb_pos > vb->vb_end)
		vb->vb_pos = vb->vb_start;

	/* Initialize new sections of the bitmap to zero. */
	if (siz > (size_t)end)
		memset(start + end + 1, 0, siz - end - 1);
	if (newsize && vb->vb_lastsize == 0)
		vb->vb_lastsize = NBBY;
	return (0);
}

/**
 * psc_vbitmap_getsize - get the number of elements a bitmap represents.
 * @vb: variable bitmap.
 */
size_t
psc_vbitmap_getsize(const struct psc_vbitmap *vb)
{
	return ((vb->vb_end - vb->vb_start) * NBBY + vb->vb_lastsize);
}

/**
 * psc_vbitmap_printbin - print the contents of a bitmap in binary.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printbin(const struct psc_vbitmap *vb)
{
	unsigned char *p;
	int j;

	for (p = vb->vb_start; p < vb->vb_end; p++) {
		printf("%d%d%d%d%d%d%d%d ",
		    (*p >> 0) & 1, (*p >> 1) & 1,
		    (*p >> 2) & 1, (*p >> 3) & 1,
		    (*p >> 4) & 1, (*p >> 5) & 1,
		    (*p >> 6) & 1, (*p >> 7) & 1);
		if (((p + 1 - vb->vb_start) % 8) == 0 ||
		    p == vb->vb_end)
			printf("\n");
	}
	for (j = 0; j < vb->vb_lastsize; j++)
		printf("%d", (*p >> j) & 1);
	if (vb->vb_lastsize)
		printf("\n");
}

/**
 * psc_vbitmap_printhex - print the contents of a bitmap in hexadecimal.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printhex(const struct psc_vbitmap *vb)
{
	const unsigned char *p;

	for (p = vb->vb_start; p <= vb->vb_end; p++) {
		printf("%02x", *p);
		if (((p + 1 - vb->vb_start) % 32) == 0 ||
		    p == vb->vb_end)
			printf("\n");
		else if (((p + 1 - vb->vb_start) % 4) == 0 ||
		    p == vb->vb_end)
			printf(" ");
	}
}

/**
 * psc_vbitmap_getstats - gather the statistics of a bitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_getstats(const struct psc_vbitmap *vb, int *used, int *total)
{
	const unsigned char *p;

	*used = 0;
	for (p = vb->vb_start; p <= vb->vb_end; p++)
		*used += psc_countbits(*p);
	*total = psc_vbitmap_getsize(vb);
}

/**
 * psc_vbitmap_printstats - print the statistics of a bitmap.
 * @vb: variable bitmap.
 */
void
psc_vbitmap_printstats(const struct psc_vbitmap *vb)
{
	int used, total;

	psc_vbitmap_getstats(vb, &used, &total);

	printf("vbitmap statistics: %d/%d (%.4f%%) in use\n", used, total,
	    100.0 * used / total);
}
