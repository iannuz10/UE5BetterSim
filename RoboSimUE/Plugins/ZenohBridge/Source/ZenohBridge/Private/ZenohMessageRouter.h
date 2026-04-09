#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class FZenohBackend;
class UZenohSubsystem;

/**
 * Lightweight router that lives directly on Zenoh's C-callback thread.
 * Achieves True Zero-Copy handoff using MoveTemp and FStringView.
 */
class FZenohMessageRouter
{
public:
	FZenohMessageRouter(FZenohBackend* InBackend, TWeakObjectPtr<UZenohSubsystem> InSubsystem);
	~FZenohMessageRouter();

	/** * Note: Payload is passed by VALUE here. 
	 * This is required so we can steal its memory later using MoveTemp.
	 */
	void RouteMessage(const FName& ConnectionName, const FString& Topic, FString Payload);
    
	void SetAsyncParsing(bool bEnable) { bUseAsyncParsing = bEnable; }

private:
	static void ParseAndDispatch(TWeakObjectPtr<UZenohSubsystem> WeakSubsystem, const FName& ConnName, const FString& Topic, int64 MsgId, const FString& JsonString);

	FZenohBackend* Backend;
	TWeakObjectPtr<UZenohSubsystem> Subsystem;
    
	bool bUseAsyncParsing = false;
};