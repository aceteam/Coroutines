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
constexpr ESPMode DefaultSPMode = ESPMode::NotThreadSafe;
typedef TSharedPtr<FCoroutineNode, DefaultSPMode> FCoroutineNodePtr;

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
	
}