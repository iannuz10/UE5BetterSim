#include "ZenohWorkerThread.h"
#include "ZenohBackend.h"
#include "ZenohSubsystem.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

FZenohWorkerThread::FZenohWorkerThread(FZenohBackend* InBackend, TWeakObjectPtr<UZenohSubsystem> InSubsystem)
    : Backend(InBackend), Subsystem(InSubsystem), bStopThread(false)
{
}

FZenohWorkerThread::~FZenohWorkerThread()
{
}

bool FZenohWorkerThread::Init()
{
    return (Backend != nullptr);
}

uint32 FZenohWorkerThread::Run()
{
    // We leave this alive just so the FRunnable doesn't crash, 
    // but we let it sleep peacefully. It no longer handles the data.
    while (!bStopThread)
    {
        FPlatformProcess::Sleep(0.1f); 
    }
    return 0;
}

void FZenohWorkerThread::Stop()
{
    bStopThread = true;
}

void FZenohWorkerThread::Exit()
{
}

// ---------------------------------------------------------
// HELPER FUNCTION 
// ---------------------------------------------------------
void FZenohWorkerThread::ParseAndDispatch(TWeakObjectPtr<UZenohSubsystem> WeakSubsystem, const FName& ConnName, const FString& Topic, int64 MsgId, const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, ConnName, Topic, MsgId, JsonObject]()
        {
            if (WeakSubsystem.IsValid())
            {
                WeakSubsystem->ProcessParsedPayload(ConnName, Topic, MsgId, JsonObject);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[Router] FATAL: WeakSubsystem is INVALID on the Game Thread!"));
            }
        });
    }
    else
    {
        // TRACER DYE: If it fails here, your JSON is malformed.
        UE_LOG(LogTemp, Error, TEXT("[Router] JSON Deserialization FAILED for Topic: %s"), *Topic);
        UE_LOG(LogTemp, Warning, TEXT("[Router] Raw Payload Dump: %s"), *JsonString.Left(200)); // Print first 200 chars
    }
}

// ---------------------------------------------------------
// THE SMART ROUTER (Your new EnqueueMessage)
// ---------------------------------------------------------
void FZenohWorkerThread::EnqueueMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    int64 MsgIdInt = -1;
    int32 SplitIndex = INDEX_NONE;

    // INSTANT ACK (Always fast, regardless of path)
    if (Payload.FindChar('|', SplitIndex))
    {
        FString Header = Payload.Left(SplitIndex);
        FString AckTopic, MsgIdStr;

        if (Header.Split(TEXT(":"), &AckTopic, &MsgIdStr))
        {
            if (Backend) Backend->Publish(AckTopic, MsgIdStr);
            MsgIdInt = FCString::Atoi64(*MsgIdStr);
        }
    }

    FString CleanPayload = (SplitIndex != INDEX_NONE) ? Payload.Mid(SplitIndex + 1) : Payload;

    // SMART SWITCH
    if (bUseAsyncParsing)
    {
        // MASSIVE SCENES (10+ Robots (or topics))
        // Throws the work to the background cores to prevent network blocking
        Async(EAsyncExecution::ThreadPool, [WeakSubsystem = Subsystem, ConnName = ConnectionName, Topic, MsgIdInt, CleanPayload]()
        {
            ParseAndDispatch(WeakSubsystem, ConnName, Topic, MsgIdInt, CleanPayload);
        });
    }
    else
    {
        // TINY SCENES (1-4 Robots)
        // Parses immediately on the network thread for < 2ms latency
        ParseAndDispatch(Subsystem, ConnectionName, Topic, MsgIdInt, CleanPayload);
    }
}
