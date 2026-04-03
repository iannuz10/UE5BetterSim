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
    bIsMobile = bInIsMobile;
    RootFrameComponent = InRootFrameComponent;
    JointsCache.Empty();

    TArray<USceneComponent*> AllComponents;
    GetComponents<USceneComponent>(AllComponents);

    for (const FString& JointName : JointNames)
    {
        FString TargetName = JointName + TEXT("_DefaultSceneRoot");
        USceneComponent* FoundComponent = nullptr;

        for (USceneComponent* Comp : AllComponents)
        {
            if (Comp->GetName() == TargetName)
            {
                FoundComponent = Comp;
                break;
            }
        }

        if (FoundComponent)
        {
            JointsCache.Add(JointName, FoundComponent);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ARoboSimAgent: Joint component not found for %s (Expected name: %s)"), *JointName, *TargetName);
        }
    }
}

void ARoboSimAgent::ApplyUnifiedState(const FString& JsonPayload)
{
    // To be implemented in Task 5
}
