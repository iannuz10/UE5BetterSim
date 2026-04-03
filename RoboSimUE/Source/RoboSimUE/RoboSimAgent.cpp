#include "RoboSimAgent.h"

ARoboSimAgent::ARoboSimAgent()
{
    PrimaryActorTick.bCanEverTick = true;
    bIsMobile = false;
    RootFrameComponent = nullptr;
}

void ARoboSimAgent::BeginPlay()
{
    Super::BeginPlay();
}

void ARoboSimAgent::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ARoboSimAgent::CacheJointComponents(const TArray<FString>& JointNames, bool bInIsMobile, USceneComponent* InRootFrameComponent)
{
    // To be implemented in Task 4
}

void ARoboSimAgent::ApplyUnifiedState(const FString& JsonPayload)
{
    // To be implemented in Task 5
}
