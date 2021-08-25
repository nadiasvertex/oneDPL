namespace oneapi
{
namespace dpl
{
namespace __par_backend
{
template <class _ExecutionPolicy, class _ForwardIterator, class _Fp>
void
__parallel_for_each(_ExecutionPolicy&&, _ForwardIterator __begin, _ForwardIterator __end, _Fp __f)
{
    tbb::this_task_arena::isolate([&]() { tbb::parallel_for_each(__begin, __end, __f); });
}
} // namespace __par_backend
} // namespace dpl
} // namespace oneapi