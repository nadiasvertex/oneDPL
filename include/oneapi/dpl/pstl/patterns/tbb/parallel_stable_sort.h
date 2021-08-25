namespace oneapi
{
namespace dpl
{
namespace __par_backend
{

//------------------------------------------------------------------------
// stable_sort utilities
//
// These are used by parallel implementations but do not depend on them.
//------------------------------------------------------------------------
#define _ONEDPL_MERGE_CUT_OFF 2000

template <typename _Func>
class __func_task;
template <typename _Func>
class __root_task;

#if TBB_INTERFACE_VERSION <= 12000
class __task : public tbb::task
{
  public:
    template <typename _Fn>
    __task*
    make_continuation(_Fn&& __f)
    {
        return new (allocate_continuation()) __func_task<typename ::std::decay<_Fn>::type>(::std::forward<_Fn>(__f));
    }

    template <typename _Fn>
    __task*
    make_child_of(__task* parent, _Fn&& __f)
    {
        return new (parent->allocate_child()) __func_task<typename ::std::decay<_Fn>::type>(::std::forward<_Fn>(__f));
    }

    template <typename _Fn>
    __task*
    make_additional_child_of(tbb::task* parent, _Fn&& __f)
    {
        return new (tbb::task::allocate_additional_child_of(*parent))
            __func_task<typename ::std::decay<_Fn>::type>(::std::forward<_Fn>(__f));
    }

    inline void
    recycle_as_continuation()
    {
        tbb::task::recycle_as_continuation();
    }

    inline void
    recycle_as_child_of(__task* parent)
    {
        tbb::task::recycle_as_child_of(*parent);
    }

    inline void
    spawn(__task* __t)
    {
        tbb::task::spawn(*__t);
    }

    template <typename _Fn>
    static inline void
    spawn_root_and_wait(__root_task<_Fn>& __root)
    {
        tbb::task::spawn_root_and_wait(*__root._M_task);
    }
};

template <typename _Func>
class __func_task : public __task
{
    _Func _M_func;

    tbb::task*
    execute()
    {
        return _M_func(this);
    };

  public:
    template <typename _Fn>
    __func_task(_Fn&& __f) : _M_func{::std::forward<_Fn>(__f)}
    {
    }

    _Func&
    body()
    {
        return _M_func;
    }
};

template <typename _Func>
class __root_task
{
    tbb::task* _M_task;

  public:
    template <typename... Args>
    __root_task(Args&&... args)
        : _M_task{new (tbb::task::allocate_root()) __func_task<_Func>{_Func(::std::forward<Args>(args)...)}}
    {
    }

    friend class __task;
    friend class __func_task<_Func>;
};

#else  // TBB_INTERFACE_VERSION <= 12000
class __task : public tbb::detail::d1::task
{
  protected:
    tbb::detail::d1::small_object_allocator _M_allocator{};
    tbb::detail::d1::execution_data* _M_execute_data{};
    __task* _M_parent{};
    ::std::atomic<int> _M_refcount{};
    bool _M_recycle{};

    template <typename _Fn>
    __task*
    allocate_func_task(_Fn&& __f)
    {
        assert(_M_execute_data != nullptr);
        tbb::detail::d1::small_object_allocator __alloc{};
        auto __t = __alloc.new_object<__func_task<typename ::std::decay<_Fn>::type>>(*_M_execute_data,
                                                                                     ::std::forward<_Fn>(__f));
        __t->_M_allocator = __alloc;
        return __t;
    }

  public:
    __task*
    parent()
    {
        return _M_parent;
    }

    void
    set_ref_count(int __n)
    {
        _M_refcount.store(__n, ::std::memory_order_release);
    }

    template <typename _Fn>
    __task*
    make_continuation(_Fn&& __f)
    {
        auto __t = allocate_func_task(::std::forward<_Fn&&>(__f));
        __t->_M_parent = _M_parent;
        _M_parent = nullptr;
        return __t;
    }

    template <typename _Fn>
    __task*
    make_child_of(__task* __parent, _Fn&& __f)
    {
        auto __t = allocate_func_task(::std::forward<_Fn&&>(__f));
        __t->_M_parent = __parent;
        return __t;
    }

    template <typename _Fn>
    __task*
    make_additional_child_of(__task* __parent, _Fn&& __f)
    {
        auto __t = make_child_of(__parent, ::std::forward<_Fn>(__f));
        assert(__parent->_M_refcount.load(::std::memory_order_relaxed) > 0);
        ++__parent->_M_refcount;
        return __t;
    }

    inline void
    recycle_as_continuation()
    {
        _M_recycle = true;
    }

    inline void
    recycle_as_child_of(__task* parent)
    {
        _M_recycle = true;
        _M_parent = parent;
    }

    inline void
    spawn(__task* __t)
    {
        assert(_M_execute_data != nullptr);
        tbb::detail::d1::spawn(*__t, *_M_execute_data->context);
    }

    template <typename _Fn>
    static inline void
    spawn_root_and_wait(__root_task<_Fn>& __root)
    {
        tbb::detail::d1::execute_and_wait(*__root._M_func_task, __root._M_context, __root._M_wait_object,
                                          __root._M_context);
    }

    template <typename _Func>
    friend class __func_task;
};

template <typename _Func>
class __func_task : public __task
{
    _Func _M_func;

    __task*
    execute(tbb::detail::d1::execution_data& __ed) override
    {
        _M_execute_data = &__ed;
        _M_recycle = false;
        __task* __next = _M_func(this);
        return finalize(__next);
    };

    __task*
    cancel(tbb::detail::d1::execution_data&) override
    {
        return finalize(nullptr);
    }

    __task*
    finalize(__task* __next)
    {
        bool __recycle = _M_recycle;
        _M_recycle = false;

        if (__recycle)
        {
            return __next;
        }

        auto __parent = _M_parent;
        auto __alloc = _M_allocator;
        auto __ed = _M_execute_data;

        this->~__func_task();

        assert(__parent != nullptr);
        assert(__parent->_M_refcount.load(::std::memory_order_relaxed) > 0);
        if (--__parent->_M_refcount == 0)
        {
            assert(__next == nullptr);
            __alloc.deallocate(this, *__ed);
            return __parent;
        }

        return __next;
    }

    friend class __root_task<_Func>;

  public:
    template <typename _Fn>
    __func_task(_Fn&& __f) : _M_func(::std::forward<_Fn>(__f))
    {
    }

    _Func&
    body()
    {
        return _M_func;
    }
};

template <typename _Func>
class __root_task : public __task
{
    __task*
    execute(tbb::detail::d1::execution_data&) override
    {
        _M_wait_object.release();
        return nullptr;
    };

    __task*
    cancel(tbb::detail::d1::execution_data&) override
    {
        _M_wait_object.release();
        return nullptr;
    }

    __func_task<_Func>* _M_func_task{};
    tbb::detail::d1::wait_context _M_wait_object{0};
    tbb::task_group_context _M_context{};

  public:
    template <typename... Args>
    __root_task(Args&&... args) : _M_wait_object{1}
    {
        tbb::detail::d1::small_object_allocator __alloc{};
        _M_func_task = __alloc.new_object<__func_task<_Func>>(_Func(::std::forward<Args>(args)...));
        _M_func_task->_M_allocator = __alloc;
        _M_func_task->_M_parent = this;
        _M_refcount.store(1, ::std::memory_order_relaxed);
    }

    friend class __task;
};
#endif // TBB_INTERFACE_VERSION <= 12000

template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename _Compare, typename _Cleanup,
          typename _LeafMerge>
class __merge_func
{
    typedef typename ::std::iterator_traits<_RandomAccessIterator1>::difference_type _DifferenceType1;
    typedef typename ::std::iterator_traits<_RandomAccessIterator2>::difference_type _DifferenceType2;
    typedef typename ::std::common_type<_DifferenceType1, _DifferenceType2>::type _SizeType;
    typedef typename ::std::iterator_traits<_RandomAccessIterator1>::value_type _ValueType;

    _RandomAccessIterator1 _M_x_beg;
    _RandomAccessIterator2 _M_z_beg;

    _SizeType _M_xs, _M_xe;
    _SizeType _M_ys, _M_ye;
    _SizeType _M_zs;
    _Compare _M_comp;
    _LeafMerge _M_leaf_merge;
    _SizeType _M_nsort; //number of elements to be sorted for partial_sort alforithm

    static const _SizeType __merge_cut_off = _ONEDPL_MERGE_CUT_OFF;

    bool _root;   //means a task is merging root task
    bool _x_orig; //"true" means X(or left ) subrange is in the original container; false - in the buffer
    bool _y_orig; //"true" means Y(or right) subrange is in the original container; false - in the buffer
    bool _split;  //"true" means a merge task is a split task for parallel merging, the execution logic differs

    struct __move_value
    {
        template <typename Iterator1, typename Iterator2>
        void
        operator()(Iterator1 __x, Iterator2 __z)
        {
            *__z = ::std::move(*__x);
        }
    };

    struct __move_value_construct
    {
        template <typename Iterator1, typename Iterator2>
        void
        operator()(Iterator1 __x, Iterator2 __z)
        {
            ::new (::std::addressof(*__z)) _ValueType(::std::move(*__x));
        }
    };

    struct __move_range
    {
        template <typename Iterator1, typename Iterator2>
        Iterator2
        operator()(Iterator1 __first1, Iterator1 __last1, Iterator2 __first2)
        {
            if (__last1 - __first1 < __merge_cut_off)
                return ::std::move(__first1, __last1, __first2);

            auto __n = __last1 - __first1;
            tbb::parallel_for(
                tbb::blocked_range<_SizeType>(0, __n, __merge_cut_off),
                [__first1, __first2](const tbb::blocked_range<_SizeType>& __range)
                { ::std::move(__first1 + __range.begin(), __first1 + __range.end(), __first2 + __range.begin()); });
            return __first2 + __n;
        }
    };

    struct __move_range_construct
    {
        template <typename Iterator1, typename Iterator2>
        Iterator2
        operator()(Iterator1 __first1, Iterator1 __last1, Iterator2 __first2)
        {
            if (__last1 - __first1 < __merge_cut_off)
            {
                for (; __first1 != __last1; ++__first1, ++__first2)
                    __move_value_construct()(__first1, __first2);
                return __first2;
            }

            auto __n = __last1 - __first1;
            tbb::parallel_for(tbb::blocked_range<_SizeType>(0, __n, __merge_cut_off),
                              [__first1, __first2](const tbb::blocked_range<_SizeType>& __range)
                              {
                                  for (auto i = __range.begin(); i != __range.end(); ++i)
                                      __move_value_construct()(__first1 + i, __first2 + i);
                              });
            return __first2 + __n;
        }
    };

    struct __cleanup_range
    {
        template <typename Iterator>
        void
        operator()(Iterator __first, Iterator __last)
        {
            if (__last - __first < __merge_cut_off)
                _Cleanup()(__first, __last);
            else
            {
                auto __n = __last - __first;
                tbb::parallel_for(tbb::blocked_range<_SizeType>(0, __n, __merge_cut_off),
                                  [__first](const tbb::blocked_range<_SizeType>& __range)
                                  { _Cleanup()(__first + __range.begin(), __first + __range.end()); });
            }
        }
    };

  public:
    __merge_func(_SizeType __xs, _SizeType __xe, _SizeType __ys, _SizeType __ye, _SizeType __zs, _Compare __comp,
                 _Cleanup, _LeafMerge __leaf_merge, _SizeType __nsort, _RandomAccessIterator1 __x_beg,
                 _RandomAccessIterator2 __z_beg, bool __x_orig, bool __y_orig, bool __root)
        : _M_x_beg(__x_beg), _M_z_beg(__z_beg), _M_xs(__xs), _M_xe(__xe), _M_ys(__ys), _M_ye(__ye), _M_zs(__zs),
          _M_comp(__comp), _M_leaf_merge(__leaf_merge), _M_nsort(__nsort), _root(__root), _x_orig(__x_orig),
          _y_orig(__y_orig), _split(false)
    {
    }

    bool
    is_left(_SizeType __idx) const
    {
        return _M_xs == __idx;
    }

    template <typename IndexType>
    void
    set_odd(IndexType __idx, bool __on_off)
    {
        if (is_left(__idx))
            _x_orig = __on_off;
        else
            _y_orig = __on_off;
    }

    __task*
    operator()(__task* __self);

  private:
    __merge_func*
    parent_merge(__task* __self) const
    {
        return _root ? nullptr : &static_cast<__func_task<__merge_func>*>(__self->parent())->body();
    }
    bool
    x_less_y()
    {
        auto __nx = (_M_xe - _M_xs);
        auto __ny = (_M_ye - _M_ys);

        assert(__nx > 0 && __ny > 0);
        assert(_M_nsort > 0);

        auto __kx = ::std::min(_M_nsort, __nx);
        auto __ky = ::std::min(_M_nsort, __ny);

        assert(_x_orig == _y_orig);

        if (_x_orig)
        {
            assert(::std::is_sorted(_M_x_beg + _M_xs, _M_x_beg + _M_xs + __kx, _M_comp));
            assert(::std::is_sorted(_M_x_beg + _M_ys, _M_x_beg + _M_ys + __ky, _M_comp));
            return !_M_comp(*(_M_x_beg + _M_ys), *(_M_x_beg + _M_xs + __kx - 1));
        }

        assert(::std::is_sorted(_M_z_beg + _M_xs, _M_z_beg + _M_xs + __kx, _M_comp));
        assert(::std::is_sorted(_M_z_beg + _M_ys, _M_z_beg + _M_ys + __ky, _M_comp));
        return !_M_comp(*(_M_z_beg + _M_zs + __nx), *(_M_z_beg + _M_zs + __kx - 1));
    }
    void
    move_x_range()
    {
        const auto __nx = (_M_xe - _M_xs);
        const auto __ny = (_M_ye - _M_ys);
        assert(__nx > 0 && __ny > 0);

        if (_x_orig)
            __move_range_construct()(_M_x_beg + _M_xs, _M_x_beg + _M_xe, _M_z_beg + _M_zs);
        else
        {
            __move_range()(_M_z_beg + _M_zs, _M_z_beg + _M_zs + __nx, _M_x_beg + _M_xs);
            __cleanup_range()(_M_z_beg + _M_zs, _M_z_beg + _M_zs + __nx);
        }

        _x_orig = !_x_orig;
    }
    void
    move_y_range()
    {
        const auto __nx = (_M_xe - _M_xs);
        const auto __ny = (_M_ye - _M_ys);

        if (_y_orig)
            __move_range_construct()(_M_x_beg + _M_ys, _M_x_beg + _M_ye, _M_z_beg + _M_zs + __nx);
        else
        {
            __move_range()(_M_z_beg + _M_zs + __nx, _M_z_beg + _M_zs + __nx + __ny, _M_x_beg + _M_ys);
            __cleanup_range()(_M_z_beg + _M_zs + __nx, _M_z_beg + _M_zs + __nx + __ny);
        }

        _y_orig = !_y_orig;
    }
    __task*
    merge_ranges(__task* __self)
    {
        assert(_M_nsort > 0);
        assert(_x_orig == _y_orig); //two merged subrange must be lie into the same buffer

        const auto __nx = (_M_xe - _M_xs);
        const auto __ny = (_M_ye - _M_ys);
        const auto __n = __nx + __ny;

        // need to merge {x} and {y}
        if (__n > __merge_cut_off)
            return split_merging(__self);

        //merge to buffer
        if (_x_orig)
        {
            _M_leaf_merge(_M_x_beg + _M_xs, _M_x_beg + _M_xe, _M_x_beg + _M_ys, _M_x_beg + _M_ye, _M_z_beg + _M_zs,
                          _M_comp, __move_value_construct(), __move_value_construct(), __move_range_construct(),
                          __move_range_construct());
            assert(parent_merge(__self)); //not root merging task
        }
        //merge to "origin"
        else
        {
            assert(_x_orig == _y_orig);

            _M_leaf_merge(_M_z_beg + _M_xs, _M_z_beg + _M_xe, _M_z_beg + _M_ys, _M_z_beg + _M_ye, _M_x_beg + _M_zs,
                          _M_comp, __move_value(), __move_value(), __move_range(), __move_range());

            __cleanup_range()(_M_z_beg + _M_xs, _M_z_beg + _M_xe);
            __cleanup_range()(_M_z_beg + _M_ys, _M_z_beg + _M_ye);
        }
        return nullptr;
    }

    __task*
    process_ranges(__task* __self)
    {
        assert(_x_orig == _y_orig);
        assert(!_split);

        auto p = parent_merge(__self);

        if (!p)
        { //root merging task

            //optimization, just for sort algorithm, //{x} <= {y}
            if (x_less_y()) //we have a solution
            {
                if (!_x_orig)
                {                   //we have to move the solution to the origin
                    move_x_range(); //parallel moving
                    move_y_range(); //parallel moving
                }
                return nullptr;
            }
            //else: if we have data in the origin,
            //we have to move data to the buffer for final merging into the origin.
            if (_x_orig)
            {
                move_x_range(); //parallel moving
                move_y_range(); //parallel moving
            }
            // need to merge {x} and {y}.
            return merge_ranges(__self);
        }
        //else: not root merging task (parent_merge() == NULL)
        //optimization, just for sort algorithm, //{x} <= {y}
        if (x_less_y())
        {
            const auto id_range = _M_zs;
            p->set_odd(id_range, _x_orig);
            return nullptr;
        }
        //else: we have to revert "_x(y)_orig" flag of the parent merging task
        const auto id_range = _M_zs;
        p->set_odd(id_range, !_x_orig);

        return merge_ranges(__self);
    }

    //splitting as merge task into 2 of the same level
    __task*
    split_merging(__task* __self)
    {
        assert(_x_orig == _y_orig);

        const auto __nx = (_M_xe - _M_xs);
        const auto __ny = (_M_ye - _M_ys);
        _SizeType __xm{};
        _SizeType __ym{};

        if (__nx < __ny)
        {
            __ym = _M_ys + __ny / 2;

            if (_x_orig)
                __xm = ::std::upper_bound(_M_x_beg + _M_xs, _M_x_beg + _M_xs + __nx, *(_M_x_beg + __ym), _M_comp) -
                       _M_x_beg;
            else
                __xm = ::std::upper_bound(_M_z_beg + _M_xs, _M_z_beg + _M_xs + __nx, *(_M_z_beg + __ym), _M_comp) -
                       _M_z_beg;
        }
        else
        {
            __xm = _M_xs + __nx / 2;

            if (_y_orig)
                __ym = ::std::lower_bound(_M_x_beg + _M_ys, _M_x_beg + _M_ys + __ny, *(_M_x_beg + __xm), _M_comp) -
                       _M_x_beg;
            else
                __ym = ::std::lower_bound(_M_z_beg + _M_ys, _M_z_beg + _M_ys + __ny, *(_M_z_beg + __xm), _M_comp) -
                       _M_z_beg;
        }

        auto __zm = _M_zs + ((__xm - _M_xs) + (__ym - _M_ys));
        __merge_func __right_func(__xm, _M_xe, __ym, _M_ye, __zm, _M_comp, _Cleanup(), _M_leaf_merge, _M_nsort,
                                  _M_x_beg, _M_z_beg, _x_orig, _y_orig, _root);
        __right_func._split = true;
        auto __merge_task = __self->make_additional_child_of(__self->parent(), ::std::move(__right_func));
        __self->spawn(__merge_task);
        __self->recycle_as_continuation();

        _M_xe = __xm;
        _M_ye = __ym;
        _split = true;

        return __self;
    }
};

template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename __M_Compare, typename _Cleanup,
          typename _LeafMerge>
__task*
__merge_func<_RandomAccessIterator1, _RandomAccessIterator2, __M_Compare, _Cleanup, _LeafMerge>::operator()(
    __task* __self)
{
    //a. split merge task into 2 of the same level; the special logic,
    //without processing(process_ranges) adjacent sub-ranges x and y
    if (_split)
        return merge_ranges(__self);

    //b. General merging of adjacent sub-ranges x and y (with optimization in case of {x} <= {y} )

    //1. x and y are in the even buffer
    //2. x and y are in the odd buffer
    if (_x_orig == _y_orig)
        return process_ranges(__self);

    //3. x is in even buffer, y is in the odd buffer
    //4. x is in odd buffer, y is in the even buffer
    if (!parent_merge(__self))
    { //root merge task
        if (_x_orig)
            move_x_range();
        else
            move_y_range();
    }
    else
    {
        const _SizeType __nx = (_M_xe - _M_xs);
        const _SizeType __ny = (_M_ye - _M_ys);
        assert(__nx > 0);
        assert(__nx > 0);

        if (__nx < __ny)
            move_x_range();
        else
            move_y_range();
    }

    return process_ranges(__self);
}

template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename _Compare, typename _LeafSort>
class __stable_sort_func
{
  public:
    typedef typename ::std::iterator_traits<_RandomAccessIterator1>::difference_type _DifferenceType1;
    typedef typename ::std::iterator_traits<_RandomAccessIterator2>::difference_type _DifferenceType2;
    typedef typename ::std::common_type<_DifferenceType1, _DifferenceType2>::type _SizeType;

  private:
    _RandomAccessIterator1 _M_xs, _M_xe, _M_x_beg;
    _RandomAccessIterator2 _M_zs, _M_z_beg;
    _Compare _M_comp;
    _LeafSort _M_leaf_sort;
    bool _M_root;
    _SizeType _M_nsort; //zero or number of elements to be sorted for partial_sort alforithm

  public:
    __stable_sort_func(_RandomAccessIterator1 __xs, _RandomAccessIterator1 __xe, _RandomAccessIterator2 __zs,
                       bool __root, _Compare __comp, _LeafSort __leaf_sort, _SizeType __nsort,
                       _RandomAccessIterator1 __x_beg, _RandomAccessIterator2 __z_beg)
        : _M_xs(__xs), _M_xe(__xe), _M_x_beg(__x_beg), _M_zs(__zs), _M_z_beg(__z_beg), _M_comp(__comp),
          _M_leaf_sort(__leaf_sort), _M_root(__root), _M_nsort(__nsort)
    {
    }

    __task*
    operator()(__task* __self);
};

#define _ONEDPL_STABLE_SORT_CUT_OFF 500

template <typename _RandomAccessIterator1, typename _RandomAccessIterator2, typename _Compare, typename _LeafSort>
__task*
__stable_sort_func<_RandomAccessIterator1, _RandomAccessIterator2, _Compare, _LeafSort>::operator()(__task* __self)
{
    typedef __merge_func<_RandomAccessIterator1, _RandomAccessIterator2, _Compare, __utils::__serial_destroy,
                         __utils::__serial_move_merge>
        _MergeTaskType;

    assert(_M_nsort > 0);

    const _SizeType __n = _M_xe - _M_xs;
    const _SizeType __nmerge = ::std::min(_M_nsort, __n);
    const _SizeType __sort_cut_off = _ONEDPL_STABLE_SORT_CUT_OFF;
    if (__n <= __sort_cut_off)
    {
        _M_leaf_sort(_M_xs, _M_xe, _M_comp);
        assert(!_M_root);
        return nullptr;
    }

    const _RandomAccessIterator1 __xm = _M_xs + __n / 2;
    const _RandomAccessIterator2 __zm = _M_zs + (__xm - _M_xs);
    _MergeTaskType __m(_MergeTaskType(_M_xs - _M_x_beg, __xm - _M_x_beg, __xm - _M_x_beg, _M_xe - _M_x_beg,
                                      _M_zs - _M_z_beg, _M_comp, __utils::__serial_destroy(),
                                      __utils::__serial_move_merge(__nmerge), _M_nsort, _M_x_beg, _M_z_beg,
                                      /*x_orig*/ true, /*y_orig*/ true, /*root*/ _M_root));
    auto __parent = __self->make_continuation(::std::move(__m));
    __parent->set_ref_count(2);
    auto __right = __self->make_child_of(
        __parent, __stable_sort_func(__xm, _M_xe, __zm, false, _M_comp, _M_leaf_sort, _M_nsort, _M_x_beg, _M_z_beg));
    __self->spawn(__right);
    __self->recycle_as_child_of(__parent);
    _M_root = false;
    _M_xe = __xm;

    return __self;
}

template <class _ExecutionPolicy, typename _RandomAccessIterator, typename _Compare, typename _LeafSort>
void
__parallel_stable_sort(_ExecutionPolicy&&, _RandomAccessIterator __xs, _RandomAccessIterator __xe, _Compare __comp,
                       _LeafSort __leaf_sort, ::std::size_t __nsort)
{
    tbb::this_task_arena::isolate(
        [=, &__nsort]()
        {
            //sorting based on task tree and parallel merge
            typedef typename ::std::iterator_traits<_RandomAccessIterator>::value_type _ValueType;
            typedef typename ::std::iterator_traits<_RandomAccessIterator>::difference_type _DifferenceType;
            const _DifferenceType __n = __xe - __xs;

            const _DifferenceType __sort_cut_off = _ONEDPL_STABLE_SORT_CUT_OFF;
            if (__n > __sort_cut_off)
            {
                __buffer<_ExecutionPolicy, _ValueType> __buf(__n);
                __root_task<__stable_sort_func<_RandomAccessIterator, _ValueType*, _Compare, _LeafSort>> __root{
                    __xs, __xe, __buf.get(), true, __comp, __leaf_sort, __nsort, __xs, __buf.get()};
                __task::spawn_root_and_wait(__root);
                return;
            }
            //serial sort
            __leaf_sort(__xs, __xe, __comp);
        });
}
} // namespace __par_backend
} // namespace dpl
} // namespace oneapi