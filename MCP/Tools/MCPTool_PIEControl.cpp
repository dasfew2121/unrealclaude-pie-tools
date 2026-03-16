// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PIEControl.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LevelEditor.h"

FMCPToolResult FMCPTool_PIEControl::Execute(const TSharedRef<FJsonObject>& Params)
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

	if (Operation == TEXT("start"))
	{
		return ExecuteStart();
	}
	else if (Operation == TEXT("stop"))
	{
		return ExecuteStop();
	}
	else if (Operation == TEXT("status"))
	{
		return ExecuteStatus();
	}
	else if (Operation == TEXT("pause"))
	{
		return ExecutePause();
	}
	else if (Operation == TEXT("resume"))
	{
		return ExecuteResume();
	}

	return FMCPToolResult::Error(FString::Printf(
		TEXT("Unknown operation: '%s'. Valid: start, stop, status, pause, resume"), *Operation));
}

FMCPToolResult FMCPTool_PIEControl::ExecuteStart()
{
	// Check if PIE is already running
	if (GEditor->PlayWorld)
	{
		TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
		ResultData->SetBoolField(TEXT("already_running"), true);
		ResultData->SetBoolField(TEXT("is_paused"), GEditor->PlayWorld->IsPaused());
		return FMCPToolResult::Success(TEXT("PIE is already running."), ResultData);
	}

	// Request a new PIE session
	FRequestPlaySessionParams PlayParams;
	PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
	PlayParams.DestinationSlateViewport = NAME_None;

	GEditor->RequestPlaySession(PlayParams);

	UE_LOG(LogUnrealClaude, Log, TEXT("PIE session start requested"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("requested"), true);
	ResultData->SetStringField(TEXT("note"),
		TEXT("PIE session requested. It may take a moment to initialize. Use 'status' to check when ready."));

	return FMCPToolResult::Success(TEXT("PIE session start requested."), ResultData);
}

FMCPToolResult FMCPTool_PIEControl::ExecuteStop()
{
	if (!GEditor->PlayWorld)
	{
		return FMCPToolResult::Error(TEXT("PIE is not currently running."));
	}

	GEditor->RequestEndPlayMap();

	UE_LOG(LogUnrealClaude, Log, TEXT("PIE session stop requested"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("stopped"), true);

	return FMCPToolResult::Success(TEXT("PIE session stopped."), ResultData);
}

FMCPToolResult FMCPTool_PIEControl::ExecuteStatus()
{
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();

	const bool bIsRunning = GEditor->PlayWorld != nullptr;
	ResultData->SetBoolField(TEXT("is_running"), bIsRunning);

	if (bIsRunning)
	{
		ResultData->SetBoolField(TEXT("is_paused"), GEditor->PlayWorld->IsPaused());

		// Get player info if available
		UWorld* PlayWorld = GEditor->PlayWorld;
		if (APlayerController* PC = PlayWorld->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				FVector Location = Pawn->GetActorLocation();
				FRotator Rotation = Pawn->GetActorRotation();

				TSharedPtr<FJsonObject> PlayerInfo = MakeShared<FJsonObject>();
				PlayerInfo->SetObjectField(TEXT("location"), UnrealClaudeJsonUtils::VectorToJson(Location));
				PlayerInfo->SetObjectField(TEXT("rotation"), UnrealClaudeJsonUtils::RotatorToJson(Rotation));
				PlayerInfo->SetStringField(TEXT("pawn_class"), Pawn->GetClass()->GetName());
				ResultData->SetObjectField(TEXT("player"), PlayerInfo);
			}
		}

		// Get elapsed time
		ResultData->SetNumberField(TEXT("elapsed_seconds"), PlayWorld->GetTimeSeconds());
	}

	return FMCPToolResult::Success(
		bIsRunning ? TEXT("PIE is running.") : TEXT("PIE is not running."),
		ResultData);
}

FMCPToolResult FMCPTool_PIEControl::ExecutePause()
{
	if (!GEditor->PlayWorld)
	{
		return FMCPToolResult::Error(TEXT("PIE is not currently running."));
	}

	if (GEditor->PlayWorld->IsPaused())
	{
		return FMCPToolResult::Success(TEXT("PIE is already paused."));
	}

	// Pause through the player controller
	if (APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController())
	{
		PC->SetPause(true);
	}
	else
	{
		// Fallback: pause world directly
		GEditor->PlayWorld->bIsPaused = true;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("PIE session paused"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("paused"), true);

	return FMCPToolResult::Success(TEXT("PIE session paused."), ResultData);
}

FMCPToolResult FMCPTool_PIEControl::ExecuteResume()
{
	if (!GEditor->PlayWorld)
	{
		return FMCPToolResult::Error(TEXT("PIE is not currently running."));
	}

	if (!GEditor->PlayWorld->IsPaused())
	{
		return FMCPToolResult::Success(TEXT("PIE is not paused."));
	}

	if (APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController())
	{
		PC->SetPause(false);
	}
	else
	{
		GEditor->PlayWorld->bIsPaused = false;
	}

	UE_LOG(LogUnrealClaude, Log, TEXT("PIE session resumed"));

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetBoolField(TEXT("resumed"), true);

	return FMCPToolResult::Success(TEXT("PIE session resumed."), ResultData);
}
