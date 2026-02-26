#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ZenohSubsystem.h"
#include "ZenohConnectAsyncNode.generated.h"

// The delegates that will become the execution pins in Blueprints
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnZenohConnectResult);

UCLASS()
class ZENOHBRIDGE_API UZenohAsyncConnect : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// This is the function you will search for in the Blueprint menu
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Zenoh|Connection")
	static UZenohAsyncConnect* ConnectToZenohAsync(const UObject* WorldContextObject, const FZenohConnectionInfo& ConnectionInfo);

	// The execution pins!
	UPROPERTY(BlueprintAssignable)
	FOnZenohConnectResult OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnZenohConnectResult OnFailed;

	// This is called automatically by Unreal when the node executes
	virtual void Activate() override;

private:
	const UObject* WorldContext;
	FZenohConnectionInfo ConnectionData;
};