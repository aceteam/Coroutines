// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"
#include "FunctionTraits.h"

/**
 * Helper templates for templated coroutine nodes that want to be able to support parameters that can either be static,
 * read from a shared CoroVar, or evaluated from a stored functor
 */
namespace ACETeam_Coroutines
{
	namespace Detail
	{
		//Compile time check to see if a template parameter is a valid "provider" type of the desired data type
		template <typename T, typename U>
		constexpr bool TIsCoroutineParam_V = []
		{
			if constexpr (std::is_same_v<T, U>) {
				return true;
			} else if constexpr (std::is_same_v<TCoroVar<T>, U>) {
				return true;
			} else if constexpr (TIsFunctor_V<U>) {
				return std::is_same_v<T, typename TFunctorTraits<U>::RetType>;
			}
			return false;
		}();

		//Adapts the "provider" type into a functor that returns a value of the desired data type
		//Passthrough for types that are already valid functors
		template <typename T, typename U>
		auto ParameterHelper(U const& Value)
		{
			if constexpr (std::is_same_v<T, U>)
			{
				return [Value] { return Value;};
			}
			else if constexpr (std::is_same_v<TCoroVar<T>, U>)
			{
				return [Value] { return *Value;};
			}
			else if constexpr (TIsFunctor_V<U> && std::is_same_v<T, typename TFunctorTraits<U>::RetType>)
			{
				return Value;
			}
		}
	}
}