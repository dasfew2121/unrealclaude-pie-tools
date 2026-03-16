---
name: unreal-mcp
description: >
  Guide for using the UnrealClaude MCP bridge to manipulate Unreal Engine 5.7 Editor.
  Use this skill whenever the user wants to interact with Unreal Editor through MCP tools —
  spawning/moving/deleting actors, modifying blueprints or animation blueprints, capturing viewports,
  running console commands, executing scripts, searching assets, setting material properties,
  or any editor automation task. Also trigger when the user mentions "MCP", "unreal_" tool names,
  "capture viewport", "spawn actor", "execute script", "anim blueprint", "state machine",
  "blueprint modify", "console command", "output log", "asset search", or wants to do anything
  in the Unreal Editor without opening it manually. Trigger even for indirect requests like
  "place some lights in the level", "show me what the viewport looks like", or "set up the
  animation state machine".
---

# UnrealClaude MCP Bridge — Operator Guide

The UnrealClaude MCP bridge connects Claude to a live Unreal Engine 5.7 Editor session. All tools are prefixed with `unreal_` and communicate over HTTP to the plugin at `localhost:3000`. This guide teaches you how to use these tools effectively.

## Connection — Always Check First

Before any MCP operation, call `unreal_status` to verify the editor is running. If it returns NOT CONNECTED, tell the user to start Unreal Editor with the UnrealClaude plugin enabled. Do not attempt other MCP calls when disconnected — they will all fail silently or timeout.

## Tool Categories & When to Use Each

### Actor & Level Tools
| Tool | Use When |
|------|----------|
| `unreal_spawn_actor` | User wants to place something in the level (lights, meshes, cameras, etc.) |
| `unreal_get_level_actors` | Need to see what's in the current level, find actors by name/class |
| `unreal_set_property` | Change any property on an existing actor (intensity, color, material, etc.) |
| `unreal_move_actor` | Reposition, rotate, or scale an actor |
| `unreal_delete_actors` | Remove actors from the level |
| `unreal_open_level` | Switch to a different map/level |

### Console & Logging
| Tool | Use When |
|------|----------|
| `unreal_run_console_command` | Execute any UE console command (e.g., `stat fps`, `LiveCoding.Compile`) |
| `unreal_get_output_log` | Check recent log output, filter for errors or specific categories |

### Script Execution
| Tool | Use When |
|------|----------|
| `unreal_execute_script` | Run C++ or console scripts directly in the editor |
| `unreal_cleanup_scripts` | Remove generated script files (ALWAYS do this after execute_script with cpp type) |
| `unreal_get_script_history` | Review what scripts were previously executed |

### Viewport
| Tool | Use When |
|------|----------|
| `unreal_capture_viewport` | User wants to see the editor viewport, verify visual changes, take a screenshot |

### Blueprint Tools
| Tool | Use When |
|------|----------|
| `unreal_blueprint_query` | Inspect existing blueprints — list, inspect structure, get graph details |
| `unreal_blueprint_modify` | Create blueprints, add variables/functions/nodes, connect pins |

### Animation Blueprint Tools
| Tool | Use When |
|------|----------|
| `unreal_anim_blueprint_modify` | Anything related to Animation Blueprints — state machines, transitions, conditions, animation assignment |

### Asset Tools
| Tool | Use When |
|------|----------|
| `unreal_asset_search` | Find assets by class, path, or name pattern |
| `unreal_asset` | Get asset info, save assets, set asset properties (operation-based) |

### Character Tools
| Tool | Use When |
|------|----------|
| `unreal_character` | List characters, get/set movement parameters |

### Material Tools
| Tool | Use When |
|------|----------|
| `unreal_material` | Set material properties, assign materials to meshes |

### Context System
| Tool | Use When |
|------|----------|
| `unreal_get_ue_context` | Need UE 5.7 API docs/patterns before writing code. Categories: animation, blueprint, slate, actor, assets, replication, enhanced_input, character, material, parallel_workflows |

---

## Critical Pitfalls

These are hard-won lessons. Ignoring them will break builds or produce silent failures.

### 1. execute_script Leaves Files Behind
When you call `unreal_execute_script` with `script_type: "cpp"`, it creates a `.cpp` file in `Source/MetinRemaster/Generated/UnrealClaude/`. These files get picked up by the build system and WILL break subsequent builds. **Always call `unreal_cleanup_scripts` after using cpp-type scripts.** Console-type scripts don't have this problem.

### 2. Enum/Byte Transition Conditions
`setup_transition_conditions` does NOT work for enum or byte variables. It will silently fail or produce incorrect graphs. **Always use `add_comparison_chain` instead** — it handles all variable types correctly and auto-ANDs with existing conditions.

### 3. BlendSpace Assignment on State Creation
`add_state` accepts animation parameters but does NOT reliably assign BlendSpaces. **Always use `set_state_animation` as a separate call after creating the state.** This applies to BlendSpace, BlendSpace1D, AnimSequence, and Montage types.

### 4. BlueprintReadWrite for ABP Variables
Variables used in Animation Blueprint comparison nodes MUST have `BlueprintReadWrite` access. If a variable is `ReadOnly`, the comparison node will compile but produce incorrect results at runtime.

### 5. Live Coding Conflicts
Before running a full UBT (Unreal Build Tool) build via command line, you MUST kill both `LiveCodingConsole.exe` AND `UnrealEditor.exe`. They hold locks on build artifacts. Use:
```
powershell.exe -NoProfile -Command "Stop-Process -Name LiveCodingConsole -Force -ErrorAction SilentlyContinue; Stop-Process -Name UnrealEditor -Force -ErrorAction SilentlyContinue"
```

### 6. Timeouts
MCP tools default to 30s timeout. Long operations (large script execution, asset operations) may need the async task queue. The bridge automatically uses async when available — if a tool times out, the task may still be running. Check with `unreal_task_list`.

### 7. PowerShell Dollar Signs
When calling MCP via bash `curl` commands, `$` in PowerShell expressions gets eaten by bash. Prefer using the MCP tools directly rather than curl. If you must use curl, single-quote the body.

---

## Workflow Recipes

### Recipe 1: Spawn and Configure an Actor
```
1. unreal_spawn_actor — place the actor
2. unreal_set_property — configure properties (intensity, color, etc.)
3. unreal_move_actor — position it precisely
4. unreal_capture_viewport — verify the result visually
```

### Recipe 2: Build an Animation Blueprint State Machine
```
1. unreal_anim_blueprint_modify (get_info) — check current ABP structure
2. unreal_anim_blueprint_modify (create_state_machine) — create the SM
3. unreal_anim_blueprint_modify (batch) — add multiple states at once
4. unreal_anim_blueprint_modify (set_state_animation) — assign animations PER STATE
5. unreal_anim_blueprint_modify (add_transition) — connect states
6. unreal_anim_blueprint_modify (add_comparison_chain) — set transition conditions
7. unreal_anim_blueprint_modify (connect_state_machine_to_output) — wire to output pose
8. unreal_anim_blueprint_modify (validate_blueprint) — check for compile errors
```

Use `batch` to combine multiple operations (add_state, add_transition) into a single atomic call:
```json
{
  "blueprint_path": "/Game/Characters/ABP_Character",
  "operation": "batch",
  "operations": [
    {"operation": "add_state", "state_machine": "Locomotion", "state_name": "Idle", "is_entry_state": true},
    {"operation": "add_state", "state_machine": "Locomotion", "state_name": "Walk"},
    {"operation": "add_state", "state_machine": "Locomotion", "state_name": "Run"},
    {"operation": "add_transition", "state_machine": "Locomotion", "from_state": "Idle", "to_state": "Walk"},
    {"operation": "add_transition", "state_machine": "Locomotion", "from_state": "Walk", "to_state": "Run"}
  ]
}
```

Then assign animations separately (NOT in the batch with add_state):
```json
{
  "blueprint_path": "/Game/Characters/ABP_Character",
  "operation": "set_state_animation",
  "state_machine": "Locomotion",
  "state_name": "Walk",
  "animation_type": "blendspace",
  "animation_path": "/Game/Characters/BS_Walk",
  "parameter_bindings": {"X": "Speed", "Y": "Direction"}
}
```

### Recipe 3: Debug a Visual Issue
```
1. unreal_capture_viewport — see current state
2. unreal_get_level_actors — find relevant actors
3. unreal_get_output_log (filter: "Warning|Error") — check for issues
4. Fix the issue (set_property, move_actor, etc.)
5. unreal_capture_viewport — verify the fix
```

### Recipe 4: Find and Inspect Assets
```
1. unreal_asset_search — find assets by class/path/name
2. unreal_asset (get_asset_info) — get detailed info about specific asset
3. unreal_asset (set_asset_property) — modify asset properties
4. unreal_asset (save_asset) — persist changes
```

### Recipe 5: Blueprint Creation
```
1. unreal_blueprint_modify (create) — create the blueprint
2. unreal_blueprint_modify (add_variable) — add variables
3. unreal_blueprint_modify (add_node) — add function/event nodes
4. unreal_blueprint_modify (connect_pins) — wire nodes together
5. unreal_blueprint_query (inspect) — verify the result
```

### Recipe 6: Live Coding Compile
When the user has made C++ changes and wants to hot-reload:
```
unreal_run_console_command with command: "LiveCoding.Compile"
```
Then check the output log for success/failure:
```
unreal_get_output_log with filter: "LiveCoding"
```

---

## anim_blueprint_modify Operations Quick Reference

This is the most complex tool. Here are all operations and their key parameters:

| Operation | Required Params | Notes |
|-----------|----------------|-------|
| `get_info` | blueprint_path | Overview of ABP structure |
| `get_state_machine` | blueprint_path, state_machine | Detailed SM info |
| `create_state_machine` | blueprint_path, state_machine | Creates new SM |
| `add_state` | blueprint_path, state_machine, state_name | Optional: is_entry_state |
| `remove_state` | blueprint_path, state_machine, state_name | |
| `set_entry_state` | blueprint_path, state_machine, state_name | |
| `add_transition` | blueprint_path, state_machine, from_state, to_state | |
| `remove_transition` | blueprint_path, state_machine, from_state, to_state | |
| `set_transition_duration` | blueprint_path, state_machine, from_state, to_state, duration | |
| `set_transition_priority` | blueprint_path, state_machine, from_state, to_state, priority | |
| `add_comparison_chain` | blueprint_path, state_machine, from_state, to_state, variable_name, comparison_type, compare_value | Types: Greater, Less, GreaterEqual, LessEqual, Equal, NotEqual |
| `set_state_animation` | blueprint_path, state_machine, state_name, animation_type, animation_path | Types: sequence, blendspace, blendspace1d, montage. Optional: parameter_bindings |
| `find_animations` | blueprint_path | Search compatible animations |
| `get_transition_nodes` | blueprint_path, state_machine | Optional: from_state, to_state |
| `inspect_node_pins` | blueprint_path, state_machine, from_state, to_state, node_id | |
| `set_pin_default_value` | blueprint_path, state_machine, from_state, to_state, node_id, pin_name, pin_value | |
| `connect_state_machine_to_output` | blueprint_path, state_machine | Wires SM to AnimGraph output |
| `validate_blueprint` | blueprint_path | Returns compile status |
| `batch` | blueprint_path, operations[] | Atomic multi-operation |

### Condition Node Types for Transition Graphs
| Type | Description | Use With |
|------|-------------|----------|
| `TimeRemaining` | Time left in current animation | Float comparison |
| `Greater` / `Less` / `GreaterEqual` / `LessEqual` / `Equal` / `NotEqual` | Numeric comparisons | Two float inputs |
| `And` / `Or` / `Not` | Boolean logic | Combining conditions |
| `GetVariable` | Read a blueprint variable | Feeds into comparisons |

Prefer `add_comparison_chain` over manually building condition graphs — it handles GetVariable + Comparison + Result wiring automatically and auto-ANDs with existing conditions.

---

## Async Task Queue

For long-running operations, the bridge uses an async task queue transparently. You don't usually need to manage this directly, but if you do:

| Tool | Purpose |
|------|---------|
| `unreal_task_submit` | Submit a tool execution as an async task |
| `unreal_task_status` | Check if a task is still running |
| `unreal_task_result` | Get the result of a completed task |
| `unreal_task_list` | List all pending/completed tasks |
| `unreal_task_cancel` | Cancel a running task |

The bridge automatically routes tool calls through the async queue when `MCP_ASYNC_ENABLED` is true (default). You only need to use these manually if you want to fire-and-forget operations or check on stale tasks.

---

## Tips for Effective Use

1. **Batch when possible** — The `batch` operation in `anim_blueprint_modify` is much faster than individual calls. Use it for setting up multiple states and transitions.

2. **Capture viewport for verification** — After making visual changes, always offer to capture the viewport so the user can verify. It's cheap and catches mistakes early.

3. **Use get_ue_context before writing code** — If you need to write C++ that interacts with UE APIs (via execute_script or direct code), pull the relevant context category first. This avoids API mistakes.

4. **Filter output logs** — `get_output_log` can return a lot of noise. Always use the `filter` parameter to narrow results (e.g., "Error", "Warning", "LiveCoding", "LogTemp").

5. **Check before creating** — Before creating a state machine, blueprint, or actor, check if it already exists. Use `get_info`, `blueprint_query (list)`, or `get_level_actors` first.

6. **Save after asset changes** — When modifying assets with `unreal_asset (set_asset_property)`, follow up with `unreal_asset (save_asset)` to persist changes to disk.
