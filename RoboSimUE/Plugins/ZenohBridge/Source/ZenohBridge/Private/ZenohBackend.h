#pragma once

#include "CoreMinimal.h"

// Forward declaration allows us to hide the struct
struct FZenohState;

class FZenohBackend
{
public:
	FZenohBackend();
	~FZenohBackend();

	bool Initialize();
	void Shutdown();
	bool Publish(const FString& Topic, const FString& Message);
    
	// We will pass a lambda/function pointer for callbacks
	typedef TFunction<void(const FString&)> FOnMessageCallback;
	void Subscribe(const FString& Topic, FOnMessageCallback Callback);

private:
	// This opaque pointer hides ALL Zenoh data from Unreal
	FZenohState* State = nullptr;
};