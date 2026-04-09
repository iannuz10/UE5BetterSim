#include "ZenohMessageRouter.h"
#include "ZenohBackend.h"
#include "ZenohSubsystem.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

FZenohMessageRouter::FZenohMessageRouter(FZenohBackend* InBackend, TWeakObjectPtr<UZenohSubsystem> InSubsystem)
    : Backend(InBackend), Subsystem(InSubsystem), bUseAsyncParsing(false)
{
}

FZenohMessageRouter::~FZenohMessageRouter()
{
}

void FZenohMessageRouter::ParseAndDispatch(TWeakObjectPtr<UZenohSubsystem> WeakSubsystem, const FName& ConnName, const FString& Topic, int64 MsgId, const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Always apply physics on the Game Thread
        AsyncTask(ENamedThreads::GameThread, [WeakSubsystem, ConnName, Topic, MsgId, JsonObject]()
        {
            if (WeakSubsystem.IsValid())
            {
                WeakSubsystem->ProcessParsedPayload(ConnName, Topic, MsgId, JsonObject);
            }
        });
    }
}

void FZenohMessageRouter::RouteMessage(const FName& ConnectionName, const FString& Topic, FString Payload)
{
    int64 MsgIdInt = -1;
    int32 PipeIndex = INDEX_NONE;

    // 1. NON-ALLOCATING HEADER PARSE
    if (Payload.FindChar('|', PipeIndex))
    {
        // FStringView looks at the memory without copying it!
        FStringView FullView(Payload);
        FStringView HeaderView = FullView.Left(PipeIndex);

        int32 ColonIndex;
        if (HeaderView.FindChar(':', ColonIndex))
        {
            FStringView AckTopicView = HeaderView.Left(ColonIndex);
            FStringView MsgIdView = HeaderView.Right(HeaderView.Len() - ColonIndex - 1);

            // Publish the ACK instantly
            FString AckTopic(AckTopicView);
            LexFromString(MsgIdInt, MsgIdView); // Fast macro to parse ints from Views
            
            if (Backend) Backend->Publish(AckTopic, FString::Printf(TEXT("%lld"), MsgIdInt));
        }
    }

    // 2. THE SMART SWITCH (Deferred Allocation)
    if (bUseAsyncParsing)
    {
        // TRUE ZERO-COPY HANDOFF
        // We move the entire massive string into the lambda. No memory is copied here.
        Async(EAsyncExecution::ThreadPool, [WeakSubsystem = Subsystem, ConnName = ConnectionName, Topic, MsgIdInt, PipeIndex, RawPayload = MoveTemp(Payload)]()
        {
            // The heap allocation for Mid() happens safely on the background core
            FString CleanPayload = (PipeIndex != INDEX_NONE) ? RawPayload.Mid(PipeIndex + 1) : RawPayload;
            ParseAndDispatch(WeakSubsystem, ConnName, Topic, MsgIdInt, CleanPayload);
        });
    }
    else
    {
        // TINY SCENES (Direct parse)
        FString CleanPayload = (PipeIndex != INDEX_NONE) ? Payload.Mid(PipeIndex + 1) : Payload;
        ParseAndDispatch(Subsystem, ConnectionName, Topic, MsgIdInt, CleanPayload);
    }
}