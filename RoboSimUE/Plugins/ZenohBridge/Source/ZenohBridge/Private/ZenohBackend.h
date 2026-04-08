#pragma once

#include "CoreMinimal.h"

// Forward declaration
struct FZenohState;
class FZenohWorkerThread;
class FRunnableThread;
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

    /** 
     * Internal pointer to the worker thread. 
     * Public so the C-callback can access it directly for speed.
     */
    FZenohWorkerThread* WorkerThread = nullptr;

    /** The identifier for this connection, used for routing messages back to the subsystem */
    FName ConnectionName;

private:
    /** This pointer hides ALL Zenoh data from Unreal */
    FZenohState* State = nullptr;

    /** Thread lifecycle manager */
    FRunnableThread* WorkerThreadHandle = nullptr;
};
