#include "ZenohWorkerThread.h"
#include "ZenohBackend.h"
#include "ZenohSubsystem.h"
#include "Async/Async.h"

FZenohWorkerThread::FZenohWorkerThread(FZenohBackend* InBackend, TWeakObjectPtr<UZenohSubsystem> InSubsystem)
    : Backend(InBackend)
    , Subsystem(InSubsystem)
    , bStopThread(false)
{
    Semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);
}

FZenohWorkerThread::~FZenohWorkerThread()
{
    if (Semaphore)
    {
        FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
        Semaphore = nullptr;
    }
}

bool FZenohWorkerThread::Init()
{
    return (Backend != nullptr && Semaphore != nullptr);
}

uint32 FZenohWorkerThread::Run()
{
    while (!bStopThread)
    {
        // Wait for messages or stop signal
        Semaphore->Wait(FTimespan::FromMilliseconds(100));

        if (bStopThread) break;

        FRawMessage Incoming;
        while (RawNetworkQueue.Dequeue(Incoming))
        {
            const FName& ConnName = Incoming.ConnectionName;
            const FString& Topic = Incoming.Topic;
            const FString& Payload = Incoming.Payload;

            FString Header;
            FString CleanPayload;

            // Envelope parsing: [AckTopic:MsgId]|[JsonPayload]
            if (Payload.Split(TEXT("|"), &Header, &CleanPayload))
            {
                FString AckTopic;
                FString MsgId;

                // Sub-split Header: [AckTopic]:[MsgId]
                if (Header.Split(TEXT(":"), &AckTopic, &MsgId))
                {
                    // Immediate ACK on the background thread using the provided topic
                    if (Backend)
                    {
                        Backend->Publish(AckTopic, MsgId);
                    }
                }
                else
                {
                    // Fallback for messages with '|' but no ':'
                    // Header might be just the MsgId
                    if (Backend)
                    {
                        Backend->Publish(TEXT("sim/latency/ack"), Header);
                    }
                }
            }
            else
            {
                CleanPayload = Payload;
            }

            // Dispatch to Game Thread
            AsyncTask(ENamedThreads::GameThread, [WeakSubsystem = Subsystem, ConnName, Topic, CleanPayload]()
            {
                if (WeakSubsystem.IsValid())
                {
                    WeakSubsystem->ProcessCleanPayload(ConnName, Topic, CleanPayload);
                }
            });
        }
    }

    return 0;
}

void FZenohWorkerThread::Stop()
{
    bStopThread = true;
    if (Semaphore)
    {
        Semaphore->Trigger();
    }
}

void FZenohWorkerThread::Exit()
{
}

void FZenohWorkerThread::EnqueueMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    RawNetworkQueue.Enqueue(FRawMessage(ConnectionName, Topic, Payload));
    if (Semaphore)
    {
        Semaphore->Trigger();
    }
}
