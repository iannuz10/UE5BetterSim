#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "RoboSimAgent.generated.h"

class UZenohTopicListener;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RoboSim|Agent")
    TMap<FString, FTransform> InitialTransformsCache;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoboSim|Agent")
    bool bIsMobile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoboSim|Agent")
    FString RobotID;

    UFUNCTION(BlueprintImplementableEvent, Category = "RoboSim|Events")
    void ReceiveAgentStateUpdated();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

    static TSet<FString> GlobalRobotIDs;
    FString RegisteredRobotID;

public:    
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "RoboSim|Agent")
    bool CacheJointComponents();

    FString GetRegisteredID() const { return RegisteredRobotID; }

    bool ApplyState(TSharedPtr<FJsonObject> JsonObject);
};
