#include "ZenohSubsystem.h"

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
// FTICKABLE GAMEOBJECT INTERFACE
// ==========================================
void UZenohSubsystem::Tick(float DeltaTime)
{
    // Lock the map. If the background thread is currently adding a connection, 
    // the GameThread will wait for a microsecond
    FScopeLock Lock(&ConnectionMapLock);

    for (auto& Pair : ActiveConnections)
    {
        FName ConnName = Pair.Key;
        FZenohBackend* Backend = Pair.Value;

        if (Backend)
        {
            FZenohMessage IncomingMessage;
            while (Backend->MessageQueue.Dequeue(IncomingMessage))
            {
                HandleZenohMessage(ConnName, IncomingMessage.Topic, IncomingMessage.Payload);
            }
        }
    }
}

TStatId UZenohSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UZenohSubsystem, STATGROUP_Tickables);
}

// ==========================================
// LIFECYCLE CONTROL
// ==========================================
bool UZenohSubsystem::Connect(const FZenohConnectionInfo& ConnectionInfo)
{
    // Clean up old connections safely (Disconnect already has its own lock inside it!)
    Disconnect(ConnectionInfo.ConnectionName);

    // Prepare the connection strings
    FString ProtocolStr = (ConnectionInfo.Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *ConnectionInfo.IPAddress, ConnectionInfo.Port);
    FString ModeStr = (ConnectionInfo.ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");

    UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] [%s] Connecting as %s to %s..."), *ConnectionInfo.ConnectionName.ToString(), *ModeStr, *Endpoint);
    
    // The background thread does the DNS and TCP handshake freely 
    // Because no lock is held, the GameThread's Tick() can continue running and processing messages from existing connections without waiting for this potentially slow operation
    FZenohBackend* NewBackend = new FZenohBackend();
    
    if (NewBackend->Initialize(ModeStr, Endpoint))
    {
        // SUCCESS! Now we lock the map for 1 microsecond just to add the connection.
        FScopeLock Lock(&ConnectionMapLock);
        ActiveConnections.Add(ConnectionInfo.ConnectionName, NewBackend);

        // The connection is now online. Flush any deferred subscriptions.
        if (TArray<FString>* PendingTopics = PendingSubscriptions.Find(ConnectionInfo.ConnectionName))
        {
            for (const FString& PendingTopic : *PendingTopics)
            {
                NewBackend->Subscribe(PendingTopic);
            }
            PendingSubscriptions.Remove(ConnectionInfo.ConnectionName);
        }
        
        return true;
    }
    else
    {
        delete NewBackend;
        return false;
    }
}

void UZenohSubsystem::Disconnect(FName ConnectionName)
{
    // Lock the map. Protect it while the background thread modifies it.
    FScopeLock Lock(&ConnectionMapLock);

    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            (*BackendPtr)->Shutdown();
            delete *BackendPtr; 
        }
        
        ActiveConnections.Remove(ConnectionName);
        UE_LOG(LogTemp, Log, TEXT("[ZenohSubsystem] Disconnected '%s'."), *ConnectionName.ToString());
    }
}

void UZenohSubsystem::DisconnectAll()
{
    // Lock the map.
    FScopeLock Lock(&ConnectionMapLock);

    for (auto& Pair : ActiveConnections)
    {
        if (Pair.Value)
        {
            Pair.Value->Shutdown();
            delete Pair.Value;
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
    // Generate a unique key for this specific route (e.g., "Docker::sim/state")
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    // 2. If a listener object doesn't exist yet, create it instantly so BP can bind to it.
    UZenohTopicListener* Listener = TopicListeners.FindRef(RoutingKey);
    if (!Listener)
    {
        Listener = NewObject<UZenohTopicListener>(this);
        TopicListeners.Add(RoutingKey, Listener);
    }
    
    // Tell the background thread to listen to the network
    FScopeLock Lock(&ConnectionMapLock);
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            (*BackendPtr)->Subscribe(Topic);
        }
    }
    else
    {
        // The connection isn't ready yet. Store it in our back pocket.
        PendingSubscriptions.FindOrAdd(ConnectionName).AddUnique(Topic);
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Connection '%s' not ready. Deferring subscription to '%s'."), *ConnectionName.ToString(), *Topic);
    }
    
    // If a listener object already exists for this topic, just hand it back!
    if (TopicListeners.Contains(RoutingKey))
    {
        return TopicListeners[RoutingKey];
    }
    
    return Listener;
}

bool UZenohSubsystem::Publish(FName ConnectionName, FString Topic, FString Message)
{
    FScopeLock Lock(&ConnectionMapLock);
    if (FZenohBackend** BackendPtr = ActiveConnections.Find(ConnectionName))
    {
        if (*BackendPtr)
        {
            return (*BackendPtr)->Publish(Topic, Message);
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("[ZenohSubsystem] Ignored Publish: Connection '%s' not found!"), *ConnectionName.ToString());
    return false;
}

void UZenohSubsystem::HandleZenohMessage(const FName& ConnectionName, const FString& Topic, const FString& Payload)
{
    // GLOBAL FIREHOSE: Broadcast to anyone listening to everything (e.g., Debug UI)
    if (OnGlobalMessageReceived.IsBound())
    {
        OnGlobalMessageReceived.Broadcast(ConnectionName, Topic, Payload);
    }

    // SPECIFIC ROUTING: Reconstruct the Routing Key
    FString RoutingKey = ConnectionName.ToString() + TEXT("::") + Topic;

    // High-speed O(1) Dictionary Lookup
    if (UZenohTopicListener** ListenerPtr = TopicListeners.Find(RoutingKey))
    {
        if (*ListenerPtr)
        {
            // Only wake up the specific Blueprint that asked for this exact topic!
            (*ListenerPtr)->OnMessageReceived.Broadcast(Payload);
        }
    }
}