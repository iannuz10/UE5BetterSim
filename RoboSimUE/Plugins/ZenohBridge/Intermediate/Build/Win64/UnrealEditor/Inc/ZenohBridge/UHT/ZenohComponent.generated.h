// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "ZenohComponent.h"

#ifdef ZENOHBRIDGE_ZenohComponent_generated_h
#error "ZenohComponent.generated.h already included, missing '#pragma once' in ZenohComponent.h"
#endif
#define ZENOHBRIDGE_ZenohComponent_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Delegate FOnZenohMessage *******************************************************
#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_10_DELEGATE \
ZENOHBRIDGE_API void FOnZenohMessage_DelegateWrapper(const FMulticastScriptDelegate& OnZenohMessage, const FString& Message);


// ********** End Delegate FOnZenohMessage *********************************************************

// ********** Begin Class UZenohComponent **********************************************************
#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execPublish);


struct Z_Construct_UClass_UZenohComponent_Statics;
ZENOHBRIDGE_API UClass* Z_Construct_UClass_UZenohComponent_NoRegister();

#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUZenohComponent(); \
	friend struct ::Z_Construct_UClass_UZenohComponent_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend ZENOHBRIDGE_API UClass* ::Z_Construct_UClass_UZenohComponent_NoRegister(); \
public: \
	DECLARE_CLASS2(UZenohComponent, UActorComponent, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/ZenohBridge"), Z_Construct_UClass_UZenohComponent_NoRegister) \
	DECLARE_SERIALIZER(UZenohComponent)


#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	UZenohComponent(UZenohComponent&&) = delete; \
	UZenohComponent(const UZenohComponent&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UZenohComponent); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UZenohComponent); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(UZenohComponent)


#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_12_PROLOG
#define FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_INCLASS_NO_PURE_DECLS \
	FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h_15_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UZenohComponent;

// ********** End Class UZenohComponent ************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
