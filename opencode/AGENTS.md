<!-- codebase-memory-mcp:start -->
# Codebase Knowledge Graph (codebase-memory-mcp)

This project uses codebase-memory-mcp to maintain a knowledge graph of the codebase.
ALWAYS prefer MCP graph tools over grep/glob/file-search for code discovery.

## Priority Order
1. `search_graph` — find functions, classes, routes, variables by pattern
2. `trace_path` — trace who calls a function or what it calls
3. `get_code_snippet` — read specific function/class source code
4. `query_graph` — run Cypher queries for complex patterns
5. `get_architecture` — high-level project summary

## When to fall back to grep/glob
- Searching for string literals, error messages, config values
- Searching non-code files (Dockerfiles, shell scripts, configs)
- When MCP tools return insufficient results

## Examples
- Find a handler: `search_graph(name_pattern=".*OrderHandler.*")`
- Who calls it: `trace_path(function_name="OrderHandler", direction="inbound")`
- Read source: `get_code_snippet(qualified_name="pkg/orders.OrderHandler")"
<!-- codebase-memory-mcp:end -->

<!-- opencode-mempalace:start -->
# MemPalace Memory System

I have persistent project-scoped memory via opencode-mempalace. Use these tools:
- `mempalace_status` — check current palace state
- `mempalace_search` — semantic memory search across sessions
- `mempalace_kg_query` — knowledge graph queries
- `mempalace_diary_read/write` — session journaling
- `mempalace_add_drawer` — store specific memories

Memories are project-scoped (per-directory). Use wakeUp() context on session start.
<!-- opencode-mempalace:end -->

<!-- opencode-pty:start -->
# PTY Management

I can manage background processes via opencode-pty:
- `pty_spawn` — start background process (dev servers, watchers, etc.)
- `pty_write` — send input (keystrokes, Ctrl+C)
- `pty_read` — read output with regex filtering
- `pty_list` — list active sessions
- `pty_kill` — terminate a session
<!-- opencode-pty:end -->

<!-- opencode-skillful:start -->
# Skills System

I have on-demand skills via @zenobius/opencode-skillful:
- `skill_find <keywords>` — discover relevant skills
- `skill_use <skill_name>` — load a skill into context
- `skill_resource <skill_name> <path>` — read skill reference material

Available skills are in ~/.config/opencode/skills/
<!-- opencode-skillful:end -->

<!-- opencode-websearch:start -->
# Web Search with Citations

I have websearch_cited tool for searching the web with citations.
Use it for: current events, documentation lookups, troubleshooting.
<!-- opencode-websearch:end -->

<!-- task-type-detection:start -->
# Task Type Detection & Model Selection

**AUTOMATIC ROUTING ENABLED**: Plugin `model-router` intercepts messages and routes to appropriate model.

## How It Works

1. **Plugin intercepts** every message via `chat.message` hook
2. **Classifies task type** using regex patterns
3. **Modifies model** via `chat.params` hook before LLM call

## Classification Rules

### HIGH-QUALITY (claude-3.5-sonnet)
**Patterns**: implement, add, create, fix, refactor, debug, review, architect, multi-file, complex, architecture

**Examples**:
- "Implementa DMA para SPI2"
- "Fix the memory leak in sensor_task"
- "Refactor modem_task to use retry logic"

### LOW-CONSUMPTION (claude-3-haiku)
**Patterns**: what, how, why, explain, compare, difference, when, where, concept, definition, overview

**Examples**:
- "¿Qué es un MCP?"
- "How does FreeRTOS scheduler work?"
- "Compare CMSIS v1 vs v2"

## Manual Override

Use explicit commands to force routing:
- `/qa question` → forces claude-3-haiku
- `/deep implementation` → forces claude-3.5-sonnet

## Plugin Location

`~/.config/opencode/plugins/model-router/`

## Fallback

If classification fails or no pattern matches → defaults to claude-3.5-sonnet (high quality)

## Model Availability
- **Primary**: claude-3.5-sonnet (high quality), claude-3-haiku (low consumption)
- **Fallback**: If Claude unavailable, use gpt-4o / gpt-4o-mini
- **Avoid**: gemini-2.5-pro/flash (saturated, unreliable)
<!-- task-type-detection:end -->
