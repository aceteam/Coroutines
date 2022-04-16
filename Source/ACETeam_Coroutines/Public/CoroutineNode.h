// Copyright ACE Team Software S.A. All Rights Reserved.
#pragma once

namespace ACETeam_Coroutines
{
enum EStatus
{
	Completed = 1<<1,
	Failed = 1<<2,
	Running = 1<<3,
	Suspended = 1<<4,
	Aborted = 1<<5,
};
class FCoroutineNode;
typedef TSharedPtr<FCoroutineNode> FCoroutineNodePtr;

class FCoroutineExecutor;

class ACETEAM_COROUTINES_API FCoroutineNode
{
public:
	virtual ~FCoroutineNode(){}
	virtual EStatus Start(FCoroutineExecutor* Exec) { return Running; }
	virtual EStatus Update(FCoroutineExecutor* Exec, float dt) { return Running; }
	virtual void End(FCoroutineExecutor* Exec, EStatus Status) {};
	virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutineNode* Child) { return Running; }
};

namespace Detail
{
	template <typename F>
	class FLambdaCoroutine: public FCoroutineNode
	{
		F	m_f;
	public:
		FLambdaCoroutine (F const & f) : m_f(f){}
		virtual EStatus Start(FCoroutineExecutor*) override { m_f(); return Completed;}
	};

	template <typename F>
	class FConditionLambdaCoroutine : public FCoroutineNode
	{
		F m_f;
	public:
		FConditionLambdaCoroutine (F const & f) : m_f(f) {}
		virtual EStatus Start(FCoroutineExecutor*) override { return m_f() ? Completed : Failed; }
	};
}

//Make a simple task out of a function, functor or lambda
template <typename F>
FCoroutineNodePtr _Lambda(F const & f)
{
	return MakeShared<Detail::FLambdaCoroutine<F> >(f);
}

//Make a simple task out of a function, functor or lambda
template <typename F>
FCoroutineNodePtr _ConditionalLambda(F const & f)
{
	return MakeShared<Detail::FConditionLambdaCoroutine<F> >(f);
}
	
}