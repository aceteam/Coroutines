// Copyright ACE Team Software S.A. All Rights Reserved.

#include "ACETeam_CoroutinesModule.h"

#include "CoroutineLog.h"

#if WITH_ACETEAM_COROUTINE_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebugger/GameplayDebuggerCategory_Coroutines.h"
#endif

void FACETeam_CoroutinesModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

#if WITH_ACETEAM_COROUTINE_DEBUGGER
	//If the gameplay debugger is available, register the category and notify the editor about the changes
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();

	GameplayDebuggerModule.RegisterCategory("Coroutines", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Coroutines::MakeInstance));

	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif
}

void FACETeam_CoroutinesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

DEFINE_LOG_CATEGORY(LogACETeamCoroutines);

IMPLEMENT_MODULE(FACETeam_CoroutinesModule, ACETeam_Coroutines)