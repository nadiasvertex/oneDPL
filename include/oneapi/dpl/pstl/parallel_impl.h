// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This file incorporates work covered by the following copyright and permission
// notice:
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//

#ifndef _ONEDPL_PARALLEL_IMPL_H
#define _ONEDPL_PARALLEL_IMPL_H

#include <atomic>
// This header defines the minimum set of parallel routines required to support Parallel STL,
// implemented on top of Intel(R) Threading Building Blocks (Intel(R) TBB) library

namespace oneapi
{
namespace dpl
{
namespace __internal
{

//------------------------------------------------------------------------
// parallel_find
//-----------------------------------------------------------------------
/** Return extremum value returned by brick f[i,j) for subranges [i,j) of [first,last)
Each f[i,j) must return a value in [i,j). */
template <class _ExecutionPolicy, class _Index, class _Brick, class _IsFirst>
_Index
__parallel_find(_ExecutionPolicy&& __exec, _Index __first, _Index __last, _Brick __f, _IsFirst)
{
    typedef typename ::std::iterator_traits<_Index>::difference_type _DifferenceType;
    const _DifferenceType __n = __last - __first;
    _DifferenceType __initial_dist = _IsFirst::value ? __n : -1;

    constexpr auto __comp = typename ::std::conditional<_IsFirst::value, __pstl_less, __pstl_greater>::type{};

    ::std::atomic<_DifferenceType> __extremum(__initial_dist);
    // TODO: find out what is better here: parallel_for or parallel_reduce
    __par_backend::__parallel_for(::std::forward<_ExecutionPolicy>(__exec), __first, __last,
                                  [__comp, __f, __first, &__extremum](_Index __i, _Index __j)
                                  {
                                      // See "Reducing Contention Through Priority Updates", PPoPP '13, for discussion of
                                      // why using a shared variable scales fairly well in this situation.
                                      if (__comp(__i - __first, __extremum))
                                      {
                                          _Index __res = __f(__i, __j);
                                          // If not '__last' returned then we found what we want so put this to extremum
                                          if (__res != __j)
                                          {
                                              const _DifferenceType __k = __res - __first;
                                              for (_DifferenceType __old = __extremum; __comp(__k, __old);
                                                   __old = __extremum)
                                              {
                                                  __extremum.compare_exchange_weak(__old, __k);
                                              }
                                          }
                                      }
                                  });
    return __extremum != __initial_dist ? __first + __extremum : __last;
}

//------------------------------------------------------------------------
// parallel_or
//------------------------------------------------------------------------
//! Return true if brick f[i,j) returns true for some subrange [i,j) of [first,last)
template <class _ExecutionPolicy, class _Index, class _Brick>
bool
__parallel_or(_ExecutionPolicy&& __exec, _Index __first, _Index __last, _Brick __f)
{
    ::std::atomic<bool> __found(false);
    __par_backend::__parallel_for(::std::forward<_ExecutionPolicy>(__exec), __first, __last,
                                  [__f, &__found](_Index __i, _Index __j)
                                  {
                                      if (!__found.load(::std::memory_order_relaxed) && __f(__i, __j))
                                      {
                                          __found.store(true, ::std::memory_order_relaxed);
                                          __par_backend::__cancel_execution();
                                      }
                                  });
    return __found;
}

// TODO: Here are improvements to consider:
// 1. Affinitize leaves of upsweep and leaves of downsweep.  For working sets that
//    fit in cache, this might reduce memory interconnect load significantly.
// 2. Add automatic tilesize adjustment.  Initial stealing during upsweep ought to provide a good hint.
// 3. Use continuation-passing style for the tasks.  For downsweep, a binomial tree pattern is likely optimal.

template <typename _Index>
_Index
__split(_Index __m)
{
    _Index __k = 1;
    while (2 * __k < __m)
        __k *= 2;
    return __k;
}

template <typename _Index, typename _Tp, typename _Rp, typename _Cp>
void
__upsweep(_Index __i, _Index __m, _Index __tilesize, _Tp* __r, _Index __lastsize, _Rp __reduce, _Cp __combine)
{
    if (__m == 1)
        __r[0] = __reduce(__i * __tilesize, __lastsize);
    else
    {
        _Index __k = __split(__m);
        __par_backend::__parallel_invoke_body(
            [=] { __internal::__upsweep(__i, __k, __tilesize, __r, __tilesize, __reduce, __combine); }, [=]
            { __internal::__upsweep(__i + __k, __m - __k, __tilesize, __r + __k, __lastsize, __reduce, __combine); });
        if (__m == 2 * __k)
            __r[__m - 1] = __combine(__r[__k - 1], __r[__m - 1]);
    }
}

template <typename _Index, typename _Tp, typename _Cp, typename _Sp>
void
__downsweep(_Index __i, _Index __m, _Index __tilesize, _Tp* __r, _Index __lastsize, _Tp __initial, _Cp __combine,
            _Sp __scan)
{
    if (__m == 1)
        __scan(__i * __tilesize, __lastsize, __initial);
    else
    {
        const _Index __k = __split(__m);
        __par_backend::__parallel_invoke_body(
            [=] { __internal::__downsweep(__i, __k, __tilesize, __r, __tilesize, __initial, __combine, __scan); },
            // Assumes that __combine never throws.
            // TODO: Consider adding a requirement for user functors to be constant.
            [=, &__combine]
            {
                __internal::__downsweep(__i + __k, __m - __k, __tilesize, __r + __k, __lastsize,
                                        __combine(__initial, __r[__k - 1]), __combine, __scan);
            });
    }
}

} // namespace __internal
} // namespace dpl
} // namespace oneapi

#endif /* _ONEDPL_PARALLEL_IMPL_H */
