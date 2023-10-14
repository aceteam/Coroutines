// Copyright ACE Team Software S.A. All Rights Reserved.

#include "CoroutineTest.h"

#include "CoroutineElements.h"
#include "CoroutineEvents.h"
#include "CoroutinesWorldSubsystem.h"
#include "CoroutinesSubsystem.h"
#include "CoroutineAsync.h"
#include "CoroutineGenerator.h"
#include "CoroutineSemaphores.h"
#include "DrawDebugHelpers.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ACETeam_CoroutinesTest)

// Sets default values
ACoroutineTest::ACoroutineTest()
{
	PrimaryActorTick.bCanEverTick = false;
}

using namespace ACETeam_Coroutines;

FCoroutineNodeRef _CoroutineEventsTest();

FCoroutineNodeRef _CoroutineTest(UWorld* World, FString TextToLog)
{
	//Shared ptr values can be used to share data between different execution branches,
	// or different steps in your coroutine
	//This way they're guaranteed to share the same lifetime as the code that's using them
	auto SharedValue = MakeShared<int>(0);

	constexpr int TextPrintTimes = 5;
	
	//_Seq concatenates coroutine elements in a sequence. It runs one after the other until
	// one of them fails
	//If one of them finishes during a frame, the next in the sequence will be evaluated
	// in the same frame
	return _Seq(
		//_Race evaluates each of its contained elements once per frame, in the declared order, until
		// one finishes its execution (successfully or unsuccessfully)
		_Race(
			//a _Loop will run its contained element once per frame, until it fails or is aborted
			_Loop(
				//_Weak creates an element that will only evaluate its lambda if the passed in
				// object is valid on evaluation. It will also return failure if the object is not valid
				// so it will cause the coroutine to stop, unless that failure is caught somewhere.
				_Weak(World, [=] { DrawDebugPoint(World, FVector::ZeroVector, 10, FColor::White); })
			),
			//_LoopSeq is a shortcut for a _Loop containing a _Seq
			_LoopSeq(
				//_Wait will pause this execution branch for the specified time, this can also be a shared float variable, or a lambda that returns a float
				_Wait(1.0f),
				[=] {
					UE_LOG(LogTemp, Log, TEXT("This text will print once per second %d times"), TextPrintTimes);
				},
				//mutable lambdas can be used to have local variables that share their lifetime
				[=, Counter = 0] () mutable {
					UE_LOG(LogTemp, Log, TEXT("%d/%d"), Counter+1, TextPrintTimes);
					//When this returns false, the containing loop will finish
					return ++Counter < TextPrintTimes;
				},
				[=] {
					//If one execution branch wants to pass data to a different
					//branch, you can use shared variables
					*SharedValue += FMath::RandHelper(10);
				}
			)
		),
		_CoroutineEventsTest(),
		[=] { 
			UE_LOG(LogTemp, Log, TEXT("This log will appear after the race above finishes"));
			UE_LOG(LogTemp, Log, TEXT("The final value stored in SharedValue is  %d"), *SharedValue); 
		},
		_Async(ENamedThreads::AnyBackgroundThreadNormalTask, []
		{
			//You can do any sort of work that blocks the thread inside this _Async lambda.
			//The coroutine branch that launched it will be suspended, not blocking the game thread
			//(Unless you passed in ENamedThreads::GameThread as the thread parameter)
			FPlatformProcess::Sleep(1);
		}),
		[=]
		{
			UE_LOG(LogTemp, Log, TEXT("This log appears after the async task above finishes\nThe string passed in as a parameter when the coroutine started was \"%s\""), *TextToLog);
		}
	);
}

FCoroutineNodeRef _CoroutineEventsTest()
{
	auto TestEvent1 = MakeEvent<int, float>();
	auto TestEvent2 = MakeEvent();
	return
	//Events
		_Race(
			//Events can be used to communicate between different branches in a coroutine
			_Seq(
				_Wait(1.0f),
				[=]
				{
					//They can pass data, if they were declared with parameter types...
					TestEvent1->Broadcast(5, 10.0f);
				},
				_Wait(1.0f),
				[=]
				{
					//... or they can simply be broadcast, if they're of <void> type
					TestEvent2->Broadcast();
				}
			),
			//Listening to several events in a race will only call the first declared handler for the event that was broadcast first.
			//This is also an example of the different kind of event handlers you can have:
			_Race(
				//1. Returns a coroutine when the event fires, which will run in its place
				_WaitFor(TestEvent1, [](int Value1, float Value2)
				{
					UE_LOG(LogTemp, Log, TEXT("Received values %d %f"), Value1, Value2);
					//You can return a coroutine when the event arrives, and the coroutine will run in this same branch
					return
					//Scopes are equivalent to ON_SCOPE_EXIT. They run the lambda in their first set of parentheses when the scope exits, regardless of why the scope is exiting
					//Useful for doing cleanup when your branch is aborted.
					_Scope([]{ UE_LOG(LogTemp, Log, TEXT("Wait aborted")); })
					(
						_Seq(
							[=]
							{
								UE_LOG(LogTemp, Log, TEXT("Wait Started for %d seconds"), Value1);
							},
							_Wait(Value1),
							[=]
							{
								check(false); // This log should not appear, because the wait will be interrupted by the first _WaitFor(TestEvent2...)
								UE_LOG(LogTemp, Log, TEXT("Wait Ended after %d seconds"), Value1);
							}
						)
					); //this will wait for the number of seconds received from the event
				}),
				//2. Has a lambda, but doesn't return anything which will simply complete the race when it triggers
				_WaitFor(TestEvent2, []
				{
					UE_LOG(LogTemp, Log, TEXT("This log will appear when TestEvent2 is broadcast"));
				}),
				//3. Returns a boolean, which could fail the race when handled, but will be skipped because the race will be completed by 2.
				_WaitFor(TestEvent2, []
				{
					check(false);
					return false;
				}),
				//4. Events that have no parameters can have no lambda, this will simply suspend until the event is broadcast
				_WaitFor(TestEvent2)
			)
		);
}

void SemaphoreTest()
{
	auto Semaphore = MakeSemaphore(4);
	UCoroutinesSubsystem::Get().StartCoroutine(
		_GenerateChildren(_Sync(), [&](auto AddChild)
		{
			auto _MyCoroutine = [=](int Num)
			{
				return
				_Seq(
					[=] { UE_LOG(LogTemp, Log, TEXT("Coroutine instance %d will %s"), Num, Semaphore->IsAvailable() ? TEXT("go through semaphore now") : TEXT("wait at semaphore")); },
					_SemaphoreScope(Semaphore)
					(
						_Seq(
							[=] { UE_LOG(LogTemp, Log, TEXT("Coroutine instance %d entering semaphore"), Num); },
							_Wait(1.0f),
							[=] { UE_LOG(LogTemp, Log, TEXT("Coroutine instance %d exiting semaphore"), Num); }
						)
					)
				);
			};
			for (int i = 0; i < 10; ++i)
			{
				AddChild(_MyCoroutine(i+1));
			}
		})
	);
}

void ACoroutineTest::BeginPlay()
{
	Super::BeginPlay();

	UCoroutinesWorldSubsystem::Get(this).StartCoroutine(
		_Seq(
			_CoroutineTest(GetWorld(), TEXT("test string")),
			[=] { SemaphoreTest(); }
		)
	);
}
