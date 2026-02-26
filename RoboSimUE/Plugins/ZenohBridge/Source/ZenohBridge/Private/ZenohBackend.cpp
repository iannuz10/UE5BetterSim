#include "ZenohBackend.h"
#include "Containers/StringConv.h"
#include "Async/Async.h"
#include "String"

// --- SAFE MACRO MANAGEMENT ---

// 1. Platform-Specific Headers
#if PLATFORM_WINDOWS
    #include "Windows/AllowWindowsPlatformTypes.h"
#endif

// 2. Disable Warnings (Common for both)
#pragma warning(push)
#pragma warning(disable: 4191) 
#pragma warning(disable: 4005)

// 3. Handle ALIGN conflict
// Unreal defines ALIGN. Zenoh defines ALIGN.
#ifdef ALIGN
    #define UNREAL_ALIGN ALIGN
    #undef ALIGN
#endif

// 4. Include Zenoh
#include "zenoh.h" 

// 5. Restore ALIGN
#ifdef ALIGN
    #undef ALIGN // remove Zenoh's
#endif

#ifdef UNREAL_ALIGN
    #define ALIGN UNREAL_ALIGN
    #undef UNREAL_ALIGN
#endif

#undef ZENOHC_API

// 6. Restore Platform Headers
#if PLATFORM_WINDOWS
    #pragma warning(pop)
    #include "Windows/HideWindowsPlatformTypes.h"
#else
    #pragma warning(pop)
#endif
// ------------------------------

// This struct lives ONLY inside this .cpp file
struct FZenohState
{
    struct z_owned_session_t Session;
    struct z_owned_subscriber_t Subscriber;
    bool bInitialized = false;
};

// --- CALLBACK WRAPPER ---
void zenoh_pimpl_callback(z_loaned_sample_t* sample, void* context)
{
    if (!sample || !context) return;

    // Extract the Topic safely
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &key_string);
    const char* topic_data = z_string_data(z_loan(key_string));
    size_t topic_len = z_string_len(z_loan(key_string));
    FUTF8ToTCHAR TopicConverter(topic_data, (int32)topic_len);
    FString Topic(TopicConverter.Length(), TopicConverter.Get());

    // Extract the Payload safely
    z_owned_string_t payload_string;
    z_bytes_to_string(z_sample_payload(sample), &payload_string);
    const char* data = z_string_data(z_loan(payload_string));
    size_t len = z_string_len(z_loan(payload_string));
    FUTF8ToTCHAR StringConverter(data, (int32)len);
    FString Msg(StringConverter.Length(), StringConverter.Get());

    // Clean up memory
    z_drop(z_move(payload_string));

    // Cast the context directly to our Backend class
    auto* Backend = static_cast<FZenohBackend*>(context);
    if (Backend)
    {
        Backend->MessageQueue.Enqueue(FZenohMessage(Topic, Msg));
    }
}

FZenohBackend::FZenohBackend()
{
    State = new FZenohState();
}

FZenohBackend::~FZenohBackend()
{
    if (State)
    {
        // z_close tells Zenoh to sever the connection and stop all background threads.
        // This guarantees the C-thread will never touch our deleted MessageQueue!
        z_close(z_session_loan_mut(&State->Session), NULL);
        
        delete State;
        State = nullptr;
    }
}

bool FZenohBackend::Initialize(const FString& Mode, const FString& Endpoint)
{
    State = new FZenohState();
    
    // Convert Unreal Strings to standard C strings
    std::string StdMode = std::string(TCHAR_TO_UTF8(*Mode));
    std::string StdEndpoint = std::string(TCHAR_TO_UTF8(*Endpoint));

    // Initialize configuration 
    struct z_owned_config_t config;
    z_config_default(&config);

    // Set Mode (client or peer) using the generic z_loan_mut macro
    zc_config_insert_json5(z_loan_mut(config), "mode", StdMode.c_str());

    // Set Endpoint dynamically
    FString ConfigKey = (Mode == TEXT("client")) ? TEXT("connect/endpoints") : TEXT("listen/endpoints");
    std::string StdConfigKey = std::string(TCHAR_TO_UTF8(*ConfigKey));

    // Format the JSON5 array string for the endpoint: "['tcp/127.0.0.1:7447']"
    std::string JsonEndpoint = "['" + StdEndpoint + "']";

    zc_config_insert_json5(z_loan_mut(config), StdConfigKey.c_str(), JsonEndpoint.c_str());

    // Open the session
    if (z_open(&State->Session, z_move(config), nullptr) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh] Failed to open Zenoh Session."));
        return false;
    }

    return true;
}

void FZenohBackend::Shutdown()
{
    if (State && State->bInitialized)
    {
        // Close Session
        z_close(z_session_loan_mut(&State->Session), NULL);
        z_session_drop((struct z_moved_session_t*)&State->Session);
        State->bInitialized = false;
    }
}

bool FZenohBackend::Publish(const FString& Topic, const FString& Message)
{
    if (!State) return false;

    // Safely convert Unreal FString to UTF-8
    FTCHARToUTF8 StdTopic(*Topic);
    FTCHARToUTF8 StdMsg(*Message);

    // Create a "View" of the Key Expression (Read-only, no allocation/drop needed)
    z_view_keyexpr_t key_expr;
    if (z_view_keyexpr_from_str(&key_expr, StdTopic.Get()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Topic Name (No spaces allowed): '%s'"), *Topic);
        return false;
    }

    // Create an "Owned" Payload (This strictly fixes the Access Violation Crash by ensuring Zenoh manages the memory)
    z_owned_bytes_t payload;
    z_bytes_copy_from_str(&payload, StdMsg.Get());

    // Put data to the network (Passing NULL for options uses the defaults)
    if (z_put(z_loan(State->Session), z_loan(key_expr), z_move(payload), NULL) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Failed to put data on network."));
        return false;
    }

    return true;
}

void zenoh_pimpl_drop(void* context)
{
    // Do nothing. The Backend instance lifecycle is managed by Unreal now.
}

void FZenohBackend::Subscribe(const FString& Topic)
{
    if (!State) return;

    FTCHARToUTF8 TopicUTF(*Topic);
    z_view_keyexpr_t key_expr;
    if (z_view_keyexpr_from_str(&key_expr, TopicUTF.Get()) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Invalid Subscribe Topic: '%s'"), *Topic);
        return;
    }

    // NEW: Instead of passing a HeapCallback, we just pass 'this' (the pointer to our Backend)
    z_owned_closure_sample_t callback;
    z_closure(&callback, zenoh_pimpl_callback, zenoh_pimpl_drop, this);

    if (z_declare_subscriber(z_loan(State->Session), &State->Subscriber, z_loan(key_expr), z_move(callback), NULL) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[Zenoh ERROR] Failed to subscribe to network."));
        z_drop(z_move(callback)); 
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[Zenoh] Successfully subscribed to: %s"), *Topic);
    }
}