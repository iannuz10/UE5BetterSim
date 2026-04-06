#pragma once

#include "CoreMinimal.h"

// Forward declaration
struct FZenohState;
class FZenohWorkerThread;

class FZenohBackend
{
public:
    FZenohBackend();
    ~FZenohBackend();

    bool Initialize(const FString& Mode, const FString& Endpoint);
    void Shutdown();

    bool Publish(const FString& Topic, const FString& Message);
    void Subscribe(const FString& Topic);

    /** Sets the background worker thread for asynchronous message processing */
    void SetWorkerThread(FZenohWorkerThread* InWorkerThread) { WorkerThread = InWorkerThread; }

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
};
