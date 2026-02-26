#include "ZenohComponent.h"
#include "ZenohBackend.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

// --- CONSTRUCTOR ---
UZenohComponent::UZenohComponent()
{
    // FIX: Make sure this is set to TRUE so our TickComponent function actually runs!
    PrimaryComponentTick.bCanEverTick = true;
}

// --- DESTRUCTOR ---
UZenohComponent::~UZenohComponent()
{
    if (Backend)
    {
        delete Backend;
        Backend = nullptr;
    }
}

// --- BEGIN PLAY ---
void UZenohComponent::BeginPlay()
{
    Super::BeginPlay();

    // Lazy Allocation: Create the backend only when the game actually starts
    if (Backend == nullptr)
    {
        Backend = new FZenohBackend();
    }

    if (bAutoConnectOnBeginPlay)
    {
        Connect();
    }
}

// --- END PLAY ---
void UZenohComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clean shutdown of Zenoh resources
    Disconnect();
    if (Backend)
    {
        delete Backend;
        Backend = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

bool UZenohComponent::Connect()
{
    if (!Backend) return false;

    // 1. Format the Protocol String (tcp or udp)
    FString ProtocolStr = (Protocol == EZenohProtocol::TCP) ? TEXT("tcp") : TEXT("udp");

    // 2. Format the Endpoint String (e.g., "tcp/host.docker.internal:7447")
    FString Endpoint = FString::Printf(TEXT("%s/%s:%d"), *ProtocolStr, *IPAddress, Port);

    // 3. Format the Mode String
    FString ModeStr = (ConnectionMode == EZenohMode::Client) ? TEXT("client") : TEXT("peer");

    UE_LOG(LogTemp, Log, TEXT("[Zenoh] Connecting as %s to %s..."), *ModeStr, *Endpoint);

    // 4. Pass the dynamic settings to the backend
    bool bSuccess = Backend->Initialize(ModeStr, Endpoint);

    if (bSuccess)
    {
        bIsConnected = true; // Track successful connection
        if (!DefaultTopic.IsEmpty())
        {
            Subscribe(DefaultTopic);
        }
    }

    return bSuccess;
}

void UZenohComponent::Disconnect()
{
    if (Backend)
    {
        Backend->Shutdown();
        bIsConnected = false; // Reset tracking state
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Disconnected."));
    }
}

// --- TICK ---
void UZenohComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (Backend)
    {
        FZenohMessage IncomingMessage;
        
        // Dequeue removes the oldest message from the queue and puts it into 'IncomingMessage'.
        // The 'while' loop ensures that if 5 messages arrived this frame, we process all 5!
        while (Backend->MessageQueue.Dequeue(IncomingMessage))
        {
            // Hand the safely extracted strings over to the Blueprint system!
            HandleZenohMessage(IncomingMessage.Topic, IncomingMessage.Payload);
        }
    }
}

// --- PUBLISH ---
bool UZenohComponent::Publish(FString Topic, FString Message)
{
    if (!Backend || !bIsConnected) 
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Ignored Publish: Not connected to Zenoh. Call Connect() first!"));
        return false;
    }
    
    return Backend->Publish(Topic, Message);
}

// --- SUBSCRIBE ---
void UZenohComponent::Subscribe(FString NewTopic)
{
    if (!Backend || !bIsConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Zenoh] Ignored Subscribe: Not connected to Zenoh."));
        return;
    }
    
    Backend->Subscribe(NewTopic);
}

void UZenohComponent::HandleZenohMessage(const FString& Topic, const FString& Payload)
{
    // The component's ONLY job is to take the bytes from the network
    // and hand them to the Blueprint system. Nothing else.
    if (OnMessageReceived.IsBound())
    {
        OnMessageReceived.Broadcast(Topic, Payload);
    }
}