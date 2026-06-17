// token-goat bridge plugin for opencode
// Bridges opencode's plugin API to token-goat's subprocess hook protocol.
// https://github.com/DFKHelper/token-goat
import { spawnSync } from "child_process";

const TOOL_TO_TG: Record<string, string> = {
  read: "Read",
  edit: "Edit",
  apply_patch: "Edit",
  shell: "Bash",
  bash: "Bash",
  grep: "Grep",
  glob: "Glob",
  webfetch: "WebFetch",
};

// opencode uses camelCase args; token-goat expects snake_case tool_input
const ARGS_TO_TG: Record<string, Record<string, string>> = {
  read: { filePath: "file_path", offset: "offset", limit: "limit" },
  edit: { filePath: "file_path", oldString: "old_string", newString: "new_string", replaceAll: "replace_all" },
  apply_patch: { patchText: "patch_text" },
  shell: { command: "command" },
  bash: { command: "command" },
  grep: { pattern: "pattern", path: "path", include: "glob" },
  glob: { pattern: "pattern", path: "path" },
  webfetch: { url: "url", prompt: "prompt" },
};

// Post-call hook event per token-goat tool name (mirrors openclaw's POST_HOOK table)
const POST_HOOK: Record<string, string> = {
  Read: "post-read",
  Grep: "post-read",
  Glob: "post-read",
  Bash: "post-bash",
  WebFetch: "post-fetch",
  Edit: "post-edit",
  Write: "post-edit",
};

const _seenSessions = new Set<string>();

function reverseArgMap(tool: string): Record<string, string> {
  const fwd = ARGS_TO_TG[tool] ?? {};
  return Object.fromEntries(Object.entries(fwd).map(([cc, tg]) => [tg, cc]));
}

function callHook(event: string, payload: Record<string, unknown>): Record<string, unknown> | null {
  try {
    const r = spawnSync("token-goat", ["hook", event], {
      input: JSON.stringify(payload),
      encoding: "utf8",
      timeout: 5000,
      windowsHide: true,
    });
    const out = r.stdout?.trim();
    if (!out) return null;
    return JSON.parse(out) as Record<string, unknown>;
  } catch {
    return null;
  }
}

export const server = async (pluginInput: { directory: string }) => {
  const cwd = pluginInput.directory;

  return {
    "tool.execute.before": async (
      input: { tool: string; sessionID: string; callID: string },
      output: { args: Record<string, unknown> },
    ) => {
      const tgTool = TOOL_TO_TG[input.tool];
      if (!tgTool) return;

      if (!_seenSessions.has(input.sessionID)) {
        _seenSessions.add(input.sessionID);
        callHook("session-start", { session_id: input.sessionID, cwd });
      }

      const argMap = ARGS_TO_TG[input.tool] ?? {};
      const toolInput: Record<string, unknown> = {};
      for (const [ccKey, tgKey] of Object.entries(argMap)) {
        if (output.args[ccKey] !== undefined) toolInput[tgKey] = output.args[ccKey];
      }

      const hookEvent = tgTool === "WebFetch" ? "pre-fetch" : "pre-read";
      const resp = callHook(hookEvent, {
        session_id: input.sessionID,
        tool_name: tgTool,
        tool_input: toolInput,
        cwd,
      });
      if (!resp) return;

      const hso = resp["hookSpecificOutput"] as Record<string, unknown> | undefined;
      const updated = hso?.["updatedInput"] as Record<string, unknown> | undefined;
      if (updated) {
        const rev = reverseArgMap(input.tool);
        for (const [tgKey, val] of Object.entries(updated)) {
          output.args[rev[tgKey] ?? tgKey] = val;
        }
      }
    },

    "tool.execute.after": async (input: {
      tool: string;
      sessionID: string;
      callID: string;
      args: Record<string, unknown>;
    }) => {
      const tgTool = TOOL_TO_TG[input.tool];
      if (!tgTool) return;

      const argMap = ARGS_TO_TG[input.tool] ?? {};
      const toolInput: Record<string, unknown> = {};
      for (const [ccKey, tgKey] of Object.entries(argMap)) {
        if (input.args[ccKey] !== undefined) toolInput[tgKey] = input.args[ccKey];
      }

      callHook(POST_HOOK[tgTool] ?? "post-read", {
        session_id: input.sessionID,
        tool_name: tgTool,
        tool_input: toolInput,
        cwd,
      });
    },

    "experimental.session.compacting": async (
      input: { sessionID: string },
      output: { context: string[] },
    ) => {
      const resp = callHook("pre-compact", { session_id: input.sessionID, trigger: "auto" });
      const manifest = resp?.["systemMessage"] as string | undefined;
      if (manifest) output.context.push(manifest);
    },
  };
};
