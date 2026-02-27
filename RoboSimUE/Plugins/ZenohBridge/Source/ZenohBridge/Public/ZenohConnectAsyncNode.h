#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZenohSubsystem.h"
#include "ZenohConnectAsyncNode.generated.h"

// Execution pins for the Blueprint node. These are the events that will be fired when the connection attempt succeeds or fails.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnZenohConnectResult);

UCLASS()
class ZENOHBRIDGE_API UZenohAsyncConnect : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Zenoh|Connection")
	static UZenohAsyncConnect* ConnectToZenohAsync(const UObject* WorldContextObject, const FZenohConnectionInfo& ConnectionInfo);

	// Execution pins
	UPROPERTY(BlueprintAssignable)
	FOnZenohConnectResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnZenohConnectResult OnFailed;

	// Called automatically by Unreal when the node executes
	virtual void Activate() override;

private:
	const UObject* WorldContext;
	FZenohConnectionInfo ConnectionData;
};