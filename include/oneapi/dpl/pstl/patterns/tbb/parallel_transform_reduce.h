namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

//------------------------------------------------------------------------
// parallel_transform_reduce
//
// Notation:
//      r(i,j,init) returns reduction of init with reduction over [i,j)
//      u(i) returns f(i,i+1,identity) for a hypothetical left identity element of r
//      c(x,y) combines values x and y that were the result of r or u
//------------------------------------------------------------------------

template <class _Index, class _Up, class _Tp, class _Cp, class _Rp>
struct __par_trans_red_body
{
    alignas(_Tp) char _M_sum_storage[sizeof(_Tp)]; // Holds generalized non-commutative sum when has_sum==true
    _Rp _M_brick_reduce;                           // Most likely to have non-empty layout
    _Up _M_u;
    _Cp _M_combine;
    bool _M_has_sum; // Put last to minimize size of class
    _Tp&
    sum()
    {
        __TBB_ASSERT(_M_has_sum, "sum expected");
        return *(_Tp*)_M_sum_storage;
    }
    __par_trans_red_body(_Up __u, _Tp __init, _Cp __c, _Rp __r)
        : _M_brick_reduce(__r), _M_u(__u), _M_combine(__c), _M_has_sum(true)
    {
        new (_M_sum_storage) _Tp(__init);
    }

    __par_trans_red_body(__par_trans_red_body& __left, tbb::split)
        : _M_brick_reduce(__left._M_brick_reduce), _M_u(__left._M_u), _M_combine(__left._M_combine), _M_has_sum(false)
    {
    }

    ~__par_trans_red_body()
    {
        // 17.6.5.12 tells us to not worry about catching exceptions from destructors.
        if (_M_has_sum)
            sum().~_Tp();
    }

    void
    join(__par_trans_red_body& __rhs)
    {
        sum() = _M_combine(sum(), __rhs.sum());
    }

    void
    operator()(const tbb::blocked_range<_Index>& __range)
    {
        _Index __i = __range.begin();
        _Index __j = __range.end();
        if (!_M_has_sum)
        {
            __TBB_ASSERT(__range.size() > 1, "there should be at least 2 elements");
            new (&_M_sum_storage)
                _Tp(_M_combine(_M_u(__i), _M_u(__i + 1))); // The condition i+1 < j is provided by the grain size of 3
            _M_has_sum = true;
            ::std::advance(__i, 2);
            if (__i == __j)
                return;
        }
        sum() = _M_brick_reduce(__i, __j, sum());
    }
};

template <class _ExecutionPolicy, class _Index, class _Up, class _Tp, class _Cp, class _Rp>
_Tp
__parallel_transform_reduce(_ExecutionPolicy&&, _Index __first, _Index __last, _Up __u, _Tp __init, _Cp __combine,
                            _Rp __brick_reduce)
{
    __tbb_backend::__par_trans_red_body<_Index, _Up, _Tp, _Cp, _Rp> __body(__u, __init, __combine, __brick_reduce);
    // The grain size of 3 is used in order to provide minimum 2 elements for each body
    tbb::this_task_arena::isolate([__first, __last, &__body]()
                                  { tbb::parallel_reduce(tbb::blocked_range<_Index>(__first, __last, 3), __body); });
    return __body.sum();
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi