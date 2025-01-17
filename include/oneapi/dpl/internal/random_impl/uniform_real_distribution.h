// -*- C++ -*-
//===-- uniform_real_distribution.h ---------------------------------------===//
//
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This file incorporates work covered by the following copyright and permission
// notice:
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//
//
// Abstract:
//
// Public header file provides implementation for Uniform Real Distribution

#ifndef _ONEDPL_UNIFORM_REAL_DISTRIBUTION
#define _ONEDPL_UNIFORM_REAL_DISTRIBUTION

namespace oneapi
{
namespace dpl
{

template <class _RealType = double>
class uniform_real_distribution
{
  public:
    // Distribution types
    using result_type = _RealType;
    using scalar_type = internal::element_type_t<_RealType>;
    using param_type = typename ::std::pair<scalar_type, scalar_type>;

    // Constructors
    uniform_real_distribution() : uniform_real_distribution(static_cast<scalar_type>(0.0)) {}
    explicit uniform_real_distribution(scalar_type __a, scalar_type __b = static_cast<scalar_type>(1.0))
        : a_(__a), b_(__b)
    {
    }
    explicit uniform_real_distribution(const param_type& __params) : a_(__params.first), b_(__params.second) {}

    // Reset function
    void
    reset()
    {
    }

    // Property functions
    scalar_type
    a() const
    {
        return a_;
    }

    scalar_type
    b() const
    {
        return b_;
    }

    param_type
    param() const
    {
        return param_type(a_, b_);
    }

    void
    param(const param_type& __parm)
    {
        a_ = __parm.first;
        b_ = __parm.second;
    }

    scalar_type
    min() const
    {
        return a();
    }

    scalar_type
    max() const
    {
        return b();
    }

    // Generate functions
    template <class _Engine>
    result_type
    operator()(_Engine& __engine)
    {
        return operator()<_Engine>(__engine, param_type(a_, b_));
    }

    template <class _Engine>
    result_type
    operator()(_Engine& __engine, const param_type& __params)
    {
        return generate<size_of_type_, internal::type_traits_t<typename _Engine::result_type>::num_elems, _Engine>(
            __engine, __params);
    }

    template <class _Engine>
    result_type
    operator()(_Engine& __engine, unsigned int __random_nums)
    {
        return operator()<_Engine>(__engine, param_type(a_, b_), __random_nums);
    }

    template <class _Engine>
    result_type
    operator()(_Engine& __engine, const param_type& __params, unsigned int __random_nums)
    {
        return result_portion_internal<size_of_type_, internal::type_traits_t<typename _Engine::result_type>::num_elems,
                                       _Engine>(__engine, __params, __random_nums);
    }

  private:
    // Size of type
    static constexpr int size_of_type_ = internal::type_traits_t<result_type>::num_elems;

    // Static asserts
    static_assert(::std::is_floating_point<scalar_type>::value,
                  "oneapi::dpl::uniform_real_distribution. Error: unsupported data type");

    // Distribution parameters
    scalar_type a_;
    scalar_type b_;

    // Implementation for generate function
    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr == _Nengine) & (_Ndistr != 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        auto __engine_output = __engine();
        result_type __res;

        for (int __i = 0; __i < _Ndistr; ++__i)
            __res[__i] = static_cast<scalar_type>(__engine_output[__i]);

        __res = ((__res - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr == _Nengine) & (_Ndistr == 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        auto __engine_output = __engine();
        auto __res = static_cast<scalar_type>(__engine_output);
        __res = ((__res - static_cast<scalar_type>(__engine.min())) /
                 (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr < _Nengine) & (_Ndistr != 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        auto __engine_output = __engine(_Ndistr);
        result_type __res;
        for (int __i = 0; __i < _Ndistr; ++__i)
        {
            __res[__i] = static_cast<scalar_type>(__engine_output[__i]);
            __res[__i] =
                ((__res[__i] - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        }
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr < _Nengine) & (_Ndistr == 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        scalar_type __res = static_cast<scalar_type>(__engine(1)[0]);
        __res = ((__res - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr > _Nengine) & (_Nengine != 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        sycl::vec<scalar_type, _Ndistr> __res;
        int __i;
        constexpr int __tail_size = _Ndistr % _Nengine;
        for (__i = 0; __i < _Ndistr - __tail_size; __i += _Nengine)
        {
            auto __engine_output = __engine();
            auto __res_tmp = __engine_output.template convert<scalar_type, sycl::rounding_mode::rte>();
            __res_tmp =
                ((__res_tmp - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;

            for (int __j = 0; __j < _Nengine; ++__j)
                __res[__i + __j] = __res_tmp[__j];
        }

        if (__tail_size)
        {
            __i = _Ndistr - __tail_size;
            auto __engine_output = __engine(__tail_size);
            auto __res_tmp = __engine_output.template convert<scalar_type, sycl::rounding_mode::rte>();
            __res_tmp =
                ((__res_tmp - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
            for (int __j = 0; __j < __tail_size; __j++)
                __res[__i + __j] = __res_tmp[__j];
        }
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr > _Nengine) & (_Nengine == 0)), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        sycl::vec<scalar_type, _Ndistr> __res;
        for (int __i = 0; __i < _Ndistr; ++__i)
        {
            __res[__i] = static_cast<scalar_type>(__engine());
            __res[__i] =
                ((__res[__i] - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        }
        return __res;
    }

    // Implementation for result_portion function
    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr <= _Nengine) & (_Ndistr != 0)), result_type>::type
    generate_n_elems(_Engine& __engine, const param_type& __params, unsigned int __N)
    {
        auto __engine_output = __engine(__N);
        result_type __res;
        for (int __i = 0; __i < __N; ++__i)
        {
            __res[__i] = static_cast<scalar_type>(__engine_output[__i]);
            __res[__i] =
                ((__res[__i] - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        }
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr > _Nengine) & (_Nengine != 0)), result_type>::type
    generate_n_elems(_Engine& __engine, const param_type& __params, unsigned int __N)
    {
        result_type __res;
        int __i;

        if (_Nengine >= __N)
        {
            auto __engine_output = __engine(__N);
            for (__i = 0; __i < __N; ++__i)
            {
                __res[__i] = static_cast<scalar_type>(__engine_output[__i]);
                __res[__i] =
                    ((__res[__i] - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                        (__params.second - __params.first) +
                    __params.first;
            }
        }
        else
        {
            unsigned int __tail_size = __N % _Nengine;
            for (__i = 0; __i < __N; __i += _Nengine)
            {
                auto __engine_output = __engine();
                auto __res_tmp = __engine_output.template convert<scalar_type, sycl::rounding_mode::rte>();
                __res_tmp =
                    ((__res_tmp - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                        (__params.second - __params.first) +
                    __params.first;
                for (int __j = 0; __j < _Nengine; ++__j)
                {
                    __res[__i + __j] = __res_tmp[__j];
                }
            }
            if (__tail_size)
            {
                __i = _Ndistr - __tail_size;
                auto __engine_output = __engine(__tail_size);
                auto __res_tmp = __engine_output.template convert<scalar_type, sycl::rounding_mode::rte>();
                __res_tmp =
                    ((__res_tmp - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                        (__params.second - __params.first) +
                    __params.first;

                for (unsigned int __j = 0; __j < __tail_size; ++__j)
                {
                    __res[__i + __j] = __res_tmp[__j];
                }
            }
        }
        return __res;
    }

    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<((_Ndistr > _Nengine) & (_Nengine == 0)), result_type>::type
    generate_n_elems(_Engine& __engine, const param_type& __params, unsigned int __N)
    {
        result_type __res;
        for (int __i = 0; __i < __N; ++__i)
        {
            __res[__i] = static_cast<scalar_type>(__engine());
            __res[__i] =
                ((__res[__i] - __engine.min()) / (1 + static_cast<scalar_type>(__engine.max() - __engine.min()))) *
                    (__params.second - __params.first) +
                __params.first;
        }
        return __res;
    }

    // Implementation for result_portion function
    template <int _Ndistr, int _Nengine, class _Engine>
    typename ::std::enable_if<(_Ndistr != 0), result_type>::type
    result_portion_internal(_Engine& __engine, const param_type __params, unsigned int __N)
    {
        result_type __part_vec;
        if (__N == 0)
            return __part_vec;
        else if (__N >= _Ndistr)
            return operator()(__engine, __params);

        __part_vec = generate_n_elems<_Ndistr, _Nengine, _Engine>(__engine, __params, __N);
        return __part_vec;
    }
};

} // namespace dpl
} // namespace oneapi

#endif // #ifndf _ONEDPL_UNIFORM_REAL_DISTRIBUTION