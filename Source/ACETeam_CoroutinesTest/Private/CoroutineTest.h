// Copyright ACE Team Software S.A. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CoroutineExecutor.h"

#include "CoroutineTest.generated.h"

UCLASS()
class ACoroutineTest : public AActor
{
	GENERATED_BODY()

	ACETeam_Coroutines::FCoroutineExecutor Executor;
	
public:	
	// Sets default values for this actor's properties
	ACoroutineTest();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
