#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "UObject/WeakObjectPtr.h"

class FZenohBackend;
class UZenohSubsystem;

/**
 * Background thread for parsing Zenoh messages and publishing ACKs immediately.
 * Decouples network processing from the Game Thread frame rate.
 */
class FZenohWorkerThread : public FRunnable
{
public:
    FZenohWorkerThread(FZenohBackend* InBackend, TWeakObjectPtr<UZenohSubsystem> InSubsystem);
    virtual ~FZenohWorkerThread();

    // FRunnable Interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    /** 
     * Pushes a raw message into the processing queue.
     * Called from the Zenoh C-callback thread.
     */
    void EnqueueMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload);

private:
    /** Struct to hold message data within the queue */
    struct FRawMessage
    {
        FName ConnectionName;
        FString Topic;
        FString Payload;

        FRawMessage() {}
        FRawMessage(const FName& InConnName, const FString& InTopic, const FString& InPayload)
            : ConnectionName(InConnName), Topic(InTopic), Payload(InPayload) {}
    };

    /** Reference to the backend for publishing immediate ACKs */
    FZenohBackend* Backend;

    /** Weak reference to the subsystem for Game Thread dispatch */
    TWeakObjectPtr<UZenohSubsystem> Subsystem;

    /** Thread-safe queue for incoming messages */
    TQueue<FRawMessage, EQueueMode::Mpsc> RawNetworkQueue;

    /** Control flag for the thread loop */
    FThreadSafeBool bStopThread;

    /** Semaphore for efficient thread waiting */
    FEvent* Semaphore;
};
