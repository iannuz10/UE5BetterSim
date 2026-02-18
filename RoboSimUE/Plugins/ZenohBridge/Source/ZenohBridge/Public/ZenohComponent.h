#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZenohComponent.generated.h"

// Forward Declaration
class FZenohBackend;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZenohMessage, FString, Message);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ZENOHBRIDGE_API UZenohComponent : public UActorComponent
{
	GENERATED_BODY()

public:    
	UZenohComponent();
	virtual ~UZenohComponent(); // Destructor needed for PIMPL

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Zenoh")
	bool Publish(FString Topic, FString Message);

	UPROPERTY(BlueprintAssignable, Category = "Zenoh")
	FOnZenohMessage OnMessageReceived;

private:
	// The PIMPL (Pointer to Implementation)
	FZenohBackend* Backend = nullptr;
};