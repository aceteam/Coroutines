// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "FunctionTraits.h"
#include "Engine/StreamableManager.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		class FAsyncRunnerBase : public FCoroutineNode, public TSharedFromThis<FAsyncRunnerBase, DefaultSPMode>
		{
		};
			
		template <ESPMode SPMode> 
		typename TEnableIf<SPMode == ESPMode::ThreadSafe, TSharedPtr<void, ESPMode::ThreadSafe>>::Type
		GetThreadSafeRef(FAsyncRunnerBase* Owner)
		{
			return Owner->AsShared();
		}
		template <ESPMode SPMode> 
		typename TEnableIf<SPMode != ESPMode::ThreadSafe, TSharedPtr<void, ESPMode::ThreadSafe>>::Type
		GetThreadSafeRef(FAsyncRunnerBase* Owner)
		{
			auto Wrapper = MakeShared<TSharedPtr<FAsyncRunnerBase, DefaultSPMode>, ESPMode::ThreadSafe>(Owner->AsShared());
			return Wrapper;
		};
		
		template <typename TLambda>
		class TAsyncRunner : public FAsyncRunnerBase
		{
		public:
			TAsyncRunner(ENamedThreads::Type _NamedThread, TLambda const& _Lambda)
			: NamedThread(_NamedThread)
			, Lambda(_Lambda)
			{}
			
			virtual EStatus Start(FCoroutineExecutor* Exec) override
			{
				CachedExec = Exec;
				auto SafeRef = GetThreadSafeRef<DefaultSPMode>(this);
				AsyncTask(NamedThread, [this, SafeRef]
				{
					Lambda();
					AsyncTask(ENamedThreads::GameThread, [this, SafeRef]
					{
						if (CachedExec)
						{
							CachedExec->ForceNodeEnd(this, Completed);
						}
					});
				});
				return Suspended;
			}
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override
			{
				CachedExec = nullptr;
			}
		private:
			ENamedThreads::Type NamedThread;
			TLambda Lambda;
			FCoroutineExecutor* CachedExec = nullptr;
		};
	}

	template <typename TLambda>
	FCoroutineNodeRef _Async(ENamedThreads::Type NamedThread, TLambda const& Lambda)
	{
		return MakeShared<Detail::TAsyncRunner<TLambda>, DefaultSPMode>(NamedThread, Lambda);
	}

	namespace Detail
	{
		class ACETEAM_COROUTINES_API FAsyncObjectLoadNode : public FCoroutineNode, public TSharedFromThis<FAsyncObjectLoadNode, DefaultSPMode>
		{
		public:
			FAsyncObjectLoadNode(TArray<FSoftObjectPath>&& _ObjectsToLoad)
				: ObjectsToLoad(MoveTemp(_ObjectsToLoad))
			{}
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
			virtual void HandleLoaded(){}
		protected:
			TArray<FSoftObjectPath> ObjectsToLoad;
			TSharedPtr<FStreamableHandle> StreamableHandle;
			FCoroutineExecutor* CachedExec = nullptr;
		};

		template <typename TLambda, typename TUObjectType>
		class TAsyncObjectLoadNode : public FAsyncObjectLoadNode
		{
		public:
			TAsyncObjectLoadNode(TArray<FSoftObjectPath>&& _ObjectsToLoad, TLambda const& _Lambda)
				: FAsyncObjectLoadNode(MoveTemp(_ObjectsToLoad))
				, Lambda(_Lambda)
			{}
			
			typedef typename TFunctionTraits<decltype(&TLambda::operator())>::template NthArg<0>::Type FirstArgType;
			typedef typename TRemoveReference<FirstArgType>::Type FirstArgTypeNoRef;
			virtual void HandleLoaded() override
			{
				static_assert(TIsTArray<FirstArgTypeNoRef>::Value && TIsSame<typename FirstArgTypeNoRef::ElementType, TUObjectType*>::Value, "Lambda must receive an array of pointers to a UObject type");
				TArray<UObject*> LoadedAssets;
				StreamableHandle->GetLoadedAssets(LoadedAssets);
				Lambda(reinterpret_cast<TArray<TUObjectType*>&>(LoadedAssets));
			}
		private:
			TLambda Lambda;
		};
	}

	ACETEAM_COROUTINES_API FCoroutineNodeRef _AsyncLoadObjects(TArrayView<const TSoftObjectPtr<UObject>> SoftObjects);

	namespace Detail
	{
		ACETEAM_COROUTINES_API TArray<FSoftObjectPath> ConvertToSoftObjectPaths(TArrayView<const TSoftObjectPtr<UObject>> SoftObjectPtrs);
	}

	//Version that just casts away the specific type of TSoftObjectPtr because we don't need to know it
	template <typename T>
	FCoroutineNodeRef _AsyncLoadObjects(TArrayView<const TSoftObjectPtr<T>> SoftObjects)
	{
		return _AsyncLoadObjects(reinterpret_cast<TArrayView<const TSoftObjectPtr<>>&>(SoftObjects));
	}

	template <typename TLambda, typename TUObjectType>
	FCoroutineNodeRef _AsyncLoadObjects(TArrayView<const TSoftObjectPtr<TUObjectType>> SoftObjects, TLambda const& Lambda)
	{
		auto Paths = Detail::ConvertToSoftObjectPaths(reinterpret_cast<TArrayView<const TSoftObjectPtr<>>&>(SoftObjects));
		return MakeShared<Detail::TAsyncObjectLoadNode<TLambda, TUObjectType>, DefaultSPMode>(MoveTemp(Paths), Lambda);
	}

	template <typename TLambda, typename TUObjectType, typename TAllocator>
	FCoroutineNodeRef _AsyncLoadObjects(TArray<TSoftObjectPtr<TUObjectType>, TAllocator> SoftObjects, TLambda const& Lambda)
	{
		auto ArrayView = MakeArrayView(SoftObjects);
		//Bypass not being able to convert to TArrayView<const T> from TArray<T>
		return _AsyncLoadObjects(reinterpret_cast<TArrayView<const TSoftObjectPtr<TUObjectType>>&>(ArrayView), Lambda);
	}
}
