// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

#include "CoroutineNode.h"
#include "Containers/RingBuffer.h"

namespace ACETeam_Coroutines
{
	class ACETEAM_COROUTINES_API FCoroutineExecutor
	{
		enum
		{
			None = 1<<6,
			Active = ~(Completed|Failed),
		};

		struct FNodeExecInfo
		{
			FCoroutineNodePtr Node;
			FCoroutineNode* Parent = nullptr;
			EStatus Status = static_cast<EStatus>(None);
		};
		
		typedef TRingBuffer<FNodeExecInfo> ActiveNodes;
		ActiveNodes m_ActiveNodes;

		typedef TArray<FNodeExecInfo> SuspendedNodes;
		SuspendedNodes m_SuspendedNodes;

		//This flag indicates whether timers should just return instantly, and whether loops should allow more than one
		//step per frame
		bool m_bInstantMode = false;

		//Step count that's incremented each time the executor completes a full step (usually once per frame)
		//Used by loops to determine when they should stop their work for the step
		int m_StepCount= 0;

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
			bool operator() (FNodeExecInfo const& t) { return t.Node.Get() == m_Node; }
		};
	
	public:
		FCoroutineExecutor();

		//This is the main entry point for running a coroutine on an executor
		void EnqueueCoroutine(FCoroutineNodePtr const& Coroutine);

		bool IsInstant() const { return m_bInstantMode; }
		
		int StepCount() const { return m_StepCount; }
		
		bool HasRemainingWork() const
		{
			return m_ActiveNodes.Num() > 1 || m_SuspendedNodes.Num() > 0;
		}

		//Runs enqueued coroutines instantly until all finish their work
		//Use with caution, and make sure all custom time-sensitive decorators used are aware of instant mode
		void RunInstant()
		{
			m_bInstantMode = true;
			while (m_ActiveNodes.Num() > 1)
			{
				Step(0.0f);
			}
			m_bInstantMode = false;
		}

		void Step(float DeltaTime)
		{
			while (SingleStep(DeltaTime)) { continue; }
			Cleanup();
			++m_StepCount;
		}

		// Finds the root of the tree containing this node, and aborts the whole tree
		// Use of this function should be limited to the handling of fatal errors
		void AbortTree(FCoroutineNode* Coroutine);
		
		void AbortTree(FCoroutineNodePtr const& Coroutine) { AbortTree(Coroutine.Get()); }

		enum class EFindNodeResult
		{
			NotRunning,
			Suspended,
			Running,
			Aborted,
		};
		//Determines the status of a specific coroutine node in this executor
		EFindNodeResult FindCoroutineNode(FCoroutineNodePtr const& Node);

		//~ Internal functions
		
		//Internal - Enqueues a coroutine node for execution as soon as possible, if this is done while the Executor's step is
		//running this will be the next node to be evaluated.
		void EnqueueCoroutineNode(FCoroutineNodePtr const& Node, FCoroutineNode* Parent);

		// Internal - This function silently drops a coroutine node from the executor,
		// only telling the node itself about it.
		// Normally used by parallels such as races to abort branches.
		// It does not alert the parent, because either it is the parallel and already knows about it,
		// or it's an intermediate composite node that doesn't need to know because it will be aborted as well.
		void AbortNode(FCoroutineNode* Node);

		void AbortNode(FCoroutineNodePtr const& Node) { AbortNode(Node.Get()); }

		// This function can be used to force a task to end outside of the normal functioning of the executor.
		// For instance, a task whose only purpose is to wait suspended for something to happen can be notified in this
		// way. Note however that any dependent tasks will not be updated until the executor's next step.
		void ForceNodeEnd(FCoroutineNode* Node, EStatus Status);

		void ForceNodeEnd(FCoroutineNodePtr const& Node, EStatus Status) { ForceNodeEnd(Node.Get(), Status); }
	};

	namespace Detail
	{
		template <typename TLambda>
		class TDeferredCoroutineWrapper : public FCoroutineNode
		{
			TLambda m_Lambda;
			FCoroutineNodePtr m_Child;
		public:
			TDeferredCoroutineWrapper (TLambda const& Lambda) : m_Lambda(Lambda) {}
			virtual EStatus Start(FCoroutineExecutor* Executor) override
			{
				m_Child = m_Lambda();
				Executor->EnqueueCoroutineNode(m_Child,this);
				return Suspended; 
			}
			virtual EStatus OnChildStopped(FCoroutineExecutor*, EStatus Status, FCoroutineNode*) override { return Status; }
			virtual void End(FCoroutineExecutor* Executor, EStatus Status) override
			{
				if (Status == Aborted)
				{
					Executor->AbortNode(m_Child);
				}
			};
		};
	}

	//Make a simple coroutine out of a function, functor or lambda that returns a coroutine pointer
	template <typename TLambda>
	FCoroutineNodePtr _Deferred(TLambda& Lambda) { return MakeShared<Detail::TDeferredCoroutineWrapper<TLambda> >(Lambda); }
	
}