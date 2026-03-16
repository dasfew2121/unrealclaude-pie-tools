// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_VirtualInput.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"

FMCPToolResult FMCPTool_VirtualInput::Execute(const TSharedRef<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor is not available."));
	}

	FString Operation;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("operation"), Operation, ParamError))
	{
		return ParamError.GetValue();
	}

	Operation = Operation.ToLower();

	if (Operation == TEXT("key_press"))
	{
		return ExecuteKeyPress(Params);
	}
	else if (Operation == TEXT("key_release"))
	{
		return ExecuteKeyRelease(Params);
	}
	else if (Operation == TEXT("mouse_click"))
	{
		return ExecuteMouseClick(Params);
	}
	else if (Operation == TEXT("mouse_move"))
	{
		return ExecuteMouseMove(Params);
	}
	else if (Operation == TEXT("inject_action"))
	{
		return ExecuteInjectAction(Params);
	}
	else if (Operation == TEXT("get_bindings"))
	{
		return ExecuteGetBindings(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: key_press, key_release, mouse_click, mouse_move, inject_action, get_bindings"),
		*Operation));
}

bool FMCPTool_VirtualInput::ValidatePIERunning(FString& OutError) const
{
	if (!GEditor->PlayWorld)
	{
		OutError = TEXT("PIE is not running. Use pie_control with operation 'start' first.");
		return false;
	}
	if (GEditor->PlayWorld->IsPaused())
	{
		OutError = TEXT("PIE is paused. Use pie_control with operation 'resume' first.");
		return false;
	}
	return true;
}

APlayerController* FMCPTool_VirtualInput::GetPIEPlayerController() const
{
	if (!GEditor->PlayWorld)
	{
		return nullptr;
	}
	return GEditor->PlayWorld->GetFirstPlayerController();
}

FKey FMCPTool_VirtualInput::ParseKeyName(const FString& KeyName, FString& OutError) const
{
	FKey Key(FName(*KeyName));
	if (!Key.IsValid())
	{
		OutError = FString::Printf(
			TEXT("Invalid key name: '%s'. Use UE key names like W, SpaceBar, LeftMouseButton, One, F1, etc."),
			*KeyName);
	}
	return Key;
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteKeyPress(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	FString KeyName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("key"), KeyName, ParamError))
	{
		return ParamError.GetValue();
	}

	FString KeyError;
	FKey Key = ParseKeyName(KeyName, KeyError);
	if (!Key.IsValid())
	{
		return FMCPToolResult::Error(KeyError);
	}

	const int32 DurationMs = ExtractOptionalNumber<int32>(Params, TEXT("duration_ms"), 0);
	if (DurationMs < 0 || DurationMs > 10000)
	{
		return FMCPToolResult::Error(TEXT("duration_ms must be between 0 and 10000."));
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	// Check for modifier keys
	bool bShift = false, bCtrl = false, bAlt = false;
	const TArray<TSharedPtr<FJsonValue>>* ModifiersArray;
	if (Params->TryGetArrayField(TEXT("modifiers"), ModifiersArray))
	{
		for (const auto& ModVal : *ModifiersArray)
		{
			FString Mod = ModVal->AsString().ToLower();
			if (Mod == TEXT("shift")) bShift = true;
			else if (Mod == TEXT("ctrl") || Mod == TEXT("control")) bCtrl = true;
			else if (Mod == TEXT("alt")) bAlt = true;
		}
	}

	// Inject the key press into the player controller's input stack
	// This goes through the normal input processing pipeline
	FInputKeyParams KeyParams(Key, IE_Pressed, 1.0);
	PC->InputKey(KeyParams);

	UE_LOG(LogUnrealClaude, Log, TEXT("Virtual key press: %s (duration: %dms, shift:%d ctrl:%d alt:%d)"),
		*KeyName, DurationMs, bShift, bCtrl, bAlt);

	// If duration specified, set up a timer to release the key
	if (DurationMs > 0)
	{
		// Use a timer on the game world to release the key after duration
		FTimerHandle TimerHandle;
		TWeakObjectPtr<APlayerController> WeakPC = PC;
		FKey CapturedKey = Key;

		GEditor->PlayWorld->GetTimerManager().SetTimer(
			TimerHandle,
			[WeakPC, CapturedKey]()
			{
				if (APlayerController* StrongPC = WeakPC.Get())
				{
					FInputKeyParams ReleaseParams(CapturedKey, IE_Released, 0.0);
					StrongPC->InputKey(ReleaseParams);
				}
			},
			DurationMs / 1000.0f,
			false
		);
	}
	else
	{
		// Instant press+release
		FInputKeyParams ReleaseParams(Key, IE_Released, 0.0);
		PC->InputKey(ReleaseParams);
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("key"), KeyName);
	ResultData->SetNumberField(TEXT("duration_ms"), DurationMs);
	ResultData->SetBoolField(TEXT("shift"), bShift);
	ResultData->SetBoolField(TEXT("ctrl"), bCtrl);
	ResultData->SetBoolField(TEXT("alt"), bAlt);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Key '%s' pressed%s"),
			*KeyName,
			DurationMs > 0 ? *FString::Printf(TEXT(" (held for %dms)"), DurationMs) : TEXT("")),
		ResultData);
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteKeyRelease(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	FString KeyName;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("key"), KeyName, ParamError))
	{
		return ParamError.GetValue();
	}

	FString KeyError;
	FKey Key = ParseKeyName(KeyName, KeyError);
	if (!Key.IsValid())
	{
		return FMCPToolResult::Error(KeyError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	FInputKeyParams ReleaseParams(Key, IE_Released, 0.0);
	PC->InputKey(ReleaseParams);

	UE_LOG(LogUnrealClaude, Log, TEXT("Virtual key release: %s"), *KeyName);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("key"), KeyName);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Key '%s' released"), *KeyName), ResultData);
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteMouseClick(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	const FString ButtonName = ExtractOptionalString(Params, TEXT("button"), TEXT("Left"));
	FKey MouseKey;
	if (ButtonName.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		MouseKey = EKeys::LeftMouseButton;
	}
	else if (ButtonName.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		MouseKey = EKeys::RightMouseButton;
	}
	else if (ButtonName.Equals(TEXT("Middle"), ESearchCase::IgnoreCase))
	{
		MouseKey = EKeys::MiddleMouseButton;
	}
	else
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Invalid mouse button: '%s'. Valid: Left, Right, Middle"), *ButtonName));
	}

	// Get screen position if provided
	const double ScreenX = ExtractOptionalNumber<double>(Params, TEXT("screen_x"), 0.5);
	const double ScreenY = ExtractOptionalNumber<double>(Params, TEXT("screen_y"), 0.5);

	// Get viewport for coordinate conversion
	FViewport* Viewport = GEditor->GetPIEViewport();
	if (!Viewport)
	{
		return FMCPToolResult::Error(TEXT("No PIE viewport available."));
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	int32 PixelX, PixelY;

	// Normalized (0-1) vs pixel coordinates
	if (ScreenX <= 1.0 && ScreenY <= 1.0 && ScreenX >= 0.0 && ScreenY >= 0.0)
	{
		PixelX = FMath::RoundToInt32(ScreenX * ViewportSize.X);
		PixelY = FMath::RoundToInt32(ScreenY * ViewportSize.Y);
	}
	else
	{
		PixelX = FMath::RoundToInt32(ScreenX);
		PixelY = FMath::RoundToInt32(ScreenY);
	}

	// Inject click via PlayerController InputKey
	FInputKeyParams PressParams(MouseKey, IE_Pressed, 1.0);
	PC->InputKey(PressParams);

	FInputKeyParams ReleaseParams(MouseKey, IE_Released, 0.0);
	PC->InputKey(ReleaseParams);

	UE_LOG(LogUnrealClaude, Log, TEXT("Virtual mouse %s click at (%d, %d)"),
		*ButtonName, PixelX, PixelY);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("button"), ButtonName);
	ResultData->SetNumberField(TEXT("pixel_x"), PixelX);
	ResultData->SetNumberField(TEXT("pixel_y"), PixelY);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Mouse %s click at (%d, %d)"), *ButtonName, PixelX, PixelY),
		ResultData);
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteMouseMove(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	const double DeltaYaw = ExtractOptionalNumber<double>(Params, TEXT("delta_yaw"), 0.0);
	const double DeltaPitch = ExtractOptionalNumber<double>(Params, TEXT("delta_pitch"), 0.0);

	if (FMath::IsNearlyZero(DeltaYaw) && FMath::IsNearlyZero(DeltaPitch))
	{
		return FMCPToolResult::Error(TEXT("At least one of delta_yaw or delta_pitch must be non-zero."));
	}

	// Apply rotation directly to the controller
	PC->AddYawInput(DeltaYaw);
	PC->AddPitchInput(DeltaPitch);

	UE_LOG(LogUnrealClaude, Log, TEXT("Virtual mouse move: yaw=%.1f, pitch=%.1f"), DeltaYaw, DeltaPitch);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetNumberField(TEXT("delta_yaw"), DeltaYaw);
	ResultData->SetNumberField(TEXT("delta_pitch"), DeltaPitch);

	FRotator CurrentRotation = PC->GetControlRotation();
	ResultData->SetObjectField(TEXT("resulting_rotation"), UnrealClaudeJsonUtils::RotatorToJson(CurrentRotation));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Camera rotated: yaw=%.1f, pitch=%.1f"), DeltaYaw, DeltaPitch),
		ResultData);
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteInjectAction(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	FString ActionPath;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("action_path"), ActionPath, ParamError))
	{
		return ParamError.GetValue();
	}

	// Load the InputAction asset
	UInputAction* InputAction = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!InputAction)
	{
		return FMCPToolResult::Error(FString::Printf(
			TEXT("Could not load InputAction: '%s'"), *ActionPath));
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	// Get the Enhanced Input subsystem
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!InputSubsystem)
	{
		return FMCPToolResult::Error(TEXT("Enhanced Input subsystem not available."));
	}

	// Extract action value
	FInputActionValue ActionValue;
	const TSharedPtr<FJsonObject>* ValueObj;
	if (Params->TryGetObjectField(TEXT("action_value"), ValueObj) && ValueObj && (*ValueObj).IsValid())
	{
		double X = 0.0, Y = 0.0, Z = 0.0;
		(*ValueObj)->TryGetNumberField(TEXT("x"), X);
		(*ValueObj)->TryGetNumberField(TEXT("y"), Y);
		(*ValueObj)->TryGetNumberField(TEXT("z"), Z);

		// Determine value type from the InputAction
		switch (InputAction->ValueType)
		{
		case EInputActionValueType::Boolean:
			ActionValue = FInputActionValue(X > 0.0);
			break;
		case EInputActionValueType::Axis1D:
			ActionValue = FInputActionValue(static_cast<float>(X));
			break;
		case EInputActionValueType::Axis2D:
			ActionValue = FInputActionValue(FVector2D(X, Y));
			break;
		case EInputActionValueType::Axis3D:
			ActionValue = FInputActionValue(FVector(X, Y, Z));
			break;
		}
	}
	else
	{
		// Default: trigger as a digital press
		ActionValue = FInputActionValue(true);
	}

	// Inject the action
	InputSubsystem->InjectInputForAction(InputAction, ActionValue);

	UE_LOG(LogUnrealClaude, Log, TEXT("Injected action: %s"), *ActionPath);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("action_path"), ActionPath);
	ResultData->SetStringField(TEXT("action_name"), InputAction->GetName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Injected action: %s"), *InputAction->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_VirtualInput::ExecuteGetBindings(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC)
	{
		return FMCPToolResult::Error(TEXT("No player controller found in PIE."));
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!InputSubsystem)
	{
		return FMCPToolResult::Error(TEXT("Enhanced Input subsystem not available."));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> BindingsArray;

	// Get all active mapping contexts and their bindings
	// We iterate through the applied mapping contexts
	const TArray<FEnhancedActionKeyMapping>& Mappings = InputSubsystem->GetAllPlayerMappableActionKeyMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (!Mapping.Action)
		{
			continue;
		}

		TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
		BindingObj->SetStringField(TEXT("action_name"), Mapping.Action->GetName());
		BindingObj->SetStringField(TEXT("action_path"), Mapping.Action->GetPathName());
		BindingObj->SetStringField(TEXT("key"), Mapping.Key.ToString());

		BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
	}

	ResultData->SetArrayField(TEXT("bindings"), BindingsArray);
	ResultData->SetNumberField(TEXT("count"), BindingsArray.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d input bindings"), BindingsArray.Num()),
		ResultData);
}
