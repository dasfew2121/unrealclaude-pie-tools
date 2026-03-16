// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Control Play-In-Editor (PIE) sessions
 *
 * Operations:
 *   - start: Begin a PIE session
 *   - stop: End the current PIE session
 *   - status: Check if PIE is currently running
 *   - pause: Pause the PIE session
 *   - resume: Resume a paused PIE session
 *
 * This enables automated gameplay testing without requiring
 * the user to manually press Alt+P.
 */
class FMCPTool_PIEControl : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("pie_control");
		Info.Description = TEXT(
			"Control Play-In-Editor (PIE) sessions.\n\n"
			"Operations:\n"
			"- 'start': Begin a PIE session in the active level\n"
			"- 'stop': End the current PIE session\n"
			"- 'status': Check whether PIE is currently running or paused\n"
			"- 'pause': Pause the running PIE session\n"
			"- 'resume': Resume a paused PIE session\n\n"
			"Use this to automate gameplay testing: start PIE, send inputs, capture screenshots, "
			"analyze behavior, then stop PIE. The editor viewport automatically switches to the "
			"game view during PIE.\n\n"
			"Note: Starting PIE may take a moment while the level initializes."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("operation"), TEXT("string"),
				TEXT("Operation to perform: 'start', 'stop', 'status', 'pause', 'resume'"), true)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecuteStart();
	FMCPToolResult ExecuteStop();
	FMCPToolResult ExecuteStatus();
	FMCPToolResult ExecutePause();
	FMCPToolResult ExecuteResume();
};
