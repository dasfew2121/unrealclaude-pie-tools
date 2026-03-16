// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Virtual Input Controller
 *
 * Injects keyboard, mouse, and Enhanced Input actions into the running PIE session
 * WITHOUT interfering with the user's real mouse/keyboard on other applications.
 *
 * Input is injected directly into UE's input pipeline (FSlateApplication for keys,
 * Enhanced Input subsystem for actions, PlayerController for movement).
 *
 * Operations:
 *   - key_press: Simulate a keyboard key press (with optional duration and modifiers)
 *   - key_release: Release a previously held key
 *   - mouse_click: Simulate a mouse click at viewport coordinates
 *   - mouse_move: Move the virtual mouse cursor by delta
 *   - inject_action: Inject an Enhanced Input action value directly
 *   - get_bindings: List current input action bindings and their keys
 */
class FMCPTool_VirtualInput : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("virtual_input");
		Info.Description = TEXT(
			"Inject virtual keyboard, mouse, and game input into a running PIE session.\n\n"
			"This is a 'second keyboard/mouse' that only affects Unreal — it does NOT move "
			"the user's real cursor or steal focus from other applications.\n\n"
			"Operations:\n"
			"- 'key_press': Press a key for a duration (default instant). Supports modifiers (Shift, Ctrl, Alt).\n"
			"- 'key_release': Release a held key.\n"
			"- 'mouse_click': Click at viewport coordinates (normalized 0-1 or pixel).\n"
			"- 'mouse_move': Rotate camera by yaw/pitch delta (in degrees).\n"
			"- 'inject_action': Inject an Enhanced Input action by asset path (e.g., '/Game/Input/IA_Attack').\n"
			"- 'get_bindings': List all active input action bindings and their mapped keys.\n\n"
			"Keys use UE names: W, A, S, D, SpaceBar, LeftMouseButton, One, Two, F1, Escape, etc.\n\n"
			"Requires PIE to be running. Use pie_control to start PIE first."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation: 'key_press', 'key_release', 'mouse_click', 'mouse_move', 'inject_action', 'get_bindings'"), true),

			// For key_press / key_release
			FMCPToolParameter(TEXT("key"), TEXT("string"),
				TEXT("UE key name (e.g., 'W', 'SpaceBar', 'LeftMouseButton', 'One', 'F1')"), false),
			FMCPToolParameter(TEXT("duration_ms"), TEXT("number"),
				TEXT("How long to hold the key in milliseconds (0 = single press/release cycle). Max 10000ms."), false, TEXT("0")),
			FMCPToolParameter(TEXT("modifiers"), TEXT("array"),
				TEXT("Modifier keys to hold: ['Shift', 'Ctrl', 'Alt'] (array of strings)"), false),

			// For mouse_click
			FMCPToolParameter(TEXT("button"), TEXT("string"),
				TEXT("Mouse button: 'Left', 'Right', 'Middle'"), false, TEXT("Left")),
			FMCPToolParameter(TEXT("screen_x"), TEXT("number"),
				TEXT("Click X position (0.0-1.0 normalized, or pixel coordinate if > 1)"), false),
			FMCPToolParameter(TEXT("screen_y"), TEXT("number"),
				TEXT("Click Y position (0.0-1.0 normalized, or pixel coordinate if > 1)"), false),

			// For mouse_move (camera rotation)
			FMCPToolParameter(TEXT("delta_yaw"), TEXT("number"),
				TEXT("Horizontal rotation in degrees (positive = right)"), false, TEXT("0")),
			FMCPToolParameter(TEXT("delta_pitch"), TEXT("number"),
				TEXT("Vertical rotation in degrees (positive = up)"), false, TEXT("0")),

			// For inject_action
			FMCPToolParameter(TEXT("action_path"), TEXT("string"),
				TEXT("Asset path of InputAction (e.g., '/Game/Input/IA_Attack')"), false),
			FMCPToolParameter(TEXT("action_value"), TEXT("object"),
				TEXT("Action value: {x, y, z} for axis actions, or {x: 1.0} for digital triggers"), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteKeyPress(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteKeyRelease(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteMouseClick(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteMouseMove(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteInjectAction(const TSharedRef<FJsonObject>& Params);
	FMCPToolResult ExecuteGetBindings(const TSharedRef<FJsonObject>& Params);

	// Helpers
	FKey ParseKeyName(const FString& KeyName, FString& OutError) const;
	bool ValidatePIERunning(FString& OutError) const;
	APlayerController* GetPIEPlayerController() const;
};
