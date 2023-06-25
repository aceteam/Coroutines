// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineExecutor.h"
#include "CoroutineNode.h"
#include "FunctionTraits.h"

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
		typename TEnableIf<SPMode != ESPMode::ThreadSafe, TSharedPtr<void, ESPMode::ThreadSafe>>::Type GetThreadSafeRef(FAsyncRunnerBase* Owner)
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
				if (NamedThread == ENamedThreads::GameThread && IsInGameThread())
				{
					Lambda();
					return Completed;
				}
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

	//Suspends execution of the coroutine until the lambda has finished executing in another thread. If ENamedThreads::GameThread is passed in, the lambda will block the game thread until finished
	template <typename TLambda>
	FCoroutineNodeRef _Async(ENamedThreads::Type NamedThread, TLambda const& Lambda)
	{
		return MakeShared<Detail::TAsyncRunner<TLambda>, DefaultSPMode>(NamedThread, Lambda);
	}
}
