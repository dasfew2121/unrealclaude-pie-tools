// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_TimelapseCapture.h"
#include "UnrealClaudeModule.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "UnrealClient.h"
#include "HAL/PlatformProcess.h"

namespace
{
	void ResizePixelsBilinear(const TArray<FColor>& InPixels, int32 InWidth, int32 InHeight,
		TArray<FColor>& OutPixels, int32 OutWidth, int32 OutHeight)
	{
		OutPixels.SetNumUninitialized(OutWidth * OutHeight);
		const float ScaleX = static_cast<float>(InWidth) / OutWidth;
		const float ScaleY = static_cast<float>(InHeight) / OutHeight;

		for (int32 Y = 0; Y < OutHeight; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				const int32 SrcX = FMath::Clamp(static_cast<int32>(X * ScaleX), 0, InWidth - 1);
				const int32 SrcY = FMath::Clamp(static_cast<int32>(Y * ScaleY), 0, InHeight - 1);
				OutPixels[Y * OutWidth + X] = InPixels[SrcY * InWidth + SrcX];
			}
		}
	}
}

FMCPToolResult FMCPTool_TimelapseCapture::Execute(const TSharedRef<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMCPToolResult::Error(TEXT("Editor is not available."));
	}

	if (!GEditor->PlayWorld)
	{
		return FMCPToolResult::Error(TEXT("PIE is not running. Use pie_control with operation 'start' first."));
	}

	// Parse settings
	FCaptureSettings Settings;
	Settings.Count = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("count"), 10), 1, 30);
	Settings.IntervalMs = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("interval_ms"), 500), 100, 5000);
	Settings.bIncludeHUDInfo = ExtractOptionalBool(Params, TEXT("include_hud_info"), true);

	// Resolution
	FString Resolution = ExtractOptionalString(Params, TEXT("resolution"), TEXT("medium")).ToLower();
	if (Resolution == TEXT("low"))
	{
		Settings.Width = 512;
		Settings.Height = 288;
		Settings.JPEGQuality = 55;
	}
	else if (Resolution == TEXT("high"))
	{
		Settings.Width = 1024;
		Settings.Height = 576;
		Settings.JPEGQuality = 70;
	}
	else // medium (default)
	{
		Settings.Width = 768;
		Settings.Height = 432;
		Settings.JPEGQuality = 65;
	}

	// Get viewport
	FViewport* Viewport = GEditor->GetPIEViewport();
	if (!Viewport)
	{
		Viewport = GEditor->GetActiveViewport();
	}
	if (!Viewport)
	{
		return FMCPToolResult::Error(TEXT("No viewport available for capture."));
	}

	const double StartTime = FPlatformTime::Seconds();

	UE_LOG(LogUnrealClaude, Log, TEXT("Starting timelapse capture: %d frames at %dms intervals (%dx%d)"),
		Settings.Count, Settings.IntervalMs, Settings.Width, Settings.Height);

	// Capture frames
	TArray<TSharedPtr<FJsonValue>> FramesArray;
	int32 SuccessCount = 0;

	for (int32 i = 0; i < Settings.Count; ++i)
	{
		// Wait for the interval (skip wait before first frame)
		if (i > 0)
		{
			FPlatformProcess::Sleep(Settings.IntervalMs / 1000.0f);
		}

		// Check if PIE was stopped during capture
		if (!GEditor->PlayWorld)
		{
			UE_LOG(LogUnrealClaude, Warning, TEXT("PIE stopped during timelapse capture at frame %d/%d"),
				i, Settings.Count);
			break;
		}

		TSharedPtr<FJsonObject> FrameData = CaptureSingleFrame(Viewport, Settings, i, StartTime);
		if (FrameData.IsValid())
		{
			FramesArray.Add(MakeShared<FJsonValueObject>(FrameData));
			SuccessCount++;
		}
	}

	const double TotalTime = FPlatformTime::Seconds() - StartTime;

	UE_LOG(LogUnrealClaude, Log, TEXT("Timelapse capture complete: %d/%d frames in %.1fs"),
		SuccessCount, Settings.Count, TotalTime);

	// Build result
	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetArrayField(TEXT("frames"), FramesArray);
	ResultData->SetNumberField(TEXT("captured_count"), SuccessCount);
	ResultData->SetNumberField(TEXT("requested_count"), Settings.Count);
	ResultData->SetNumberField(TEXT("interval_ms"), Settings.IntervalMs);
	ResultData->SetNumberField(TEXT("total_time_seconds"), TotalTime);
	ResultData->SetNumberField(TEXT("width"), Settings.Width);
	ResultData->SetNumberField(TEXT("height"), Settings.Height);
	ResultData->SetStringField(TEXT("format"), TEXT("jpeg"));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Captured %d frames over %.1f seconds"), SuccessCount, TotalTime),
		ResultData);
}

TSharedPtr<FJsonObject> FMCPTool_TimelapseCapture::CaptureSingleFrame(
	FViewport* Viewport,
	const FCaptureSettings& Settings,
	int32 FrameIndex,
	double StartTime) const
{
	if (!Viewport)
	{
		return nullptr;
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return nullptr;
	}

	// Read pixels
	TArray<FColor> Pixels;
	if (!Viewport->ReadPixels(Pixels))
	{
		return nullptr;
	}

	if (Pixels.Num() != ViewportSize.X * ViewportSize.Y)
	{
		return nullptr;
	}

	// Resize
	TArray<FColor> ResizedPixels;
	ResizePixelsBilinear(Pixels, ViewportSize.X, ViewportSize.Y,
		ResizedPixels, Settings.Width, Settings.Height);

	// Compress JPEG
	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapper =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

	if (!ImageWrapper.IsValid())
	{
		return nullptr;
	}

	if (!ImageWrapper->SetRaw(ResizedPixels.GetData(), ResizedPixels.Num() * sizeof(FColor),
		Settings.Width, Settings.Height, ERGBFormat::BGRA, 8))
	{
		return nullptr;
	}

	TArray64<uint8> CompressedData = ImageWrapper->GetCompressed(Settings.JPEGQuality);
	if (CompressedData.Num() == 0)
	{
		return nullptr;
	}

	// Base64 encode
	FString Base64Image = FBase64::Encode(CompressedData.GetData(), CompressedData.Num());

	// Build frame JSON
	TSharedPtr<FJsonObject> FrameObj = MakeShared<FJsonObject>();
	FrameObj->SetNumberField(TEXT("frame_index"), FrameIndex);
	FrameObj->SetNumberField(TEXT("timestamp_ms"),
		(FPlatformTime::Seconds() - StartTime) * 1000.0);
	FrameObj->SetStringField(TEXT("image_base64"), Base64Image);
	FrameObj->SetNumberField(TEXT("size_bytes"), CompressedData.Num());

	// Include game state info if requested
	if (Settings.bIncludeHUDInfo && GEditor->PlayWorld)
	{
		FrameObj->SetNumberField(TEXT("game_time"), GEditor->PlayWorld->GetTimeSeconds());

		if (APlayerController* PC = GEditor->PlayWorld->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				FrameObj->SetObjectField(TEXT("player_location"),
					UnrealClaudeJsonUtils::VectorToJson(Pawn->GetActorLocation()));
				FrameObj->SetObjectField(TEXT("player_rotation"),
					UnrealClaudeJsonUtils::RotatorToJson(PC->GetControlRotation()));
				FrameObj->SetNumberField(TEXT("player_speed"), Pawn->GetVelocity().Size());
			}
		}
	}

	return FrameObj;
}
