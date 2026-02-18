// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "ZenohComponent.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeZenohComponent() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UActorComponent();
UPackage* Z_Construct_UPackage__Script_ZenohBridge();
ZENOHBRIDGE_API UClass* Z_Construct_UClass_UZenohComponent();
ZENOHBRIDGE_API UClass* Z_Construct_UClass_UZenohComponent_NoRegister();
ZENOHBRIDGE_API UFunction* Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature();
// ********** End Cross Module References **********************************************************

// ********** Begin Delegate FOnZenohMessage *******************************************************
struct Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics
{
	struct _Script_ZenohBridge_eventOnZenohMessage_Parms
	{
		FString Message;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/ZenohComponent.h" },
	};
#endif // WITH_METADATA

// ********** Begin Delegate FOnZenohMessage constinit property declarations ***********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_Message;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Delegate FOnZenohMessage constinit property declarations *************************
	static const UECodeGen_Private::FDelegateFunctionParams FuncParams;
};

// ********** Begin Delegate FOnZenohMessage Property Definitions **********************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::NewProp_Message = { "Message", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(_Script_ZenohBridge_eventOnZenohMessage_Parms, Message), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::NewProp_Message,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::PropPointers) < 2048);
// ********** End Delegate FOnZenohMessage Property Definitions ************************************
const UECodeGen_Private::FDelegateFunctionParams Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UPackage__Script_ZenohBridge, nullptr, "OnZenohMessage__DelegateSignature", 	Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::PropPointers), 
sizeof(Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::_Script_ZenohBridge_eventOnZenohMessage_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00130000, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::Function_MetaDataParams), Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::_Script_ZenohBridge_eventOnZenohMessage_Parms) < MAX_uint16);
UFunction* Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUDelegateFunction(&ReturnFunction, Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature_Statics::FuncParams);
	}
	return ReturnFunction;
}
void FOnZenohMessage_DelegateWrapper(const FMulticastScriptDelegate& OnZenohMessage, const FString& Message)
{
	struct _Script_ZenohBridge_eventOnZenohMessage_Parms
	{
		FString Message;
	};
	_Script_ZenohBridge_eventOnZenohMessage_Parms Parms;
	Parms.Message=Message;
	OnZenohMessage.ProcessMulticastDelegate<UObject>(&Parms);
}
// ********** End Delegate FOnZenohMessage *********************************************************

// ********** Begin Class UZenohComponent Function Publish *****************************************
struct Z_Construct_UFunction_UZenohComponent_Publish_Statics
{
	struct ZenohComponent_eventPublish_Parms
	{
		FString Topic;
		FString Message;
		bool ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Zenoh" },
		{ "ModuleRelativePath", "Public/ZenohComponent.h" },
	};
#endif // WITH_METADATA

// ********** Begin Function Publish constinit property declarations *******************************
	static const UECodeGen_Private::FStrPropertyParams NewProp_Topic;
	static const UECodeGen_Private::FStrPropertyParams NewProp_Message;
	static void NewProp_ReturnValue_SetBit(void* Obj);
	static const UECodeGen_Private::FBoolPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function Publish constinit property declarations *********************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function Publish Property Definitions ******************************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_Topic = { "Topic", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ZenohComponent_eventPublish_Parms, Topic), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_Message = { "Message", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ZenohComponent_eventPublish_Parms, Message), METADATA_PARAMS(0, nullptr) };
void Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_ReturnValue_SetBit(void* Obj)
{
	((ZenohComponent_eventPublish_Parms*)Obj)->ReturnValue = 1;
}
const UECodeGen_Private::FBoolPropertyParams Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Bool | UECodeGen_Private::EPropertyGenFlags::NativeBool, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, sizeof(bool), sizeof(ZenohComponent_eventPublish_Parms), &Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_ReturnValue_SetBit, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UZenohComponent_Publish_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_Topic,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_Message,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UZenohComponent_Publish_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UZenohComponent_Publish_Statics::PropPointers) < 2048);
// ********** End Function Publish Property Definitions ********************************************
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UZenohComponent_Publish_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UZenohComponent, nullptr, "Publish", 	Z_Construct_UFunction_UZenohComponent_Publish_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UFunction_UZenohComponent_Publish_Statics::PropPointers), 
sizeof(Z_Construct_UFunction_UZenohComponent_Publish_Statics::ZenohComponent_eventPublish_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UZenohComponent_Publish_Statics::Function_MetaDataParams), Z_Construct_UFunction_UZenohComponent_Publish_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UZenohComponent_Publish_Statics::ZenohComponent_eventPublish_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UZenohComponent_Publish()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UZenohComponent_Publish_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UZenohComponent::execPublish)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_Topic);
	P_GET_PROPERTY(FStrProperty,Z_Param_Message);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(bool*)Z_Param__Result=P_THIS->Publish(Z_Param_Topic,Z_Param_Message);
	P_NATIVE_END;
}
// ********** End Class UZenohComponent Function Publish *******************************************

// ********** Begin Class UZenohComponent **********************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UZenohComponent;
UClass* UZenohComponent::GetPrivateStaticClass()
{
	using TClass = UZenohComponent;
	if (!Z_Registration_Info_UClass_UZenohComponent.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("ZenohComponent"),
			Z_Registration_Info_UClass_UZenohComponent.InnerSingleton,
			StaticRegisterNativesUZenohComponent,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UZenohComponent.InnerSingleton;
}
UClass* Z_Construct_UClass_UZenohComponent_NoRegister()
{
	return UZenohComponent::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UZenohComponent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintSpawnableComponent", "" },
		{ "ClassGroupNames", "Custom" },
		{ "IncludePath", "ZenohComponent.h" },
		{ "ModuleRelativePath", "Public/ZenohComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OnMessageReceived_MetaData[] = {
		{ "Category", "Zenoh" },
		{ "ModuleRelativePath", "Public/ZenohComponent.h" },
	};
#endif // WITH_METADATA

// ********** Begin Class UZenohComponent constinit property declarations **************************
	static const UECodeGen_Private::FMulticastDelegatePropertyParams NewProp_OnMessageReceived;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Class UZenohComponent constinit property declarations ****************************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("Publish"), .Pointer = &UZenohComponent::execPublish },
	};
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UZenohComponent_Publish, "Publish" }, // 3570160445
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UZenohComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UZenohComponent_Statics

// ********** Begin Class UZenohComponent Property Definitions *************************************
const UECodeGen_Private::FMulticastDelegatePropertyParams Z_Construct_UClass_UZenohComponent_Statics::NewProp_OnMessageReceived = { "OnMessageReceived", nullptr, (EPropertyFlags)0x0010000010080000, UECodeGen_Private::EPropertyGenFlags::InlineMulticastDelegate, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UZenohComponent, OnMessageReceived), Z_Construct_UDelegateFunction_ZenohBridge_OnZenohMessage__DelegateSignature, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OnMessageReceived_MetaData), NewProp_OnMessageReceived_MetaData) }; // 802500904
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UZenohComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UZenohComponent_Statics::NewProp_OnMessageReceived,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UZenohComponent_Statics::PropPointers) < 2048);
// ********** End Class UZenohComponent Property Definitions ***************************************
UObject* (*const Z_Construct_UClass_UZenohComponent_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UActorComponent,
	(UObject* (*)())Z_Construct_UPackage__Script_ZenohBridge,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UZenohComponent_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UZenohComponent_Statics::ClassParams = {
	&UZenohComponent::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_UZenohComponent_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_UZenohComponent_Statics::PropPointers),
	0,
	0x00B000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UZenohComponent_Statics::Class_MetaDataParams), Z_Construct_UClass_UZenohComponent_Statics::Class_MetaDataParams)
};
void UZenohComponent::StaticRegisterNativesUZenohComponent()
{
	UClass* Class = UZenohComponent::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, MakeConstArrayView(Z_Construct_UClass_UZenohComponent_Statics::Funcs));
}
UClass* Z_Construct_UClass_UZenohComponent()
{
	if (!Z_Registration_Info_UClass_UZenohComponent.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UZenohComponent.OuterSingleton, Z_Construct_UClass_UZenohComponent_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UZenohComponent.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UZenohComponent);
// ********** End Class UZenohComponent ************************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h__Script_ZenohBridge_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UZenohComponent, UZenohComponent::StaticClass, TEXT("UZenohComponent"), &Z_Registration_Info_UClass_UZenohComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UZenohComponent), 3667090594U) },
	};
}; // Z_CompiledInDeferFile_FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h__Script_ZenohBridge_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h__Script_ZenohBridge_1634055678{
	TEXT("/Script/ZenohBridge"),
	Z_CompiledInDeferFile_FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h__Script_ZenohBridge_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_UE5BetterSim_RoboSimUE_Plugins_ZenohBridge_Source_ZenohBridge_Public_ZenohComponent_h__Script_ZenohBridge_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
