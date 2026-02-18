#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZenohComponent.generated.h"

// Forward Declaration
class FZenohBackend;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZenohPosition, FVector, NewLocation);
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

	// String Event (For Debugging)
	UPROPERTY(BlueprintAssignable, Category = "Zenoh")
	FOnZenohMessage OnMessageReceived;

	// Position Event (For movements)
	UPROPERTY(BlueprintAssignable, Category = "Zenoh")
	FOnZenohPosition OnPositionReceived;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh Configuration")
	FString SubscriptionTopic = "sim/command";

	UFUNCTION(BlueprintCallable, Category = "Zenoh")
	void Subscribe(FString NewTopic);

private:
	// The PIMPL (Pointer to Implementation)
	FZenohBackend* Backend = nullptr;
	
	// 4. Helper function to parse JSON
	void HandleZenohMessage(const FString& Payload);
};