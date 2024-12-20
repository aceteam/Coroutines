// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineParameter.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		struct FEaseInOutCubic
		{
			float operator() (float x) const
			{
				if (x < 0.5f)
				{
					x = 4.0f*x*x*x;
				}
				else
				{
					const float s = (-2.0f * x + 2.0f);
					x = 1.0f-(s*s*s)/2.0f;
				}
				return x;
			}
		};
		
		struct FEaseLinear
		{
			float operator () (float x) const { return x; }
		};
		
		struct FEaseInOutElastic
		{
			float operator()(float x) const
			{
				const float c5 = (2.0f * PI) / 4.5f;

				return x < 0.5f
				  ? -(FMath::Pow(2.0f, 20.0f * x - 10.0f) * FMath::Sin((20.0f * x - 11.125f) * c5)) / 2.0f
				  : (FMath::Pow(2.0f, -20.0f * x + 10.0f) * FMath::Sin((20.0f * x - 11.125f) * c5)) / 2.0f + 1.0f;
			}
		};
		
		template <typename T, typename TTargetParam, typename TSpeedParam, typename TEaseFunc>
		class TObjectPropertyTweenNode : public FCoroutineNode
		{
			FWeakObjectPtr OwnerObj;
			T& Property;
			TTargetParam TargetValue;
			TSpeedParam Speed;

			T StartValue;
			float CurAlpha;
			float CurSpeed;
			TEaseFunc EaseFunc;

			virtual EStatus Start(FCoroutineExecutor* Exec) override
			{
				if (OwnerObj.IsValid())
				{
					StartValue = Property;
					CurAlpha = 0.0f;
					CurSpeed = Speed();
					return Running;
				}
				return Failed;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float dt) override
			{
				if (!OwnerObj.IsValid())
					return Failed;
				CurAlpha = FMath::Min(1.0f, CurAlpha + CurSpeed * dt);
				float t = EaseFunc(CurAlpha);
				Property = FMath::Lerp(StartValue, TargetValue(), t);
				return CurAlpha < 1.0f ? Running : Completed;
			}
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Tween"); }
#endif

		public:
			TObjectPropertyTweenNode(UObject* _Obj, T& _Property, TTargetParam const& _TargetVal, TSpeedParam const& _Speed, TEaseFunc const& _EaseFunc)
				: OwnerObj(_Obj)
				, Property(_Property)
				, TargetValue(_TargetVal)
				, Speed(_Speed)
				, EaseFunc(_EaseFunc)
				{}
		};
		
		template<typename T>
		constexpr bool THasRequiredTweenCallOp_V = []
		{
			if constexpr (TIsFunctor_V<T>)
			{
				return TFunctorTraits<T>::ArgCount == 1
				&& std::is_same_v<float, typename TFunctorTraits<T>::RetType>
				&& std::is_same_v<float, typename TFunctorTraits<T>::template NthArg<0>>;
			}
			else
			{
				return false;
			}
		}();
	}

	template <typename T, typename TTargetParam, typename TSpeedParam, typename TEaseFunc>
	FCoroutineNodeRef _Tween_CustomEase(UObject* Obj, T& Property, TTargetParam const& Target, TSpeedParam const& Speed, TEaseFunc const& EaseFunc)
	{
		static_assert(Detail::THasRequiredTweenCallOp_V<TEaseFunc>, "EaseFunc needs to have a float operator()(float) const function");
		static_assert(Detail::TIsCoroutineParam_V<T, TTargetParam>, "Target needs to either be a constant, TCoroVar, or a lambda that returns that type");
		static_assert(Detail::TIsCoroutineParam_V<float, TSpeedParam>, "Speed needs to either be a float constant, TCoroVar<float>, or a lambda that returns float");
		auto TargetProvider = Detail::ParameterHelper<T, TTargetParam>(Target);
		auto SpeedProvider = Detail::ParameterHelper<float, TSpeedParam>(Speed);
		return MakeShared<Detail::TObjectPropertyTweenNode<T, decltype(TargetProvider), decltype(SpeedProvider), TEaseFunc>, DefaultSPMode>(Obj, Property, TargetProvider, SpeedProvider, EaseFunc);
	}

	template <typename T, typename TTargetParam, typename TSpeedParam>
	FCoroutineNodeRef _Lerp(UObject* Obj, T& Property, TTargetParam const& TargetVal, TSpeedParam const& Speed)
	{
		return _Tween_CustomEase(Obj, Property, TargetVal, Speed, Detail::FEaseLinear());
	}

	template <typename T, typename TTargetParam, typename TSpeedParam>
	FCoroutineNodeRef _Tween_EaseInOutCubic(UObject* Obj, T& Property, TTargetParam const& TargetVal, TSpeedParam const& Speed)
	{
		return _Tween_CustomEase(Obj, Property, TargetVal, Speed, Detail::FEaseInOutCubic());
	}

	template <typename T, typename TTargetParam, typename TSpeedParam>
	FCoroutineNodeRef _Tween_EaseInOutElastic(UObject* Obj, T& Property, TTargetParam const& TargetVal, TSpeedParam const& Speed)
	{
		return _Tween_CustomEase(Obj, Property, TargetVal, Speed, Detail::FEaseInOutCubic());
	}
}