namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename _RandomAccessIterator3,
          typename _Compare, typename _LeafMerge>
class __merge_func_static
{
    _RandomAccessIterator1 _M_xs, _M_xe;
    _RandomAccessIterator2 _M_ys, _M_ye;
    _RandomAccessIterator3 _M_zs;
    _Compare _M_comp;
    _LeafMerge _M_leaf_merge;

  public:
    __merge_func_static(_RandomAccessIterator1 __xs, _RandomAccessIterator1 __xe, _RandomAccessIterator2 __ys,
                        _RandomAccessIterator2 __ye, _RandomAccessIterator3 __zs, _Compare __comp,
                        _LeafMerge __leaf_merge)
        : _M_xs(__xs), _M_xe(__xe), _M_ys(__ys), _M_ye(__ye), _M_zs(__zs), _M_comp(__comp), _M_leaf_merge(__leaf_merge)
    {
    }

    __task*
    operator()(__task* __self);
};

//TODO: consider usage of parallel_for with a custom blocked_range
template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename _RandomAccessIterator3,
          typename __M_Compare, typename _LeafMerge>
__task*
__merge_func_static<_RandomAccessIterator1, _RandomAccessIterator2, _RandomAccessIterator3, __M_Compare,
                    _LeafMerge>::operator()(__task* __self)
{
    typedef typename ::std::iterator_traits<_RandomAccessIterator1>::difference_type _DifferenceType1;
    typedef typename ::std::iterator_traits<_RandomAccessIterator2>::difference_type _DifferenceType2;
    typedef typename ::std::common_type<_DifferenceType1, _DifferenceType2>::type _SizeType;
    const _SizeType __n = (_M_xe - _M_xs) + (_M_ye - _M_ys);
    const _SizeType __merge_cut_off = _ONEDPL_MERGE_CUT_OFF;
    if (__n <= __merge_cut_off)
    {
        _M_leaf_merge(_M_xs, _M_xe, _M_ys, _M_ye, _M_zs, _M_comp);
        return nullptr;
    }

    _RandomAccessIterator1 __xm;
    _RandomAccessIterator2 __ym;
    if (_M_xe - _M_xs < _M_ye - _M_ys)
    {
        __ym = _M_ys + (_M_ye - _M_ys) / 2;
        __xm = ::std::upper_bound(_M_xs, _M_xe, *__ym, _M_comp);
    }
    else
    {
        __xm = _M_xs + (_M_xe - _M_xs) / 2;
        __ym = ::std::lower_bound(_M_ys, _M_ye, *__xm, _M_comp);
    }
    const _RandomAccessIterator3 __zm = _M_zs + ((__xm - _M_xs) + (__ym - _M_ys));
    auto __right = __self->make_additional_child_of(
        __self->parent(), __merge_func_static(__xm, _M_xe, __ym, _M_ye, __zm, _M_comp, _M_leaf_merge));
    __self->spawn(__right);
    __self->recycle_as_continuation();
    _M_xe = __xm;
    _M_ye = __ym;

    return __self;
}

template <class _ExecutionPolicy, typename _RandomAccessIterator1, typename _RandomAccessIterator2,
          typename _RandomAccessIterator3, typename _Compare, typename _LeafMerge>
void
__parallel_merge(_ExecutionPolicy&&, _RandomAccessIterator1 __xs, _RandomAccessIterator1 __xe,
                 _RandomAccessIterator2 __ys, _RandomAccessIterator2 __ye, _RandomAccessIterator3 __zs, _Compare __comp,
                 _LeafMerge __leaf_merge)
{
    typedef typename ::std::iterator_traits<_RandomAccessIterator1>::difference_type _DifferenceType1;
    typedef typename ::std::iterator_traits<_RandomAccessIterator2>::difference_type _DifferenceType2;
    typedef typename ::std::common_type<_DifferenceType1, _DifferenceType2>::type _SizeType;
    const _SizeType __n = (__xe - __xs) + (__ye - __ys);
    const _SizeType __merge_cut_off = _ONEDPL_MERGE_CUT_OFF;
    if (__n <= __merge_cut_off)
    {
        // Fall back on serial merge
        __leaf_merge(__xs, __xe, __ys, __ye, __zs, __comp);
    }
    else
    {
        tbb::this_task_arena::isolate(
            [=]()
            {
                typedef __merge_func_static<_RandomAccessIterator1, _RandomAccessIterator2, _RandomAccessIterator3,
                                            _Compare, _LeafMerge>
                    _TaskType;
                __root_task<_TaskType> __root{__xs, __xe, __ys, __ye, __zs, __comp, __leaf_merge};
                __task::spawn_root_and_wait(__root);
            });
    }
}

} // namespace __par_backend
} // namespace dpl
} // namespace oneapi