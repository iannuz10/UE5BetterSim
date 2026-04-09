#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "ZenohSubsystem.h"
#include "SimWorldManager.generated.h"


class UZenohTopicListener;

UCLASS(Blueprintable)
class ROBOSIMUE_API ASimWorldManager : public AActor
{
    GENERATED_BODY()

public:
    ASimWorldManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Network")
    TArray<FZenohConnectionInfo> ConnectionSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Network")
    FName WorldConnectionName = TEXT("DefaultConnection");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Network")
    FString WorldTopic = TEXT("sim/world/state");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation|Network")
    TArray<FName> WorldActorTags;

    UFUNCTION(BlueprintImplementableEvent, Category = "Simulation|Events")
    void ReceiveWorldStateUpdated();

    UFUNCTION(BlueprintCallable, Category = "Simulation|Network")
    void PublishWorldInit();

private:
    void HandleWorldStateUpdate(TSharedPtr<FJsonObject> JsonObject);

    UPROPERTY()
    TArray<UZenohTopicListener*> ActiveListeners;
};
