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
class FCoroutine;
typedef TSharedPtr<FCoroutine> FCoroutinePtr;

class FCoroutineExecutor;

class ACETEAM_COROUTINES_API FCoroutine
{
public:
	virtual ~FCoroutine(){}
	virtual EStatus Start(FCoroutineExecutor* Exec) { return Running; }
	virtual EStatus Update(FCoroutineExecutor* Exec, float dt) { return Running; }
	virtual void End(FCoroutineExecutor* Exec, EStatus Status) {};
	virtual EStatus OnChildStopped(FCoroutineExecutor* Exec, EStatus Status, FCoroutine* Child) { return Running; }
};

namespace detail
{
	template <typename F>
	class FLambdaCoroutine: public FCoroutine
	{
		F	m_f;
	public:
		FLambdaCoroutine (F const & f) : m_f(f){}
		virtual EStatus Start(FCoroutineExecutor*) override { m_f(); return Completed;}
	};

	template <typename F>
	class FConditionLambdaCoroutine : public FCoroutine
	{
		F m_f;
	public:
		FConditionLambdaCoroutine (F const & f) : m_f(f) {}
		virtual EStatus Start(FCoroutineExecutor*) override { return m_f() ? Completed : Failed; }
	};
}

//Make a simple task out of a function, functor or lambda
template <typename F>
FCoroutinePtr _Lambda(F const & f)
{
	return MakeShared<detail::FLambdaCoroutine<F> >(f);
}

//Make a simple task out of a function, functor or lambda
template <typename F>
FCoroutinePtr _ConditionalLambda(F const & f)
{
	return MakeShared<detail::FConditionLambdaCoroutine<F> >(f);
}
	
}