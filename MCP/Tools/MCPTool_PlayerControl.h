// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: High-level Player Control
 *
 * Provides higher-level player character commands that bypass the input system
 * for reliable programmatic control during PIE testing.
 *
 * Operations:
 *   - move_to: Navigate the player to a world location (via AI navigation or direct teleport)
 *   - teleport: Instantly move the player to a location
 *   - look_at: Rotate the camera to face a location or actor
 *   - get_state: Get comprehensive player state (position, HP, animation, target, etc.)
 *   - get_nearby_actors: Find actors near the player within a radius
 */
class FMCPTool_PlayerControl : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("player_control");
		Info.Description = TEXT(
			"High-level player character control during PIE sessions.\n\n"
			"Operations:\n"
			"- 'move_to': Navigate player to world coordinates. Uses AI pathfinding if use_navigation=true, "
			"otherwise direct movement.\n"
			"- 'teleport': Instantly move the player to a location with optional rotation.\n"
			"- 'look_at': Rotate the player/camera to face a world location or named actor.\n"
			"- 'get_state': Get player position, rotation, velocity, pawn class, and game-specific stats.\n"
			"- 'get_nearby_actors': Find all actors within a radius of the player. "
			"Useful for locating mobs, NPCs, items.\n\n"
			"Requires PIE to be running. Use pie_control to start PIE first.\n\n"
			"Coordinates are in Unreal world units (centimeters)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'move_to', 'teleport', 'look_at', 'get_state', 'get_nearby_actors'"), true),

			// For move_to / teleport
			FMCPToolParameter(TEXT("location"), TEXT("object"),
				TEXT("Target location: {x, y, z} in world coordinates"), false),
			FMCPToolParameter(TEXT("rotation"), TEXT("object"),
				TEXT("Target rotation: {pitch, yaw, roll} in degrees (for teleport)"), false),
			FMCPToolParameter(TEXT("use_navigation"), TEXT("boolean"),
				TEXT("Use AI navigation mesh for pathfinding (default: false = direct teleport)"), false, TEXT("false")),

			// For look_at
			FMCPToolParameter(TEXT("target_location"), TEXT("object"),
				TEXT("World location to look at: {x, y, z}"), false),
			FMCPToolParameter(TEXT("target_actor"), TEXT("string"),
				TEXT("Name of actor to look at (alternative to target_location)"), false),

			// For get_nearby_actors
			FMCPToolParameter(TEXT("radius"), TEXT("number"),
				TEXT("Search radius in centimeters (default: 2000)"), false, TEXT("2000")),
			FMCPToolParameter(TEXT("class_filter"), TEXT("string"),
				TEXT("Filter by actor class name (e.g., 'MetinCharacter', 'NPC')"), false),
			FMCPToolParameter(TEXT("max_results"), TEXT("number"),
				TEXT("Maximum actors to return (default: 20)"), false, TEXT("20"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteMoveTo(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteTeleport(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteLookAt(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetState(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetNearbyActors(const TSharedRef<FJsonObject>& Params);

	bool ValidatePIERunning(FString& OutError) const;
	APlayerController* GetPIEPlayerController() const;
};
