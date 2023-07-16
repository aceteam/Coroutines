// Copyright Daniel Amthauer. All Rights Reserved


#include "CoroutineEventBPWrapper.h"

void UCoroutineEventBPWrapper::FireEvent()
{
	if (EventPtr.IsValid())
	{
		EventPtr.Pin()->Broadcast();
	}
}

UCoroutineEventBPWrapper* UCoroutineEventBPWrapper::MakeFromEvent(ACETeam_Coroutines::TEventRef<void> EventRef)
{
	UCoroutineEventBPWrapper* Obj = NewObject<UCoroutineEventBPWrapper>(GetTransientPackage());
	Obj->EventPtr = EventRef;
	return Obj;
}
