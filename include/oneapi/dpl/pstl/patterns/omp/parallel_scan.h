namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

template <typename _ExecutionPolicy, typename _Index, typename _Tp, typename _Rp, typename _Cp, typename _Sp,
          typename _Ap>
void
__parallel_strict_scan_body(_Index __n, _Tp __initial, _Rp __reduce, _Cp __combine, _Sp __scan, _Ap __apex)
{
    _Index __p = omp_get_num_threads();
    const _Index __slack = 4;
    _Index __tilesize = (__n - 1) / (__slack * __p) + 1;
    _Index __m = (__n - 1) / __tilesize;
    oneapi::dpl::__utils::__buffer<_ExecutionPolicy, _Tp> __buf(__m + 1);
    _Tp* __r = __buf.get();

    __internal::__upsweep(_Index(0), _Index(__m + 1), __tilesize, __r, __n - __m * __tilesize, __reduce, __combine);

    std::size_t __k = __m + 1;
    _Tp __t = __r[__k - 1];
    while ((__k &= __k - 1))
    {
        __t = __combine(__r[__k - 1], __t);
    }

    __apex(__combine(__initial, __t));
    __internal::__downsweep(_Index(0), _Index(__m + 1), __tilesize, __r, __n - __m * __tilesize, __initial, __combine,
                            __scan);
}

template <class _ExecutionPolicy, typename _Index, typename _Tp, typename _Rp, typename _Cp, typename _Sp, typename _Ap>
void
__parallel_strict_scan(_ExecutionPolicy&&, _Index __n, _Tp __initial, _Rp __reduce, _Cp __combine, _Sp __scan,
                       _Ap __apex)
{
    if (__n <= __default_chunk_size)
    {
        _Tp __sum = __initial;
        if (__n)
        {
            __sum = __combine(__sum, __reduce(_Index(0), __n));
        }
        __apex(__sum);
        if (__n)
        {
            __scan(_Index(0), __n, __initial);
        }
        return;
    }

    if (omp_in_parallel())
    {
        dpl::__par_backend::__parallel_strict_scan_body<_ExecutionPolicy>(__n, __initial, __reduce, __combine, __scan,
                                                                          __apex);
    }
    else
    {
        _PSTL_PRAGMA(omp parallel)
        _PSTL_PRAGMA(omp single)
        {
            dpl::__par_backend::__parallel_strict_scan_body<_ExecutionPolicy>(__n, __initial, __reduce, __combine,
                                                                              __scan, __apex);
        }
    }
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi
