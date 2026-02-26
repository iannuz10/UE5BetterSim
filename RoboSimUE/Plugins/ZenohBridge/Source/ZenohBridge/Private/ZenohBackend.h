#pragma once

#include "Containers/Queue.h"
#include "CoreMinimal.h"

// Forward declaration allows hiding the struct
struct FZenohState;

struct FZenohMessage
{
	FString Topic;
	FString Payload;

	// Default constructor
	FZenohMessage() {}
    
	// Helper constructor to quickly create a message
	FZenohMessage(const FString& InTopic, const FString& InPayload) 
		: Topic(InTopic), Payload(InPayload) {}
};

class FZenohBackend
{
public:
	FZenohBackend();
	~FZenohBackend();

	bool Initialize(const FString& Mode, const FString& Endpoint);
	void Shutdown();
	
	bool Publish(const FString& Topic, const FString& Message);
	void Subscribe(const FString& Topic);
	
	// Mpsc = Multiple Producers (Zenoh network threads), Single Consumer (Unreal GameThread)
	TQueue<FZenohMessage, EQueueMode::Mpsc> MessageQueue;
	
private:
	// This pointer hides ALL Zenoh data from Unreal
	FZenohState* State = nullptr;
};