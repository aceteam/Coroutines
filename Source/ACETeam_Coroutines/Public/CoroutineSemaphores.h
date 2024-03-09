// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineElements.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		class FSemaphore;
	}
	
	typedef TSharedRef<Detail::FSemaphore> FSemaphoreRef;
	
	namespace Detail
	{
		class ACETEAM_COROUTINES_API FSemaphoreHandlerNode : public FCoroutineDecorator, public TSharedFromThis<FSemaphoreHandlerNode, DefaultSPMode>
		{
		public:
			FSemaphoreHandlerNode(FSemaphoreRef const& InSemaphore)
				: Semaphore(InSemaphore)
			{}
			void Resume();
			
		private:
			FSemaphoreRef Semaphore;
			FCoroutineExecutor* CachedExec = nullptr;
			
			virtual EStatus Start(FCoroutineExecutor* Exec) override;
			virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) override;
			virtual void End(FCoroutineExecutor* Exec, EStatus Status) override;
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			virtual FString Debug_GetName() const override { return TEXT("Semaphore"); }
#endif
		};

		typedef TSharedRef<FSemaphoreHandlerNode, DefaultSPMode> FSemaphoreHandlerRef;
		
		class ACETEAM_COROUTINES_API FSemaphore
		{
		public:
			FSemaphore(int InMaxActive)
			: MaxActive(InMaxActive)
			{
				check(MaxActive > 0);
			}
			bool IsAvailable() const { return CurrentActive < MaxActive; }
			bool TryTake(FSemaphoreHandlerRef const& Handler);
			bool DropFromQueue(FSemaphoreHandlerRef const& Handler);
			void Release();
			void SetMaxActive(int NewMaxActive);
			
		private:
			int MaxActive;
			int CurrentActive = 0;
			TArray<FSemaphoreHandlerRef, TInlineAllocator<1>> QueuedHandlers;
		};

		struct ACETEAM_COROUTINES_API FSemaphoreHelper
		{
			FSemaphoreHelper(FSemaphoreRef const& InSemaphore) :
				Semaphore(InSemaphore)
			{}
			
			FSemaphoreRef Semaphore;

			template <typename TChild>
			FCoroutineNodeRef operator() (TChild&& ScopeBody)
			{
				auto Handler = MakeShared<FSemaphoreHandlerNode, DefaultSPMode>(Semaphore);
				AddCoroutineChild(Handler, ScopeBody);
				return Handler;
			}
		};
	}
	
	ACETEAM_COROUTINES_API Detail::FSemaphoreHelper _SemaphoreScope(FSemaphoreRef const& Semaphore);

	ACETEAM_COROUTINES_API FSemaphoreRef MakeSemaphore(int MaxActive);
}