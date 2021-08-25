namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

// Adapted from Intel(R) Cilk(TM) version from cilkpub.
// Let i:len denote a counted interval of length n starting at i.  s denotes a generalized-sum value.
// Expected actions of the functors are:
//     reduce(i,len) -> s  -- return reduction value of i:len.
//     combine(s1,s2) -> s -- return merged sum
//     apex(s) -- do any processing necessary between reduce and scan.
//     scan(i,len,initial) -- perform scan over i:len starting with initial.
// The initial range 0:n is partitioned into consecutive subranges.
// reduce and scan are each called exactly once per subrange.
// Thus callers can rely upon side effects in reduce.
// combine must not throw an exception.
// apex is called exactly once, after all calls to reduce and before all calls to scan.
// For example, it's useful for allocating a __buffer used by scan but whose size is the sum of all reduction values.
// T must have a trivial constructor and destructor.
template <class _ExecutionPolicy, typename _Index, typename _Tp, typename _Rp, typename _Cp, typename _Sp, typename _Ap>
void
__parallel_strict_scan(_ExecutionPolicy&&, _Index __n, _Tp __initial, _Rp __reduce, _Cp __combine, _Sp __scan,
                       _Ap __apex)
{
    tbb::this_task_arena::isolate(
        [=, &__combine]()
        {
            if (__n > 1)
            {
                _Index __p = tbb::this_task_arena::max_concurrency();
                const _Index __slack = 4;
                _Index __tilesize = (__n - 1) / (__slack * __p) + 1;
                _Index __m = (__n - 1) / __tilesize;
                __buffer<_ExecutionPolicy, _Tp> __buf(__m + 1);
                _Tp* __r = __buf.get();
                __internal::__upsweep(_Index(0), _Index(__m + 1), __tilesize, __r, __n - __m * __tilesize, __reduce,
                                      __combine);

                // When __apex is a no-op and __combine has no side effects, a good optimizer
                // should be able to eliminate all code between here and __apex.
                // Alternatively, provide a default value for __apex that can be
                // recognized by metaprogramming that conditionlly executes the following.
                size_t __k = __m + 1;
                _Tp __t = __r[__k - 1];
                while ((__k &= __k - 1))
                    __t = __combine(__r[__k - 1], __t);
                __apex(__combine(__initial, __t));
                __internal::__downsweep(_Index(0), _Index(__m + 1), __tilesize, __r, __n - __m * __tilesize, __initial,
                                        __combine, __scan);
                return;
            }
            // Fewer than 2 elements in sequence, or out of memory.  Handle has single block.
            _Tp __sum = __initial;
            if (__n)
                __sum = __combine(__sum, __reduce(_Index(0), __n));
            __apex(__sum);
            if (__n)
                __scan(_Index(0), __n, __initial);
        });
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi