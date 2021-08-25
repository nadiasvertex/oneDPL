namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

template <typename _F1, typename _F2>
void
__parallel_invoke_body(_F1&& __f1, _F2&& __f2)
{
    //TODO: a version of tbb::this_task_arena::isolate with variadic arguments pack should be added in the future
    tbb::this_task_arena::isolate([&]()
                                  { tbb::parallel_invoke(::std::forward<_F1>(__f1), ::std::forward<_F2>(__f2)); });
}

template <class _ExecutionPolicy, typename _F1, typename _F2>
void
__parallel_invoke(_ExecutionPolicy&&, _F1&& __f1, _F2&& __f2)
{
    __parallel_invoke_body(std::forward<_F1>(__f1), std::forward<_F2>(__f2));
}
} // namespace __par_backend
} // namespace dpl
} // namespace oneapi