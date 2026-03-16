// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PlayerControl.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "EngineUtils.h"

bool FMCPTool_PlayerControl::ValidatePIERunning(FString& OutError) const
{
	if (!GEditor->PlayWorld)
	{
		OutError = TEXT("PIE is not running. Use pie_control with operation 'start' first.");
		return false;
	}
	return true;
}

APlayerController* FMCPTool_PlayerControl::GetPIEPlayerController() const
{
	if (!GEditor->PlayWorld)
	{
		return nullptr;
	}
	return GEditor->PlayWorld->GetFirstPlayerController();
}

FMCPToolResult FMCPTool_PlayerControl::Execute(const TSharedRef<FJsonObject>& Params)
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

	if (Operation == TEXT("move_to"))
	{
		return ExecuteMoveTo(Params);
	}
	else if (Operation == TEXT("teleport"))
	{
		return ExecuteTeleport(Params);
	}
	else if (Operation == TEXT("look_at"))
	{
		return ExecuteLookAt(Params);
	}
	else if (Operation == TEXT("get_state"))
	{
		return ExecuteGetState(Params);
	}
	else if (Operation == TEXT("get_nearby_actors"))
	{
		return ExecuteGetNearbyActors(Params);
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: move_to, teleport, look_at, get_state, get_nearby_actors"),
		*Operation));
}

FMCPToolResult FMCPTool_PlayerControl::ExecuteMoveTo(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return FMCPToolResult::Error(TEXT("No player pawn found in PIE."));
	}

	FVector TargetLocation = ExtractVectorParam(Params, TEXT("location"));
	const bool bUseNavigation = ExtractOptionalBool(Params, TEXT("use_navigation"), false);

	if (bUseNavigation)
	{
		// Use the navigation system for pathfinding
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GEditor->PlayWorld);
		if (!NavSys)
		{
			return FMCPToolResult::Error(TEXT("Navigation system not available. Use teleport instead or set use_navigation=false."));
		}

		// Simple move to location via the PlayerController
		PC->MoveToLocation(TargetLocation);

		UE_LOG(LogUnrealClaude, Log, TEXT("Player navigating to: %s"), *TargetLocation.ToString());

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetObjectField(TEXT("target"), UnrealClaudeJsonUtils::VectorToJson(TargetLocation));
		ResultData->SetBoolField(TEXT("using_navigation"), true);
		ResultData->SetStringField(TEXT("note"), TEXT("Player is navigating. Use get_state to check progress."));

		return FMCPToolResult::Success(TEXT("Player navigation started."), ResultData);
	}
	else
	{
		// Direct teleport
		PC->GetPawn()->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);

		UE_LOG(LogUnrealClaude, Log, TEXT("Player teleported to: %s"), *TargetLocation.ToString());

		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(TargetLocation));

		return FMCPToolResult::Success(TEXT("Player moved to location."), ResultData);
	}
}

FMCPToolResult FMCPTool_PlayerControl::ExecuteTeleport(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return FMCPToolResult::Error(TEXT("No player pawn found in PIE."));
	}

	FVector Location = ExtractVectorParam(Params, TEXT("location"), PC->GetPawn()->GetActorLocation());
	FRotator Rotation = ExtractRotatorParam(Params, TEXT("rotation"), PC->GetControlRotation());

	PC->GetPawn()->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation(Rotation);

	UE_LOG(LogUnrealClaude, Log, TEXT("Player teleported to: %s, rotation: %s"),
		*Location.ToString(), *Rotation.ToString());

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Location));
	ResultData->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(Rotation));

	return FMCPToolResult::Success(TEXT("Player teleported."), ResultData);
}

FMCPToolResult FMCPTool_PlayerControl::ExecuteLookAt(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return FMCPToolResult::Error(TEXT("No player pawn found in PIE."));
	}

	FVector TargetLocation;
	bool bHasTarget = false;

	// Try target_actor first
	FString TargetActorName = ExtractOptionalString(Params, TEXT("target_actor"));
	if (!TargetActorName.IsEmpty())
	{
		AActor* TargetActor = FindActorByNameOrLabel(GEditor->PlayWorld, TargetActorName);
		if (!TargetActor)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Actor not found: '%s'"), *TargetActorName));
		}
		TargetLocation = TargetActor->GetActorLocation();
		bHasTarget = true;
	}

	// Try target_location
	if (!bHasTarget && HasVectorParam(Params, TEXT("target_location")))
	{
		TargetLocation = ExtractVectorParam(Params, TEXT("target_location"));
		bHasTarget = true;
	}

	if (!bHasTarget)
	{
		return FMCPToolResult::Error(TEXT("Must provide either 'target_actor' or 'target_location'."));
	}

	// Calculate look-at rotation
	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
	FVector Direction = (TargetLocation - PlayerLocation).GetSafeNormal();
	FRotator LookRotation = Direction.Rotation();

	PC->SetControlRotation(LookRotation);

	UE_LOG(LogUnrealClaude, Log, TEXT("Player looking at: %s (rotation: %s)"),
		*TargetLocation.ToString(), *LookRotation.ToString());

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetObjectField(TEXT("target_location"), UnrealClaudeJsonUtils::VectorToJson(TargetLocation));
	ResultData->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(LookRotation));
	ResultData->SetNumberField(TEXT("distance"), FVector::Dist(PlayerLocation, TargetLocation));

	return FMCPToolResult::Success(TEXT("Player camera rotated to face target."), ResultData);
}

FMCPToolResult FMCPTool_PlayerControl::ExecuteGetState(const TSharedRef<FJsonObject>& Params)
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

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		ResultData->SetStringField(TEXT("pawn_class"), Pawn->GetClass()->GetName());
		ResultData->SetStringField(TEXT("pawn_name"), Pawn->GetName());
		ResultData->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Pawn->GetActorLocation()));
		ResultData->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(PC->GetControlRotation()));
		ResultData->SetObjectField(TEXT("velocity"), UnrealClaudeJsonUtils::VectorToJson(Pawn->GetVelocity()));
		ResultData->SetNumberField(TEXT("speed"), Pawn->GetVelocity().Size());

		// Character-specific info
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
			if (MovementComp)
			{
				ResultData->SetBoolField(TEXT("is_falling"), MovementComp->IsFalling());
				ResultData->SetBoolField(TEXT("is_swimming"), MovementComp->IsSwimming());
				ResultData->SetBoolField(TEXT("is_crouching"), MovementComp->IsCrouching());
				ResultData->SetNumberField(TEXT("max_walk_speed"), MovementComp->MaxWalkSpeed);
			}

			// Current animation info
			if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
			{
				if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();
					if (CurrentMontage)
					{
						ResultData->SetStringField(TEXT("current_montage"), CurrentMontage->GetName());
						ResultData->SetNumberField(TEXT("montage_position"),
							AnimInstance->Montage_GetPosition(CurrentMontage));
					}
				}
			}
		}

		// Check for game-specific properties via reflection
		// Look for HP, MP, EXP, etc. as UPROPERTY fields
		for (TFieldIterator<FNumericProperty> It(Pawn->GetClass()); It; ++It)
		{
			FString PropName = It->GetName();
			if (PropName.Contains(TEXT("HP")) || PropName.Contains(TEXT("Health")) ||
				PropName.Contains(TEXT("MP")) || PropName.Contains(TEXT("Mana")) ||
				PropName.Contains(TEXT("EXP")) || PropName.Contains(TEXT("Stamina")) ||
				PropName.Contains(TEXT("Level")))
			{
				if (FFloatProperty* FloatProp = CastField<FFloatProperty>(*It))
				{
					float Value = FloatProp->GetPropertyValue_InContainer(Pawn);
					ResultData->SetNumberField(PropName, Value);
				}
				else if (FIntProperty* IntProp = CastField<FIntProperty>(*It))
				{
					int32 Value = IntProp->GetPropertyValue_InContainer(Pawn);
					ResultData->SetNumberField(PropName, Value);
				}
				else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(*It))
				{
					double Value = DoubleProp->GetPropertyValue_InContainer(Pawn);
					ResultData->SetNumberField(PropName, Value);
				}
			}
		}
	}
	else
	{
		ResultData->SetBoolField(TEXT("has_pawn"), false);
	}

	// World time
	ResultData->SetNumberField(TEXT("world_time"), GEditor->PlayWorld->GetTimeSeconds());
	ResultData->SetBoolField(TEXT("is_paused"), GEditor->PlayWorld->IsPaused());

	return FMCPToolResult::Success(TEXT("Player state retrieved."), ResultData);
}

FMCPToolResult FMCPTool_PlayerControl::ExecuteGetNearbyActors(const TSharedRef<FJsonObject>& Params)
{
	FString PIEError;
	if (!ValidatePIERunning(PIEError))
	{
		return FMCPToolResult::Error(PIEError);
	}

	APlayerController* PC = GetPIEPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return FMCPToolResult::Error(TEXT("No player pawn found in PIE."));
	}

	const double Radius = ExtractOptionalNumber<double>(Params, TEXT("radius"), 2000.0);
	const int32 MaxResults = ExtractOptionalNumber<int32>(Params, TEXT("max_results"), 20);
	const FString ClassFilter = ExtractOptionalString(Params, TEXT("class_filter"));

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Collect nearby actors
	TArray<TPair<float, AActor*>> SortedActors;

	for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == PC->GetPawn() || Actor->IsHidden())
		{
			continue;
		}

		// Apply class filter if specified
		if (!ClassFilter.IsEmpty())
		{
			FString ClassName = Actor->GetClass()->GetName();
			if (!ClassName.Contains(ClassFilter))
			{
				continue;
			}
		}

		float Distance = FVector::Dist(PlayerLocation, Actor->GetActorLocation());
		if (Distance <= Radius)
		{
			SortedActors.Add(TPair<float, AActor*>(Distance, Actor));
		}
	}

	// Sort by distance
	SortedActors.Sort([](const TPair<float, AActor*>& A, const TPair<float, AActor*>& B)
	{
		return A.Key < B.Key;
	});

	// Build results (limited)
	TArray<TSharedPtr<FJsonValue>> ActorsArray;
	const int32 ResultCount = FMath::Min(SortedActors.Num(), MaxResults);

	for (int32 i = 0; i < ResultCount; ++i)
	{
		AActor* Actor = SortedActors[i].Value;
		TSharedPtr<FJsonObject> ActorObj = BuildActorInfoWithTransformJson(Actor);
		ActorObj->SetNumberField(TEXT("distance"), SortedActors[i].Key);
		ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("actors"), ActorsArray);
	ResultData->SetNumberField(TEXT("count"), ResultCount);
	ResultData->SetNumberField(TEXT("total_in_radius"), SortedActors.Num());
	ResultData->SetNumberField(TEXT("search_radius"), Radius);
	ResultData->SetObjectField(TEXT("player_location"), UnrealClaudeJsonUtils::VectorToJson(PlayerLocation));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d actors within %.0f units"), ResultCount, Radius),
		ResultData);
}
