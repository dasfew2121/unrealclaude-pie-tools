// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Timelapse/Burst Screenshot Capture
 *
 * Captures a series of screenshots at regular intervals during PIE,
 * returning them as an array of base64-encoded images with timestamps.
 *
 * Designed for visual analysis of gameplay sequences:
 * - Attack animations and damage timing
 * - Movement and collision behavior
 * - VFX and particle timing
 * - UI state changes during gameplay
 *
 * The returned images can be assembled into a collage using
 * the companion Python script (UE5_Tools/screenshot_collage.py).
 */
class FMCPTool_TimelapseCapture : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("timelapse_capture");
		Info.Description = TEXT(
			"Capture a burst of screenshots at regular intervals during PIE.\n\n"
			"Returns an array of timestamped base64 JPEG images for analyzing "
			"gameplay sequences frame-by-frame.\n\n"
			"Use cases:\n"
			"- Verify attack animation timing vs damage application\n"
			"- Check collision and hit detection accuracy\n"
			"- Analyze movement and pathfinding behavior\n"
			"- Debug VFX/particle timing relative to gameplay events\n"
			"- Monitor UI state changes during combat or interaction\n\n"
			"Parameters:\n"
			"- count: Number of screenshots (1-30, default 10)\n"
			"- interval_ms: Time between captures in milliseconds (100-5000, default 500)\n"
			"- resolution: Image resolution 'low' (512x288), 'medium' (768x432), 'high' (1024x576)\n\n"
			"Total capture time = count * interval_ms. A 10-frame capture at 500ms takes 5 seconds.\n\n"
			"Requires PIE to be running."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("count"), TEXT("number"),
				TEXT("Number of screenshots to capture (1-30, default: 10)"), false, TEXT("10")),
			FMCPToolParameter(TEXT("interval_ms"), TEXT("number"),
				TEXT("Milliseconds between captures (100-5000, default: 500)"), false, TEXT("500")),
			FMCPToolParameter(TEXT("resolution"), TEXT("string"),
				TEXT("Resolution: 'low' (512x288), 'medium' (768x432), 'high' (1024x576)"), false, TEXT("medium")),
			FMCPToolParameter(TEXT("include_hud_info"), TEXT("boolean"),
				TEXT("Include player position/rotation metadata with each frame (default: true)"), false, TEXT("true"))
		};
		// Not ReadOnly because it interacts with PIE state, but not destructive
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	struct FCaptureSettings
	{
		int32 Count = 10;
		int32 IntervalMs = 500;
		int32 Width = 768;
		int32 Height = 432;
		int32 JPEGQuality = 65;
		bool bIncludeHUDInfo = true;
	};

	// Capture a single frame and return its base64 data
	TSharedPtr<FJsonObject> CaptureSingleFrame(
		FViewport* Viewport,
		const FCaptureSettings& Settings,
		int32 FrameIndex,
		double StartTime) const;
};
