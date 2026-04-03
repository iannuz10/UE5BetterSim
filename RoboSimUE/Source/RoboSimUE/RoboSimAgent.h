#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "RoboSimAgent.generated.h"

UCLASS()
class ROBOSIMUE_API ARoboSimAgent : public AActor
{
    GENERATED_BODY()
    
public:    
    ARoboSimAgent();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoboSim|Agent")
    TMap<FString, USceneComponent*> JointsCache;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoboSim|Agent")
    USceneComponent* RootFrameComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoboSim|Agent")
    bool bIsMobile;

public:    
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "RoboSim|Agent")
    bool CacheJointComponents();

    UFUNCTION(BlueprintCallable, Category = "RoboSim|Agent")
    bool ApplyState(const FString& JsonPayload);
};
