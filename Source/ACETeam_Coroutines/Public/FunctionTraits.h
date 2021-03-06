// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "Templates/Tuple.h"

namespace detail
{
	template<typename R, typename... Args>
	struct TFunctionTraitsBase
	{
	  using RetType = R;
	  using ArgTypes = TTuple<Args...>;
	  static constexpr std::size_t ArgCount = sizeof...(Args);
	  template<std::size_t N>
	  using NthArg = TTupleElement<N, ArgTypes>;
	};
}

template<typename F> struct TFunctionTraits;

template<typename R, typename... Args>
struct TFunctionTraits<R(*)(Args...)>
    : detail::TFunctionTraitsBase<R, Args...>
{
  using Pointer = R(*)(Args...);
};

template<typename R, typename C, typename... Args>
struct TFunctionTraits<R(C::*)(Args...) const>
    : detail::TFunctionTraitsBase<R, Args...>
{
  using Pointer = R(*)(Args...);
};

template<typename R, typename C, typename... Args>
struct TFunctionTraits<R(C::*)(Args...)>
    : detail::TFunctionTraitsBase<R, Args...>
{
  using Pointer = R(*)(Args...);
};