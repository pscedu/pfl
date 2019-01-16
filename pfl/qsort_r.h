/*
 * %ISC_LICENSE%
 */

#ifndef _PFL_QSORT_H_
#define _PFL_QSORT_H_

#ifdef HAVE_QSORT_R_THUNK
#  define pfl_qsort_r(base, nel, wid, cmpf, arg)			\
	qsort_r((base), (nel), (wid), (arg), (cmpf))
#else
#  define pfl_qsort_r(base, nel, wid, cmpf, arg)			\
	qsort_r((base), (nel), (wid), (cmpf), (arg))
#endif

#ifndef HAVE_QSORT_R
void qsort_r(void *, size_t, size_t,
    int (*)(const void *, const void *, void *), void *);
#endif

#endif /* _PFL_QSORT_H_ */
