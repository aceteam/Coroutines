#pragma once

#include "CoroutineElements.h"
#include "CoroutineNode.h"
#include "FunctionTraits.h"
#include "Engine/StreamableManager.h"

struct FStreamableHandle;

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		struct ACETEAM_COROUTINES_API FAssetStreamingNode : FCoroutineNode, TSharedFromThis<FAssetStreamingNode, DefaultSPMode>
		{
			TFunction<TArray<FSoftObjectPath> ()> SoftObjectPathGetter;
			FCoroutineExecutor* CachedExec = nullptr;
			TSharedPtr<FStreamableHandle> Handle;
			TAsyncLoadPriority AsyncLoadPriority;

			FAssetStreamingNode(TFunction<TArray<FSoftObjectPath> ()> const& InGetter, TAsyncLoadPriority InAsyncLoadPriority);

			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
		};
		
		template <typename T>
		constexpr bool TIsSoftObjectPtrTArray_V = false;

		template <typename InObjectType, typename InAllocatorType>
		constexpr bool TIsSoftObjectPtrTArray_V<TArray<TSoftObjectPtr<InObjectType>, InAllocatorType>> = true;

		template <typename T>
		constexpr bool TIsSoftClassPtrTArray_V = false;
		
		template <typename InObjectType, typename InAllocatorType>
		constexpr bool TIsSoftClassPtrTArray_V<TArray<TSoftClassPtr<InObjectType>, InAllocatorType>> = true;
	}

	ACETEAM_COROUTINES_API FCoroutineNodeRef _StreamAssets(TArray<FSoftObjectPath> const& SoftObjectPaths, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority);
	ACETEAM_COROUTINES_API FCoroutineNodeRef _StreamAssets(std::initializer_list<FSoftObjectPath> SoftObjectPaths, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority);
	ACETEAM_COROUTINES_API FCoroutineNodeRef _StreamAssets(TFunction<TArray<FSoftObjectPath> ()> SoftObjectPathsGetter, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority);

	template<typename T>
	FCoroutineNodeRef _StreamAssets(TArray<TSoftObjectPtr<T>> SoftObjects, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority)
	{
		TArray<FSoftObjectPath> SoftObjectPaths;
		SoftObjectPaths.Reserve(SoftObjects.Num());
		for (auto& SoftObjPtr : SoftObjects)
		{
			SoftObjectPaths.Add(SoftObjPtr.ToSoftObjectPath());
		}
		return _StreamAssets(SoftObjectPaths, AsyncLoadPriority);
	}

	template<typename T>
	FCoroutineNodeRef _StreamAssets(std::initializer_list<TSoftObjectPtr<T>> SoftObjects, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority)
	{
		TArray<FSoftObjectPath> SoftObjectPaths;
		SoftObjectPaths.Reserve(SoftObjects.size());
		for (auto& SoftObjPtr : SoftObjects)
		{
			SoftObjectPaths.Add(SoftObjPtr.ToSoftObjectPath());
		}
		return _StreamAssets(SoftObjectPaths, AsyncLoadPriority);
	}

	template<typename T>
	FCoroutineNodeRef _StreamAssets(TArray<TSoftClassPtr<T>> SoftClasses, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority)
	{
		TArray<FSoftObjectPath> SoftObjectPaths;
		SoftObjectPaths.Reserve(SoftClasses.Num());
		for (auto& SoftClassPtr : SoftClasses)
		{
			SoftObjectPaths.Add(SoftClassPtr.ToSoftObjectPath());
		}
		return _StreamAssets(SoftObjectPaths, AsyncLoadPriority);
	}

	template<typename T>
	FCoroutineNodeRef _StreamAssets(std::initializer_list<TSoftClassPtr<T>> SoftClasses, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority)
	{
		TArray<FSoftObjectPath> SoftObjectPaths;
		SoftObjectPaths.Reserve(SoftClasses.size());
		for (auto& SoftClassPtr : SoftClasses)
		{
			SoftObjectPaths.Add(SoftClassPtr.ToSoftObjectPath());
		}
		return _StreamAssets(SoftObjectPaths, AsyncLoadPriority);
	}

	template<typename TFunctor>
	typename TEnableIf<TIsFunctor_V<TFunctor>, FCoroutineNodeRef>::Type
	_StreamAssets(TFunctor Getter, TAsyncLoadPriority AsyncLoadPriority = FStreamableManager::DefaultAsyncLoadPriority)
	{
		typedef typename TFunctorTraits<TFunctor>::RetType RetType;
		if constexpr (std::is_same_v<RetType, TArray<FSoftObjectPath>>)
		{
			return _StreamAssets(TFunction<TArray<FSoftObjectPath> ()>(Getter), AsyncLoadPriority);
		}
		else if constexpr (Detail::TIsSoftObjectPtrTArray_V<RetType> || Detail::TIsSoftClassPtrTArray_V<RetType>)
		{
			return _StreamAssets([=]
			{
				auto RetVal = Getter();
				TArray<FSoftObjectPath> SoftObjectPaths;
				SoftObjectPaths.Reserve(RetVal.Num());
				for (auto& SoftObjPtr : RetVal)
				{
					SoftObjectPaths.Add(SoftObjPtr.ToSoftObjectPath());
				}
				return SoftObjectPaths;
			}, AsyncLoadPriority);
		}
		else
		{
			static_assert(std::is_same_v<RetType, TArray<FSoftObjectPath>> ||
				Detail::TIsSoftObjectPtrTArray_V<RetType> ||
				Detail::TIsSoftClassPtrTArray_V<RetType>,
				"Return type needs to be either an array of FSoftObjectPath, TSoftObjectPtr or TSoftClassPtr");
			return _Error();
		}
	}
}
