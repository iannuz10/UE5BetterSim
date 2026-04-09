#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZenohSubsystem.generated.h"

// Forward declarations
class ZenohMessageRouter;
class FZenohBackend;

// Enum configurations
UENUM(BlueprintType)
enum class EZenohProtocol : uint8
{
    TCP UMETA(DisplayName = "TCP (Reliable, Localhost)"),
    UDP UMETA(DisplayName = "UDP (Fast, Networked)")
};

UENUM(BlueprintType)
enum class EZenohMode : uint8
{
    Client UMETA(DisplayName = "Client (Connects to Router/Peer)"),
    Peer UMETA(DisplayName = "Peer (Direct Mesh Network)")
};

USTRUCT(BlueprintType)
struct FZenohConnectionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    FName ConnectionName = TEXT("DefaultConnection");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    EZenohMode ConnectionMode = EZenohMode::Client;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    EZenohProtocol Protocol = EZenohProtocol::TCP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    FString IPAddress = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zenoh|Connection")
    int32 Port = 7447;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnZenohTopicParsed, TSharedPtr<FJsonObject>);

UCLASS()
class ZENOHBRIDGE_API UZenohTopicListener : public UObject
{
    GENERATED_BODY()
public:
    FOnZenohTopicParsed OnTopicParsed;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnZenohGlobalMessage, const FName&, ConnectionName, const FString&, Topic, const FString&, Message);

UCLASS()
class ZENOHBRIDGE_API UZenohSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ==========================================
    // SUBSYSTEM LIFECYCLE
    // ==========================================
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ==========================================
    // LIFECYCLE CONTROL
    // ==========================================

    // Pass a ConnectionInfo struct
    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    bool Connect(const FZenohConnectionInfo& ConnectionInfo);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    void Disconnect(FName ConnectionName);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Connection")
    void DisconnectAll();

    UFUNCTION(BlueprintPure, Category = "Zenoh|Connection")
    bool IsConnected(FName ConnectionName) const;

    // ==========================================
    // MESSAGING
    // ==========================================

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
    UZenohTopicListener* SubscribeToTopic(FName ConnectionName, FString Topic);

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Messaging")
    bool Publish(FName ConnectionName, FString Topic, FString Message);

    UPROPERTY(BlueprintAssignable, Category = "Zenoh|Messaging")
    FOnZenohGlobalMessage OnGlobalMessageReceived;

    UFUNCTION(BlueprintCallable, Category = "Zenoh|Performance")
    void SetUseAsyncParsing(bool bEnableAsync);

    /** 
     * Called from the background WorkerThread via AsyncTask to process pre-parsed payloads on the Game Thread.
     */
    void ProcessParsedPayload(const FName& ConnectionName, const FString& Topic, int64 MsgId, TSharedPtr<FJsonObject> ParsedJson);

private:
    // Multi-connection dictionary
    // Maps a name (ex. "Docker") to its specific C++ backend instance.
    TMap<FName, FZenohBackend*> ActiveConnections;

    // A Mutex Lock to prevent Background Threads and GameThreads from colliding
    mutable FCriticalSection ConnectionMapLock;

    // Dictionary of Delegates for specific topics.
    UPROPERTY()
    TMap<FString, UZenohTopicListener*> TopicListeners;

    // Stores subscriptions requested BEFORE a connection is fully established (The Race Condition Fix)
    TMap<FName, TArray<FString>> PendingSubscriptions;

    // Tracker map for stale messages
    TMap<FString, int64> LastMsgIdPerTopic;

private:
    // Tracks the current performance mode so new connections inherit it
    bool bEnableAsyncParsing = false;
};
