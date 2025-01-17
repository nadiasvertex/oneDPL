namespace oneapi
{
namespace dpl
{
namespace __omp_backend
{

template <class _Index, class _Fp>
void
__parallel_for_body(_Index __first, _Index __last, _Fp __f)
{
    // initial partition of the iteration space into chunks
    auto __policy = __omp_backend::__chunk_partitioner(__first, __last);

    // To avoid over-subscription we use taskloop for the nested parallelism
    _PSTL_PRAGMA(omp taskloop untied mergeable)
    for (std::size_t __chunk = 0; __chunk < __policy.__n_chunks; ++__chunk)
    {
        __omp_backend::__process_chunk(__policy, __first, __chunk, __f);
    }
}

//------------------------------------------------------------------------
// Notation:
// Evaluation of brick f[i,j) for each subrange [i,j) of [first, last)
//------------------------------------------------------------------------

template <class _ExecutionPolicy, class _Index, class _Fp>
void
__parallel_for(_ExecutionPolicy&&, _Index __first, _Index __last, _Fp __f)
{
    if (__should_run_serial(__first, __last))
    {
        __f(__first, __last);
        return;
    }

    if (omp_in_parallel())
    {
        // we don't create a nested parallel region in an existing parallel
        // region: just create tasks
        dpl::__omp_backend::__parallel_for_body(__first, __last, __f);
    }
    else
    {
        // in any case (nested or non-nested) one parallel region is created and
        // only one thread creates a set of tasks
        _PSTL_PRAGMA(omp parallel)
        _PSTL_PRAGMA(omp single) { dpl::__omp_backend::__parallel_for_body(__first, __last, __f); }
    }
}

} // namespace __omp_backend
} // namespace dpl
} // namespace oneapi
