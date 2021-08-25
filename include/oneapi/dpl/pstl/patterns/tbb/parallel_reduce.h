namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

//! Evaluation of brick f[i,j) for each subrange [i,j) of [first,last)
// wrapper over tbb::parallel_reduce
template <class _ExecutionPolicy, class _Value, class _Index, typename _RealBody, typename _Reduction>
_Value
__parallel_reduce(_ExecutionPolicy&&, _Index __first, _Index __last, const _Value& __identity,
                  const _RealBody& __real_body, const _Reduction& __reduction)
{
    return tbb::this_task_arena::isolate(
        [__first, __last, &__identity, &__real_body, &__reduction]() -> _Value
        {
            return tbb::parallel_reduce(
                tbb::blocked_range<_Index>(__first, __last), __identity,
                [__real_body](const tbb::blocked_range<_Index>& __r, const _Value& __value) -> _Value
                { return __real_body(__r.begin(), __r.end(), __value); },
                __reduction);
        });
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi