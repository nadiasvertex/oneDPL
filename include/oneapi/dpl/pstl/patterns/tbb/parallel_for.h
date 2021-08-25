namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

template <class _Index, class _RealBody>
class __parallel_for_body
{
  public:
    __parallel_for_body(const _RealBody& __body) : _M_body(__body) {}
    __parallel_for_body(const __parallel_for_body& __body) : _M_body(__body._M_body) {}
    void
    operator()(const tbb::blocked_range<_Index>& __range) const
    {
        _M_body(__range.begin(), __range.end());
    }

  private:
    _RealBody _M_body;
};

//! Evaluation of brick f[i,j) for each subrange [i,j) of [first,last)
// wrapper over tbb::parallel_for
template <class _ExecutionPolicy, class _Index, class _Fp>
void
__parallel_for(_ExecutionPolicy&&, _Index __first, _Index __last, _Fp __f)
{
    tbb::this_task_arena::isolate(
        [=]()
        { tbb::parallel_for(tbb::blocked_range<_Index>(__first, __last), __parallel_for_body<_Index, _Fp>(__f)); });
}
} // namespace __par_backend
} // namespace dpl
} // namespace oneapi