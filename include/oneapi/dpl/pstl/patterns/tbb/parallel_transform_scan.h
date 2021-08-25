namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

template <class _Index, class _Up, class _Tp, class _Cp, class _Rp, class _Sp>
class __trans_scan_body
{
    alignas(_Tp) char _M_sum_storage[sizeof(_Tp)]; // Holds generalized non-commutative sum when has_sum==true
    _Rp _M_brick_reduce;                           // Most likely to have non-empty layout
    _Up _M_u;
    _Cp _M_combine;
    _Sp _M_scan;
    bool _M_has_sum; // Put last to minimize size of class
  public:
    __trans_scan_body(_Up __u, _Tp __init, _Cp __combine, _Rp __reduce, _Sp __scan)
        : _M_brick_reduce(__reduce), _M_u(__u), _M_combine(__combine), _M_scan(__scan), _M_has_sum(true)
    {
        new (_M_sum_storage) _Tp(__init);
    }

    __trans_scan_body(__trans_scan_body& __b, tbb::split)
        : _M_brick_reduce(__b._M_brick_reduce), _M_u(__b._M_u), _M_combine(__b._M_combine), _M_scan(__b._M_scan),
          _M_has_sum(false)
    {
    }

    ~__trans_scan_body()
    {
        // 17.6.5.12 tells us to not worry about catching exceptions from destructors.
        if (_M_has_sum)
            sum().~_Tp();
    }

    _Tp&
    sum() const
    {
        __TBB_ASSERT(_M_has_sum, "sum expected");
        return *const_cast<_Tp*>(reinterpret_cast<_Tp const*>(_M_sum_storage));
    }

    void
    operator()(const tbb::blocked_range<_Index>& __range, tbb::pre_scan_tag)
    {
        _Index __i = __range.begin();
        _Index __j = __range.end();
        if (!_M_has_sum)
        {
            new (&_M_sum_storage) _Tp(_M_u(__i));
            _M_has_sum = true;
            ++__i;
            if (__i == __j)
                return;
        }
        sum() = _M_brick_reduce(__i, __j, sum());
    }

    void
    operator()(const tbb::blocked_range<_Index>& __range, tbb::final_scan_tag)
    {
        sum() = _M_scan(__range.begin(), __range.end(), sum());
    }

    void
    reverse_join(__trans_scan_body& __a)
    {
        if (_M_has_sum)
        {
            sum() = _M_combine(__a.sum(), sum());
        }
        else
        {
            new (&_M_sum_storage) _Tp(__a.sum());
            _M_has_sum = true;
        }
    }

    void
    assign(__trans_scan_body& __b)
    {
        sum() = __b.sum();
    }
};

template <class _ExecutionPolicy, class _Index, class _Up, class _Tp, class _Cp, class _Rp, class _Sp>
_Tp
__parallel_transform_scan(_ExecutionPolicy&&, _Index __n, _Up __u, _Tp __init, _Cp __combine, _Rp __brick_reduce,
                          _Sp __scan)
{
    __trans_scan_body<_Index, _Up, _Tp, _Cp, _Rp, _Sp> __body(__u, __init, __combine, __brick_reduce, __scan);
    auto __range = tbb::blocked_range<_Index>(0, __n);
    tbb::this_task_arena::isolate([__range, &__body]() { tbb::parallel_scan(__range, __body); });
    return __body.sum();
}
} // namespace __par_backend
} // namespace dpl
} // namespace oneapi