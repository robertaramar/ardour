/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pbd/reallocpool.h"

#if defined RAP_WITH_SEGMENT_STATS || defined RAP_WITH_CALL_STATS
#include <cstdio>
#endif

#ifdef RAP_WITH_SEGMENT_STATS
#define collectstats collect_segment_stats();
#else
#define collectstats
#endif

using namespace PBD;

typedef int poolsize_t;

ReallocPool::ReallocPool (std::string name, size_t bytes)
	: _name (name)
	, _poolsize (bytes)
	, _pool (0)
#ifdef RAP_WITH_SEGMENT_STATS
	, _cur_avail (0)
	, _cur_allocated (0)
	, _max_allocated (0)
	, _seg_cur_count (0)
	, _seg_max_count (0)
	, _seg_max_used (0)
	, _seg_max_avail (0)
#endif
#ifdef RAP_WITH_CALL_STATS
	, _n_alloc (0)
	, _n_grow (0)
	, _n_shrink (0)
	, _n_free (0)
	, _n_noop (0)
	, _n_oom (0)
#endif
{
	_pool = (char*) ::malloc (bytes); // XXX add sth for fragmentation overhead ?
	poolsize_t *in = (poolsize_t*) _pool;
	*in = - (bytes - sizeof (poolsize_t));
#ifdef RAP_TRAVERSE_MEM
	_mru = _pool;
#endif
#ifdef RAP_WITH_HISTOGRAM
	for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		_hist_alloc[i] = _hist_free[i] = _hist_grow[i] = _hist_shrink[i] = 0;
	}
#endif
}

ReallocPool::~ReallocPool ()
{
	collectstats;
	printstats ();
	::free (_pool);
	_pool = NULL;
}

void
ReallocPool::printstats ()
{
#ifdef RAP_WITH_SEGMENT_STATS
	printf ("ReallocPool '%s': used: %lu (%.1f%%) (max: %lu), free: %lu [bytes]\n"
			"|| segments: cur: %lu (max: %lu), largest-used: %lu, largest-free: %lu\n",
			_name.c_str(),
			_cur_allocated, _cur_allocated * 100.f / _poolsize, _max_allocated, _cur_avail,
			_seg_cur_count, _seg_max_count, _seg_max_used, _seg_max_avail);
#elif defined RAP_WITH_CALL_STATS
	printf ("ReallocPool '%s':\n", _name.c_str());
#endif
#ifdef RAP_WITH_CALL_STATS
		printf("|| malloc(): %lu free(): %lu realloc()+:%lu  realloc()-: %lu NOOP:%lu OOM:%lu\n",
			_n_alloc, _n_free, _n_grow, _n_shrink, _n_noop, _n_oom);
#endif
#ifdef RAP_WITH_HISTOGRAM
		printf("--- malloc()\n");
		print_histogram (_hist_alloc);
		printf("--- realloc()/grow-to\n");
		print_histogram (_hist_grow);
		printf("--- realloc()/shrink-to\n");
		print_histogram (_hist_shrink);
		printf("--- free() histogram\n");
		print_histogram (_hist_free);
		printf("--------------------\n");
#endif
}

#ifdef RAP_WITH_SEGMENT_STATS
void
ReallocPool::dumpsegments ()
{
	char *p = _pool;
	const poolsize_t sop = sizeof(poolsize_t);
	poolsize_t *in = (poolsize_t*) p;
	unsigned int traversed = 0;
	printf ("<<<<< %s\n", _name.c_str());
	while (1) {
		if ((*in) > 0) {
			printf ("0x%08x used %4d\n", traversed, *in);
			printf ("0x%08x   data %p\n", traversed + sop , p + sop);
			traversed += *in + sop;
			p += *in + sop;
		} else if ((*in) < 0) {
			printf ("0x%08x free %4d [+%d]\n", traversed, -*in, sop);
			traversed += -*in + sop;
			p += -*in + sop;
		} else {
			printf ("0x%08x Corrupt!\n", traversed);
			break;
		}
		in = (poolsize_t*) p;
		if (p == _pool + _poolsize) {
			printf ("%08x end\n", traversed);
			break;
		}
		if (p > _pool + _poolsize) {
			printf ("%08x Beyond End!\n", traversed);
			break;
		}
	}
	printf (">>>>>\n");
}

void
ReallocPool::collect_segment_stats ()
{
	char *p = _pool;
	poolsize_t *in = (poolsize_t*) p;

	_cur_allocated = _cur_avail = 0;
	_seg_cur_count = _seg_max_avail = _seg_max_used = 0;

	while (1) {
		++_seg_cur_count;
		assert (*in != 0);
		if ((*in) > 0) {
			_cur_allocated += *in;
			p += *in;
			if (*in > (poolsize_t)_seg_max_used) {
				_seg_max_used = *in;
			}
		} else {
			_cur_avail += -*in;
			p += -*in;
			if (-*in > (poolsize_t)_seg_max_avail) {
				_seg_max_avail = -*in;
			}
		}
		p += sizeof(poolsize_t);
		in = (poolsize_t*) p;
		if (p == _pool + _poolsize) {
			break;
		}
	}
	_seg_cur_count = _seg_cur_count;

	if (_cur_allocated > _max_allocated) {
		_max_allocated = _cur_allocated;
	}
	if (_seg_cur_count > _seg_max_count) {
		_seg_max_count = _seg_cur_count;
	}
}
#endif

#ifdef RAP_WITH_HISTOGRAM
void
ReallocPool::print_histogram (size_t const * const histogram) const
{
	size_t maxhist = 0;
	for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		if (histogram[i] > maxhist) maxhist = histogram[i];
	}
	const int termwidth = 50;
#ifdef RAP_BLOCKSIZE
	const int fact = RAP_BLOCKSIZE + 1;
#endif
	if (maxhist > 0) for (int i = 0; i < RAP_WITH_HISTOGRAM; ++i) {
		if (histogram[i] == 0) { continue; }
		if (i == RAP_WITH_HISTOGRAM -1 ) {
#ifdef RAP_BLOCKSIZE
			printf("     > %4d: %7lu ", i * fact, histogram[i]);
#else
			printf(">%4d:%7lu ", i * fact, histogram[i]);
#endif
		} else {
#ifdef RAP_BLOCKSIZE
			printf("%4d .. %4d: %7lu ", i * fact, (i + 1) * fact -1, histogram[i]);
#else
			printf("%4d: %7lu ", i * fact, (i + 1) * fact -1 , histogram[i]);
#endif
		}
		int bar_width = (histogram[i] * termwidth ) / maxhist;
		if (bar_width == 0 && histogram[i] > 0) bar_width = 1;
		for (int j = 0; j < bar_width; ++j) printf("#");
		printf("\n");
	}
}

unsigned int
ReallocPool::hist_bin (size_t s) const {
#ifdef RAP_BLOCKSIZE
	s = (s + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
	s /= (RAP_BLOCKSIZE + 1);
#endif
	if (s > RAP_WITH_HISTOGRAM - 1) s = RAP_WITH_HISTOGRAM - 1;
	return s;
}
#endif

#define SEGSIZ (*((poolsize_t*) p))

void
ReallocPool::consolidate_ptr (char *p) {
	assert (SEGSIZ < 0); // p must point to free mem
	const poolsize_t sop = sizeof(poolsize_t);
	if (p - SEGSIZ + sop >= _pool + _poolsize) {
		return; // reached end
	}
	poolsize_t *next = (poolsize_t*)(p - SEGSIZ + sop);
	while (*next < 0) {
		SEGSIZ = SEGSIZ + (*next) - sop;
		if (p - SEGSIZ + sop >= _pool + _poolsize) {
			break;
		}
		next = (poolsize_t*)(p -SEGSIZ + sop);
	}
#ifdef RAP_TRAVERSE_MEM
	_mru = p; // TODO IFF it was inside the consolidated area?
#endif
}

void *
ReallocPool::_malloc (size_t s) {
	assert (_pool);
	const poolsize_t sop = sizeof(poolsize_t);
#ifdef RAP_TRAVERSE_MEM
	size_t traversed = 0;
	char *p = _mru;
#else
	char *p = _pool;
#endif

#ifdef RAP_BLOCKSIZE
	s = (s + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE); // optional, helps to reduce fragmentation
#endif

	while (1) { // iterates at most once over the available pool
		while (SEGSIZ > 0) {
#ifdef RAP_TRAVERSE_MEM
			traversed += SEGSIZ + sop;
			if (traversed >= _poolsize) {
				return NULL; // reached last segment. OOM.
			}
#endif
			p += SEGSIZ + sop;
			if (p == _pool + _poolsize) {
#ifdef RAP_TRAVERSE_MEM
				p = _pool;
#else
				return NULL; // reached last segment. OOM.
#endif
			}
		}

		assert (p < _pool + _poolsize);

		// found free segment.
		const poolsize_t avail = -SEGSIZ;
		const poolsize_t sp = (poolsize_t)s;
		const poolsize_t ss = sop + s;

		if (sp == avail) {
			// exact match
			SEGSIZ = -SEGSIZ;
			return (p + sop);
		}

		if (ss < avail) {
			// segment is larger than required space,
			// (we need to fit data + two non-zero poolsize_t)
			SEGSIZ = sp; // mark area as used.
			*((poolsize_t*)(p + ss)) = ss - avail; // mark free space after.
			consolidate_ptr (p + ss);
#ifdef RAP_TRAVERSE_MEM
			_mru = p + ss;
#endif
			return (p + sop);
		}

		// segment is not large enough
		consolidate_ptr (p); // try to consolidate with next segment (if any)

		// check segment (again) and skip over too small free ones
		while (SEGSIZ < 0 && (-SEGSIZ) <= ss && (-SEGSIZ) != sp) {
#ifdef RAP_TRAVERSE_MEM
			traversed += -SEGSIZ + sop;
			if (traversed >= _poolsize) {
				return NULL; // reached last segment. OOM.
			}
#endif
			p += (-SEGSIZ) + sop;
			if (p >= _pool + _poolsize) {
#ifdef RAP_TRAVERSE_MEM
				p = _pool;
				if (SEGSIZ < 0) consolidate_ptr (p);
#else
				return NULL; // reached last segment. OOM.
#endif
			}
		}
	}
}
#undef SEGSIZ

size_t
ReallocPool::_asize (void *ptr) {
	if (ptr == 0) return 0;
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	return (*in);
}

void
ReallocPool::_free (void *ptr) {
	assert (_pool && ptr);
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	assert ((char*)ptr >= _pool && *in > 0);
	assert ((char*)ptr + *in <= _pool + _poolsize);
	*in = -*in; // mark as free

	//optional, helps to spread load evenly
#ifdef RAP_TRAVERSE_MEM
	_mru = (char*)in; // consolidate in malloc()
#else
	consolidate_ptr ((char*)in);
#endif
}

void
ReallocPool::_shrink (void *ptr, size_t newsize) {
	assert (_pool && ptr);
	poolsize_t *in = (poolsize_t*) ptr;
	--in;
	assert ((char*)ptr >= _pool && *in > 0);
	assert ((char*)ptr + *in <= _pool + _poolsize);

	const poolsize_t avail = *in;
	const poolsize_t ss = newsize + sizeof(poolsize_t);
	if (avail <= ss) {
		return; // can't shrink
	}
	const poolsize_t sp = (poolsize_t)newsize;
	*in = sp; // set new size
	char *p = (char*) in;
	*((poolsize_t*)(p + ss)) = ss - avail; // mark free space after.
#ifdef RAP_TRAVERSE_MEM
	_mru = p + ss;
#else
	consolidate_ptr (p + ss);
#endif
}

void *
ReallocPool::_realloc (void *ptr, size_t oldsize, size_t newsize) {
	void *rv = NULL;

	if (ptr == 0 && newsize == 0) {
#ifdef RAP_WITH_CALL_STATS
		++_n_noop;
#endif
		return NULL;
	}

	if (ptr == 0) {
		rv = _malloc (newsize);
#ifdef RAP_WITH_CALL_STATS
		if (!rv) {
			++_n_oom;
		}
		++_n_alloc;
#endif
#ifdef RAP_WITH_HISTOGRAM
		++_hist_alloc[hist_bin(newsize)];
#endif
		collectstats;
		return rv;
	}

	if (newsize == 0) {
#ifdef RAP_WITH_HISTOGRAM
		++_hist_free[hist_bin(_asize(ptr))];
#endif
		_free (ptr);
#ifdef RAP_WITH_CALL_STATS
		++_n_free;
#endif
		collectstats;
		return NULL;
	}

	if (newsize == oldsize) {
#ifdef RAP_WITH_CALL_STATS
		++_n_noop;
#endif
		return ptr;
	}

	if (newsize > oldsize) {
#ifdef RAP_BLOCKSIZE
		const size_t ns = (newsize + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
		if (ns <= _asize(ptr)) {
#ifdef RAP_WITH_CALL_STATS
		++_n_noop;
#endif
			return ptr;
		}
#endif
		// TODO check if it's in the same block.. -> NOOP
		if ((rv = _malloc (newsize))) {
			memcpy (rv, ptr, oldsize);
#ifdef RAP_WITH_CALL_STATS
		} else {
			++_n_oom;
#endif
		}
		_free (ptr);
#ifdef RAP_WITH_CALL_STATS
		++_n_grow;
#endif
#ifdef RAP_WITH_HISTOGRAM
		++_hist_grow[hist_bin(newsize)];
#endif
		collectstats;
		return rv;
	}

	if (newsize < oldsize) {
#if 0 // re-allocate
		if ((rv = _malloc (newsize))) {
			memccpy (rv, ptr, newsize);
#ifdef RAP_WITH_CALL_STATS
		} else {
			++_n_oom;
#endif
		}
		_free (ptr);
#elif 1 // shrink current segment
		const size_t ns = (newsize + RAP_BLOCKSIZE) & (~RAP_BLOCKSIZE);
		_shrink (ptr, ns);
		rv = ptr;
#else
		// do nothing
		rv = ptr;
#endif
#ifdef RAP_WITH_CALL_STATS
		++_n_shrink;
#endif
#ifdef RAP_WITH_HISTOGRAM
		++_hist_shrink[hist_bin(newsize)];
#endif
		collectstats;
		return rv;
	}

	assert (0);
	return NULL;
}
