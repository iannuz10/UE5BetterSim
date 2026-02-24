#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZenohBackend.h"
#include "ZenohComponent.generated.h"


UENUM(BlueprintType)
enum class EZenohProtocol : uint8
{
	TCP UMETA(DisplayName = "TCP (Reliable, Localhost)"),
	UDP UMETA(DisplayName = "UDP (Fast, Networked)")
};

UENUM(BlueprintType)
enum class EZenohMode : uint8
{
	Client UMETA(DisplayName = "Client (Connects to Router/Peer)"),
	Peer UMETA(DisplayName = "Peer (Direct Mesh Network)")
};


// Forward Declaration
class FZenohBackend;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZenohPosition, FVector, NewLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZenohMessage, FString, Message);

UCLASS( ClassGroup=(Networking), meta=(BlueprintSpawnableComponent) )
class ZENOHBRIDGE_API UZenohComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UZenohComponent();
	virtual ~UZenohComponent(); // Destructor needed for PIMPL

	// ==========================================
	// EDITOR SETTINGS 
	// ==========================================
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	bool bAutoConnectOnBeginPlay = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	EZenohMode ConnectionMode = EZenohMode::Peer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	EZenohProtocol Protocol = EZenohProtocol::TCP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	FString IPAddress = TEXT("0.0.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	int32 Port = 7447;

	// A default topic the user can type in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	FString DefaultTopic = TEXT("sim/data");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Configuration")
	FString SubscriptionTopic = TEXT("sim/command");

	// String Event
	UPROPERTY(BlueprintAssignable, Category = "Zenoh")
	FOnZenohMessage OnMessageReceived;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:    
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
	bool Publish(FString Topic, FString Message);

	UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
	void Subscribe(FString NewTopic);
	
	// Call this to manually connect using the settings above
	UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
	bool Connect();

	// Safely closes the session
	UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
	void Disconnect();

private:
	// PIMPL (Pointer to Implementation)
	FZenohBackend* Backend = nullptr;
	
	// 4. Helper function to parse JSON
	void HandleZenohMessage(const FString& Payload);
};