// -*- C++ -*-
//===-- extreme_value_distribution.h ------------------------------------------===//
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
// Public header file provides implementation for Extreme Value Distribution

#ifndef _ONEDPL_EXTREME_VALUE_DISTRIBUTION
#define _ONEDPL_EXTREME_VALUE_DISTRIBUTION

namespace oneapi
{
namespace dpl
{
template <class _RealType = double>
class extreme_value_distribution
{
  public:
    // Distribution types
    using result_type = _RealType;
    using scalar_type = internal::element_type_t<_RealType>;

    struct param_type
    {
        param_type() : param_type(scalar_type{0.0}) {}
        param_type(scalar_type __a, scalar_type __b = scalar_type{1.0}) : a(__a), b(__b) {}
        scalar_type a;
        scalar_type b;
    };

    // Constructors
    extreme_value_distribution() : extreme_value_distribution(scalar_type{0.0}) {}
    explicit extreme_value_distribution(scalar_type __a, scalar_type __b = scalar_type{1.0}) : a_(__a), b_(__b) {}
    explicit extreme_value_distribution(const param_type& __params) : a_(__params.a), b_(__params.b) {}

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
    param(const param_type& __param)
    {
        a_ = __param.a;
        b_ = __param.b;
    }

    scalar_type
    min() const
    {
        return std::numeric_limits<scalar_type>::lowest();
    }

    scalar_type
    max() const
    {
        return std::numeric_limits<scalar_type>::max();
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
        return generate<size_of_type_, _Engine>(__engine, __params);
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
        return result_portion_internal<size_of_type_, _Engine>(__engine, __params, __random_nums);
    }

  private:
    // Size of type
    static constexpr int size_of_type_ = internal::type_traits_t<result_type>::num_elems;

    // Static asserts
    static_assert(::std::is_floating_point<scalar_type>::value,
                  "oneapi::dpl::extreme_value_distribution. Error: unsupported data type");

    // Distribution parameters
    scalar_type a_;
    scalar_type b_;

    // Callback function
    template <typename _Type = float>
    inline scalar_type
    callback()
    {
        return ((scalar_type*)(internal::gaussian_sp_table))[1];
    }

    template <>
    inline scalar_type
    callback<double>()
    {
        return ((scalar_type*)(internal::gaussian_dp_table))[1];
    }

    // Implementation for generate function
    template <int _Ndistr, class _Engine>
    typename ::std::enable_if<(_Ndistr != 0), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        return generate_vec<_Ndistr, _Engine>(__engine, __params);
    }

    // Specialization of the scalar generation
    template <int _Ndistr, class _Engine>
    typename ::std::enable_if<(_Ndistr == 0), result_type>::type
    generate(_Engine& __engine, const param_type& __params)
    {
        oneapi::dpl::exponential_distribution<scalar_type> __distr;
        scalar_type __e = __distr(__engine);
        result_type __res = (__e == scalar_type{0.0}) ? callback<scalar_type>() : sycl::log(__e);
        return __params.a - __params.b * __res;
    }

    // Specialization of the vector generation with size = [1; 2; 3]
    template <int __N, class _Engine>
    typename ::std::enable_if<(__N <= 3), result_type>::type
    generate_vec(_Engine& __engine, const param_type& __params)
    {
        return generate_n_elems<_Engine>(__engine, __params, __N);
    }

    // Specialization of the vector generation with size = [4; 8; 16]
    template <int __N, class _Engine>
    typename ::std::enable_if<(__N > 3), result_type>::type
    generate_vec(_Engine& __engine, const param_type& __params)
    {
        oneapi::dpl::exponential_distribution<result_type> __distr;
        result_type __e = __distr(__engine);
        result_type __res =
            select(sycl::log(__e), result_type{callback<scalar_type>()}, sycl::isequal(__e, result_type{0.0}));
        return __params.a - __params.b * __res;
    }

    // Implementation for the N vector's elements generation
    template <class _Engine>
    result_type
    generate_n_elems(_Engine& __engine, const param_type& __params, unsigned int __N)
    {
        result_type __res;
        oneapi::dpl::exponential_distribution<scalar_type> __distr;
        scalar_type __e = __distr(__engine);
        for (int i = 0; i < __N; i++)
        {
            scalar_type __e = __distr(__engine);
            __res[i] = (__e == scalar_type{0.0}) ? callback<scalar_type>() : sycl::log(__e);
            __res[i] = __params.a - __params.b * __res[i];
        }

        return __res;
    }

    // Implementation for result_portion function
    template <int _Ndistr, class _Engine>
    typename ::std::enable_if<(_Ndistr != 0), result_type>::type
    result_portion_internal(_Engine& __engine, const param_type& __params, unsigned int __N)
    {
        result_type __part_vec;
        if (__N == 0)
            return __part_vec;
        else if (__N >= _Ndistr)
            return operator()(__engine, __params);

        __part_vec = generate_n_elems(__engine, __params, __N);
        return __part_vec;
    }
};
} // namespace dpl
} // namespace oneapi

#endif // #ifndf _ONEDPL_EXTREME_VALUE_DISTRIBUTION