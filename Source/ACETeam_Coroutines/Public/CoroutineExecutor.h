// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"
#include "Containers/RingBuffer.h"

namespace ACETeam_Coroutines
{
	namespace Detail
	{
		class FNamedScopeNode;
	}

	class ACETEAM_COROUTINES_API FCoroutineExecutor
	{
		enum
		{
			None = 1<<6,
			Active = ~(Completed|Failed),
			Finished = (Completed|Failed)
		};

		struct FNodeExecInfo
		{
			FCoroutineNodePtr Node;
			FCoroutineNode* Parent = nullptr;
			EStatus Status = static_cast<EStatus>(None);
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			Detail::FNamedScopeNode* ScopeNode = nullptr;
#endif
		};
		
		typedef TRingBuffer<FNodeExecInfo> ActiveNodes;
		ActiveNodes m_ActiveNodes;

		typedef TArray<FNodeExecInfo> SuspendedNodes;
		SuspendedNodes m_SuspendedNodes;

		//Step count that's incremented each time the executor completes a full step (usually once per frame)
		//Used by loops to determine when they should stop their work for the step
		int m_StepCount= 0;

#if WITH_ACETEAM_COROUTINE_DEBUGGER
		int32 LastCpuTraceSpecId = 0;
		int32 CurrentTraceDepth = 0;
		const Detail::FNamedScopeNode* LastScope = nullptr;
		const FNodeExecInfo* CurrentExecInfo = nullptr;
		void TraceScopeCleanup();
#endif

		bool SingleStep(float DeltaTime);
		
		void ProcessNodeEnd(FNodeExecInfo& Info, EStatus Status);

		void Cleanup();

		static bool IsActive(const FNodeExecInfo& NodeInfo)
		{
			return (NodeInfo.Status & Active) != 0;
		}
		
		struct NodeIs
		{
			FCoroutineNode* m_Node;
			NodeIs(FCoroutineNode* Node) : m_Node(Node) {}
			bool operator() (FNodeExecInfo const& t) const { return t.Node.Get() == m_Node && t.Status != Aborted; }
		};
	
	public:
		FCoroutineExecutor();
		~FCoroutineExecutor();

		//This is the main entry point for running a coroutine on an executor
		void EnqueueCoroutine(FCoroutineNodeRef const& Coroutine);
		
		int StepCount() const { return m_StepCount; }
		
		bool HasRemainingWork() const
		{
			return m_ActiveNodes.Num() > 1 || m_SuspendedNodes.Num() > 0;
		}

		void Step(float DeltaTime)
		{
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			TRACE_CPUPROFILER_EVENT_SCOPE(FCoroutineExecutor::Step);
#endif
			
			while (SingleStep(DeltaTime)) { continue; }
			
#if WITH_ACETEAM_COROUTINE_DEBUGGER
			TraceScopeCleanup();
#endif
			
			Cleanup();
			++m_StepCount;
		}

		// Finds the root of the tree containing this node, and aborts the whole tree
		// Use of this function should be limited to the handling of fatal errors
		void AbortTree(FCoroutineNode* Coroutine);
		
		void AbortTree(FCoroutineNodeRef const& Coroutine) { AbortTree(&Coroutine.Get()); }

		enum class EFindNodeResult
		{
			NotRunning,
			Suspended,
			Running,
			Aborted,
		};
		//Determines the status of a specific coroutine node in this executor
		EFindNodeResult FindCoroutineNode(FCoroutineNodeRef const& Node);

		//~ Internal functions
		
		//Internal - Enqueues a coroutine node for execution as soon as possible, if this is done while the Executor's step is
		//running this will be the next node to be evaluated.
		void EnqueueCoroutineNode(FCoroutineNodeRef const& Node, FCoroutineNode* Parent);

		// Internal - This function silently drops a coroutine node from the executor,
		// only telling the node itself about it.
		// Normally used by parallels such as races to abort branches.
		// It does not alert the parent, because either it is the parallel and already knows about it,
		// or it's an intermediate composite node that doesn't need to know because it will be aborted as well.
		void AbortNode(FCoroutineNode* Node);

		void AbortNode(FCoroutineNodeRef const& Node) { AbortNode(&Node.Get()); }

		// This function can be used to force a task to end outside of the normal functioning of the executor.
		// For instance, a task whose only purpose is to wait suspended for something to happen can be notified in this
		// way. Note however that any dependent tasks will not be updated until the executor's next step.
		void ForceNodeEnd(FCoroutineNode* Node, EStatus Status);

		void ForceNodeEnd(FCoroutineNodeRef const& Node, EStatus Status) { ForceNodeEnd(&Node.Get(), Status); }

		static bool IsFinished(EStatus Status) { return (Status & Finished) != 0; }

#if WITH_ACETEAM_COROUTINE_DEBUGGER
#if PLATFORM_COMPILER_CLANG
	public:
#else
	private:
		friend class FGameplayDebuggerCategory_Coroutines;
#endif
		struct FDebuggerEntry
		{
			FString Name;
			EStatus Status;
			double StartTime;
			double EndTime = -1.0f;
		};
		struct FDebuggerRow
		{
			FCoroutineNode* Node;
			FCoroutineNode* Parent;
			FCoroutineNode* Scope;
			int Depth = 0;
			bool bIsDeferredNodeGenerator:1;
			bool bIsScope:1;
			bool bIsLeaf:1;
			TRingBuffer<FDebuggerEntry> Entries;
			bool operator==(const FCoroutineNode* InNode) const { return Node == InNode; }
		};
		TArray<FDebuggerRow> DebuggerInfo;
#endif
	private:
		void TrackNodeStart(FCoroutineNode* Node, FCoroutineNode* Parent, EStatus Status);
		void TrackNodeSuspendFromUpdate(FCoroutineNode* Node);
		void TrackNodeEnd(FCoroutineNode* Node, EStatus Status);
	};
}