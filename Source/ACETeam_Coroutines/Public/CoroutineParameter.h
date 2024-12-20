// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"
#include "FunctionTraits.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
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
			else
			{
				return Value;
			}
		}
	}
}