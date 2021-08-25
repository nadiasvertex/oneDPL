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

#ifndef _ONEDPL_PARALLEL_BACKEND_TBB_H
#define _ONEDPL_PARALLEL_BACKEND_TBB_H

#include <cassert>
#include <algorithm>
#include <type_traits>

#include "parallel_backend_utils.h"

// Bring in minimal required subset of Intel(R) Threading Building Blocks (Intel(R) TBB)
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_scan.h>
#include <tbb/parallel_invoke.h>
#include <tbb/task_arena.h>
#include <tbb/tbb_allocator.h>
#if TBB_INTERFACE_VERSION > 12000
#    include <tbb/task.h>
#endif

#if TBB_INTERFACE_VERSION < 10000
#    error Intel(R) Threading Building Blocks 2018 is required; older versions are not supported.
#endif

namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

namespace __tbb_backend = __par_backend;

// Local type for __buffer uses tbb allocator
template <typename _ExecutionPolicy, typename _Tp>
using __buffer = oneapi::dpl::__utils::__buffer<_ExecutionPolicy, _Tp, tbb::tbb_allocator<_Tp>>;

// Wrapper for tbb::task
inline void
__cancel_execution()
{
#if TBB_INTERFACE_VERSION <= 12000
    tbb::task::self().group()->cancel_group_execution();
#else
    tbb::task::current_context()->cancel_group_execution();
#endif
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi

//------------------------------------------------------------------------
// parallel_invoke
//------------------------------------------------------------------------

#include "./patterns/tbb/parallel_invoke.h"

//------------------------------------------------------------------------
// parallel_for
//------------------------------------------------------------------------

#include "./patterns/tbb/parallel_for.h"

//------------------------------------------------------------------------
// parallel_for_each
//------------------------------------------------------------------------

#include "./patterns/tbb/parallel_for_each.h"

//------------------------------------------------------------------------
// parallel_reduce
//------------------------------------------------------------------------

#include "./patterns/tbb/parallel_reduce.h"
#include "./patterns/tbb/parallel_transform_reduce.h"

//------------------------------------------------------------------------
// parallel_scan
//------------------------------------------------------------------------

#include "parallel_impl.h"
#include "./patterns/tbb/parallel_scan.h"
#include "./patterns/tbb/parallel_transform_scan.h"

//------------------------------------------------------------------------
// parallel_stable_sort
//------------------------------------------------------------------------

#include "./patterns/tbb/parallel_stable_sort.h"

//------------------------------------------------------------------------
// parallel_merge
//------------------------------------------------------------------------
#include "./patterns/tbb/parallel_merge.h"

#endif /* _ONEDPL_PARALLEL_BACKEND_TBB_H */
