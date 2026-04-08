#include "ZenohSubsystem.h"
#include "ZenohWorkerThread.h"
#include "HAL/RunnableThread.h"

// ==========================================
// SUBSYSTEM LIFECYCLE 
// ==========================================
void UZenohSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Initialized. Ready for connections."));
}

void UZenohSubsystem::Deinitialize()
{
    // Cleanly shut down all background network threads when the game closes
    DisconnectAll();
    
    TopicListeners.Empty();
    Super::Deinitialize();
    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Deinitialized. All connections closed."));
}

// ==========================================
// LIFECYCLE CONTROL
// ==========================================
bool UZenohSubsystem::Connect(const FZenohConnectionInfo& ConnectionInfo)
{
    // Clean up old connections safely
    Disconnect(ConnectionInfo.ConnectionName);

    // Prepare the connection strings
    FString ProtocolStr = (ConnectionInfo.Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *ConnectionInfo.IPAddress, ConnectionInfo.Port);
    FString ModeStr = (ConnectionInfo.ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");    

    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] [%s] Connecting as %s to %s..."), *ConnectionInfo.ConnectionName.ToString(), *ModeStr, *Endpoint);

    TUniquePtr<FZenohBackend> NewBackend = MakeUnique<FZenohBackend>();
    NewBackend->ConnectionName = ConnectionInfo.ConnectionName;

    if (NewBackend->Initialize(ModeStr, Endpoint, this))
    {
        FScopeLock Lock(&ConnectionMapLock);
        ActiveConnections.Emplace(ConnectionInfo.ConnectionName, MoveTemp(NewBackend));

        // The connection is now online. Flush any deferred subscriptions.
        if (TArray<FString>* PendingTopics = PendingSubscriptions.Find(ConnectionInfo.ConnectionName))
        {
            for (const FString& PendingTopic : *PendingTopics)
            {
                ActiveConnections[ConnectionInfo.ConnectionName]->Subscribe(PendingTopic);
            }
            PendingSubscriptions.Remove(ConnectionInfo.ConnectionName);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void UZenohSubsystem::Disconnect(FName ConnectionName)
{
    FScopeLock Lock(&ConnectionMapLock);

    if (TUniquePtr<FZenohBackend>* BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (BackendPtr->IsValid())
        {
            (*BackendPtr)->Shutdown();
        }
        ActiveConnections.Remove(ConnectionName);
        UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Disconnected '%s'."), *ConnectionName.ToString());
    }
}

void UZenohSubsystem::DisconnectAll()
{
    FScopeLock Lock(&ConnectionMapLock);

    for (auto& Pair : ActiveConnections)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->Shutdown();
        }
    }
    ActiveConnections.Empty();
}

bool UZenohSubsystem::IsConnected(FName ConnectionName) const
{
    FScopeLock Lock(&ConnectionMapLock);
    return ActiveConnections.Contains(ConnectionName);
}

// ==========================================
// MESSAGING
// ==========================================

UZenohTopicListener* UZenohSubsystem::SubscribeToTopic(FName ConnectionName, FString Topic)
{
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    UZenohTopicListener* Listener = TopicListeners.FindRef(RoutingKey);
    if (!Listener)
    {
        Listener = NewObject<UZenohTopicListener>(this);
        TopicListeners.Add(RoutingKey, Listener);
    }

    FScopeLock Lock(&ConnectionMapLock);
    if (TUniquePtr<FZenohBackend>* BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (BackendPtr->IsValid())
        {
            (*BackendPtr)->Subscribe(Topic);
        }
    }
    else
    {
        PendingSubscriptions.FindOrAdd(ConnectionName).AddUnique(Topic);
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Connection '%s' not ready. Deferring subscription to '%s'."), *ConnectionName.ToString(), *Topic);
    }

    return Listener;
}

bool UZenohSubsystem::Publish(FName ConnectionName, FString Topic, FString Message)
{
    FScopeLock Lock(&ConnectionMapLock);
    if (TUniquePtr<FZenohBackend>* BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (BackendPtr->IsValid())
        {
            return (*BackendPtr)->Publish(Topic, Message);
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[ZenohSubsystem] Ignored Publish: Connection '%s' not found!"), *ConnectionName.ToString());
    return false;
}

void UZenohSubsystem::ProcessCleanPayload(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    // This is now called via AsyncTask on the Game Thread
    HandleZenohMessage(ConnectionName, Topic, Payload);
}

void UZenohSubsystem::HandleZenohMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    // GLOBAL FIREHOSE
    if (OnGlobalMessageReceived.IsBound())
    {
        OnGlobalMessageReceived.Broadcast(ConnectionName, Topic, Payload);
    }

    // SPECIFIC ROUTING
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    if (UZenohTopicListener** ListenerPtr = TopicListeners.Find(RoutingKey))
    {
        if (*ListenerPtr)
        {
            (*ListenerPtr)->OnMessageReceived.Broadcast(Payload);
        }
    }
}
