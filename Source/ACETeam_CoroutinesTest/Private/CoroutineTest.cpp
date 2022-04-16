// Copyright ACE Team Software S.A. All Rights Reserved.


#include "CoroutineTest.h"

#include "CoroutineElements.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ACETeam_CoroutinesTest)

// Sets default values
ACoroutineTest::ACoroutineTest()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

PRAGMA_DISABLE_OPTIMIZATION

// Called when the game starts or when spawned
void ACoroutineTest::BeginPlay()
{
	Super::BeginPlay();
	using namespace ACETeam_Coroutines;
	Executor.EnqueueCoroutine
	(
		_Race
		(
			_Scope ([]{ UE_LOG(LogTemp, Log, TEXT("Loop done")); })
			(
				_Loop
				(
					_Seq
					(
						[Diff = 0.0f]() mutable
						{
							Diff += 0.05f;
							UE_LOG(LogTemp, Log, TEXT("Waiting for %.2f"), 1.0f - Diff);
							return _Wait(1.0f - Diff);
						},
						//_Wait(1.0f),
						[=, Counter = 0]() mutable 
						{
							UE_LOG(LogTemp, Log, TEXT("Hi %s"), *GetName());
							if (++Counter > 5)
								return false;
							return true;
						}
					)
				)
			),
			_Wait(10.0f)
		)
	);
}

// Called every frame
void ACoroutineTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Executor.Step(DeltaTime);
}
