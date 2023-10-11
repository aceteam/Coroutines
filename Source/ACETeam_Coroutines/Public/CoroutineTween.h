// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"

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
		
		template <typename T, typename TEaseFunc>
		class TObjectPropertyTweenNode : public FCoroutineNode
		{
			FWeakObjectPtr OwnerObj;
			T& Property;
			T TargetValue;
			float Speed;

			T StartValue;
			float CurAlpha;
			TEaseFunc EaseFunc;

			virtual EStatus Start(FCoroutineExecutor* Exec) override
			{
				if (OwnerObj.IsValid())
				{
					StartValue = Property;
					CurAlpha = 0.0f;
					return Running;
				}
				return Failed;
			}
			virtual EStatus Update(FCoroutineExecutor* Exec, float dt) override
			{
				if (!OwnerObj.IsValid())
					return Failed;
				CurAlpha = FMath::Min(1.0f, CurAlpha + Speed * dt);
				float t = EaseFunc(CurAlpha);
				Property = FMath::Lerp(StartValue, TargetValue, t);
				return CurAlpha < 1.0f ? Running : Completed;
			}
#if WITH_GAMEPLAY_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Tween"); }
#endif

		public:
			TObjectPropertyTweenNode(UObject* _Obj, T& _Property, T _TargetVal, float _Speed, TEaseFunc const& _EaseFunc)
				: OwnerObj(_Obj)
				, Property(_Property)
				, TargetValue(_TargetVal)
				, Speed(_Speed)
				, EaseFunc(_EaseFunc)
				{}
		};
	}

	template <typename T>
	FCoroutineNodeRef _Lerp(UObject* Obj, T& Property, T TargetVal, float Speed)
	{
		return MakeShared<Detail::TObjectPropertyTweenNode<T, Detail::FEaseLinear>, DefaultSPMode>(Obj, Property, TargetVal, Speed, Detail::FEaseLinear());
	}

	template <typename T>
	FCoroutineNodeRef _Tween_EaseInOutCubic(UObject* Obj, T& Property, T TargetVal, float Speed)
	{
		return MakeShared<Detail::TObjectPropertyTweenNode<T, Detail::FEaseInOutCubic>, DefaultSPMode>(Obj, Property, TargetVal, Speed, Detail::FEaseInOutCubic());
	}

	template <typename T>
	FCoroutineNodeRef _Tween_EaseInOutElastic(UObject* Obj, T& Property, T TargetVal, float Speed)
	{
		return MakeShared<Detail::TObjectPropertyTweenNode<T, Detail::FEaseInOutElastic>, DefaultSPMode>(Obj, Property, TargetVal, Speed, Detail::FEaseInOutElastic());
	}

	namespace Detail
	{
		template<typename T>
		class THasRequiredTweenCallOp																		
		{																											
			template <typename U, float(U::*)(float) const> struct Check;								
			template <typename U> static char MemberTest(Check<U, &U::operator()> *);									
			template <typename U> static int MemberTest(...);															
		public:																											
			enum { Value = sizeof(MemberTest<T>(nullptr)) == sizeof(char) };											
		};
	}

	template <typename T, typename TEaseFunc>
	FCoroutineNodeRef _Tween_CustomEase(UObject* Obj, T& Property, T TargetVal, float Speed, TEaseFunc const& EaseFunc)
	{
		static_assert(Detail::THasRequiredTweenCallOp<TEaseFunc>::Value, "EaseFunc needs to have a float operator()(float) const function");
		return MakeShared<Detail::TObjectPropertyTweenNode<T, TEaseFunc>, DefaultSPMode>(Obj, Property, TargetVal, Speed, EaseFunc);
	}
}