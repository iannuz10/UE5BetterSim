#pragma once

#include "CoreMinimal.h"

// Forward declaration
struct FZenohState;
class ZenohMessageRouter;
class UZenohSubsystem;

class FZenohBackend
{
public:
    FZenohBackend();
    ~FZenohBackend();

    bool Initialize(const FString& Mode, const FString& Endpoint, UZenohSubsystem* InSubsystem);
    void Shutdown();

    bool Publish(const FString& Topic, const FString& Message);
    void Subscribe(const FString& Topic);

    /** Direct pointer to the Router */
    class FZenohMessageRouter* MessageRouter = nullptr;

    /** The identifier for this connection, used for routing messages back to the subsystem */
    FName ConnectionName;

private:
    /** This pointer hides ALL Zenoh data from Unreal */
    FZenohState* State = nullptr;

};
