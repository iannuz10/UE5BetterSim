#pragma once

#include "CoreMinimal.h"

// Forward declaration allows hiding the struct
struct FZenohState;

class FZenohBackend
{
public:
	FZenohBackend();
	~FZenohBackend();

	bool Initialize();
	void Shutdown();
	bool Publish(const FString& Topic, const FString& Message);
    
	// Pass a lambda/function pointer for callbacks
	typedef TFunction<void(const FString&)> FOnMessageCallback;
	void Subscribe(const FString& Topic, FOnMessageCallback Callback);

private:
	// This pointer hides ALL Zenoh data from Unreal
	FZenohState* State = nullptr;
};