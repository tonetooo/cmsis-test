// model-router — Auto-fallback model router for OpenCode
// Detects quota/provider errors and auto-switches oh-my-openagent.json
// between paid (opencode/claude-*) and free (opencode/big-pickle, etc) models.
import { tool } from "@opencode-ai/plugin/tool";
import { join } from "node:path";
import { writeFileSync, readFileSync, existsSync, copyFileSync, mkdirSync } from "node:fs";

const CONFIG_DIR = join(process.env.USERPROFILE || "C:\\Users\\toneto", ".config", "opencode");
const HEALTH_FILE = join(CONFIG_DIR, ".model-health.json");
const ACTIVE_CONFIG = join(CONFIG_DIR, "oh-my-openagent.json");
const BACKUP_CONFIG = join(CONFIG_DIR, "oh-my-openagent.paid.json");
const FREE_CONFIG = join(CONFIG_DIR, "oh-my-openagent.free.json");

// ──────────────────────────────────────────────
// Persistent health state
// ──────────────────────────────────────────────
function loadHealth() {
  try {
    if (existsSync(HEALTH_FILE)) {
      return JSON.parse(readFileSync(HEALTH_FILE, "utf-8"));
    }
  } catch (e) {
    console.log(`[model-router] Could not read health file: ${e.message}`);
  }
  return { mode: "paid", providers: {}, updatedAt: Date.now() };
}

function saveHealth(state) {
  try {
    state.updatedAt = Date.now();
    writeFileSync(HEALTH_FILE, JSON.stringify(state, null, 2), "utf-8");
  } catch (e) {
    console.log(`[model-router] Could not write health file: ${e.message}`);
  }
}

// ──────────────────────────────────────────────
// Config switching
// ──────────────────────────────────────────────
function switchToFree() {
  if (!existsSync(FREE_CONFIG)) {
    console.log(`[model-router] ⚠️ Free config not found at ${FREE_CONFIG}`);
    return false;
  }
  try {
    copyFileSync(FREE_CONFIG, ACTIVE_CONFIG);
    console.log(`[model-router] ✅ Switched to FREE model config`);
    return true;
  } catch (e) {
    console.log(`[model-router] ❌ Failed to switch to free: ${e.message}`);
    return false;
  }
}

function switchToPaid() {
  if (!existsSync(BACKUP_CONFIG)) {
    console.log(`[model-router] ⚠️ Paid config backup not found at ${BACKUP_CONFIG}`);
    return false;
  }
  try {
    copyFileSync(BACKUP_CONFIG, ACTIVE_CONFIG);
    console.log(`[model-router] ✅ Switched to PAID model config`);
    return true;
  } catch (e) {
    console.log(`[model-router] ❌ Failed to switch to paid: ${e.message}`);
    return false;
  }
}

function getCurrentMode() {
  const health = loadHealth();
  return health.mode || "paid";
}

// ──────────────────────────────────────────────
// Error classification
// ──────────────────────────────────────────────
function isQuotaError(error) {
  if (!error) return false;
  const msg = typeof error === "string" ? error : error.message || JSON.stringify(error);
  const lower = msg.toLowerCase();
  return (
    /\bquota\b/.test(lower) ||
    /\brate.?limit\b/.test(lower) ||
    /\binsufficient.?quota\b/.test(lower) ||
    /\bexhausted\b/.test(lower) ||
    /\btoo many requests\b/.test(lower) ||
    /\b429\b/.test(lower) ||
    /\bcredits?\s*(exhausted|insufficient|ran out)\b/.test(lower) ||
    /\bapi_key.?exhausted\b/.test(lower) ||
    /\bpayment.?required\b/.test(lower) ||
    /(cuota|cupo|límite|límite de velocidad)\s*(excedido|agotado|insuficiente)/i.test(lower)
  );
}

function isModelError(error) {
  if (!error) return false;
  const msg = typeof error === "string" ? error : error.message || JSON.stringify(error);
  const lower = msg.toLowerCase();
  return /\bmodel not found\b/.test(lower) || /\bdid you mean\b/.test(lower) || /\bmodel.*unavailable\b/.test(lower);
}

function extractProviderFromError(error) {
  if (!error) return null;
  const msg = typeof error === "string" ? error : error.message || JSON.stringify(error);
  if (msg.includes("opencode-go")) return "opencode-go";
  if (msg.includes("google/")) return "google";
  if (msg.includes("openai/")) return "openai";
  if (msg.includes("anthropic/")) return "anthropic";
  if (msg.includes("opencode/")) return "opencode";
  return null;
}

function getModelFromError(error) {
  if (!error) return null;
  const msg = typeof error === "string" ? error : error.message || JSON.stringify(error);
  // Extract model name from error like "model 'opencode/claude-sonnet-4-6' not found"
  const modelMatch = msg.match(/['"]?(\w+\/[\w.-]+)['"]?/);
  return modelMatch ? modelMatch[1] : null;
}

// ──────────────────────────────────────────────
// Plugin
// ──────────────────────────────────────────────
const ModelRouterPlugin = async (ctx) => {
  let health = loadHealth();
  console.log(`[model-router] Plugin loaded. Mode: ${health.mode}`);
  if (health.mode === "free") {
    console.log(`[model-router] 🔄 Running in FREE mode — paid models may be available again later`);
  }

  return {
    // ── Event hook: detect session errors from API calls ──
    event: async (input) => {
      const ev = input.event;
      if (!ev || ev.type !== "session.error") return;

      const err = ev.properties?.error;
      if (!err) return;

      const errMsg = typeof err === "string" ? err : err.message || JSON.stringify(err);
      const shortErr = errMsg.substring(0, 300);
      console.log(`[model-router] Session error detected: ${shortErr}`);

      const provider = extractProviderFromError(err) || "opencode";
      const model = getModelFromError(err);

      // Track the error in health state
      if (!health.providers[provider]) {
        health.providers[provider] = { failures: 0, healthy: true, lastError: "" };
      }
      health.providers[provider].failures = (health.providers[provider].failures || 0) + 1;
      health.providers[provider].lastError = shortErr;

      // Google gets permanently banned (user request)
      if (provider === "google") {
        health.providers[provider].healthy = false;
        health.providers[provider].permanent = true;
        console.log(`[model-router] ⛔ Google permanently banned as requested`);
      }

      // Critical error on 'opencode' provider → auto-fallback to free
      const isOjaiError = isQuotaError(err) || isModelError(err);
      if (isOjaiError && health.mode === "paid" && provider !== "google") {
        console.log(`[model-router] ⛔ Quota/model error on ${provider}${model ? ` (${model})` : ""}. Auto-switching to FREE config...`);

        const switched = switchToFree();
        if (switched) {
          health.mode = "free";
          health.providers[provider].healthy = false;
          saveHealth(health);

          console.log(`[model-router] ✅ Auto-fallback complete. Future sessions will use free models.`);
          console.log(`[model-router] 💡 Run /switch-to-paid to restore paid models when quota recovers.`);
        }
      } else {
        // Non-critical or already free — just track
        saveHealth(health);
      }
    },

    // ── Chat message hook: log model being used ──
    "chat.message": async (input, output) => {
      if (input.model) {
        const modelStr = `${input.model.providerID}/${input.model.modelID}`;
        console.log(`[model-router] Message using model: ${modelStr}`);
      }
    },

    // ── Tools ──
    tool: {
      // /quota — show provider health status + current mode
      quota: tool({
        description: "Check model provider health, current mode (paid/free), and error tracking status",
        args: {},
        async execute(args, context) {
          const h = loadHealth();
          const lines = ["## Model Router Status\n"];

          // Current mode
          const modeEmoji = h.mode === "paid" ? "💎" : "🆓";
          lines.push(`${modeEmoji} **Current mode**: ${h.mode.toUpperCase()}`);
          lines.push(`   Config: ${h.mode === "paid" ? "Paid models (sonnet-4-6, opus-4-7, deepseek-v4-flash)" : "Free models (big-pickle, deepseek-v4-flash-free)"}`);
          lines.push("");

          // Provider health
          const providerEntries = Object.entries(h.providers);
          if (providerEntries.length === 0) {
            lines.push("No errors tracked — all providers healthy.\n");
          }
          for (const [provider, info] of providerEntries) {
            const healthy = info.healthy !== false;
            if (healthy) {
              lines.push(`✅ **${provider}**: Healthy`);
            } else {
              const tag = info.permanent ? "🔴" : "⛔";
              const extra = info.permanent ? " (permanent ban)" : ` (${info.failures} failure(s))`;
              lines.push(`${tag} **${provider}**: Unhealthy${extra}`);
              if (info.lastError) {
                lines.push(`   Last: ${info.lastError.substring(0, 150)}`);
              }
            }
          }

          lines.push(`\n**Commands:**`);
          lines.push(`- \`/switch-to-free\` — force free models now`);
          lines.push(`- \`/switch-to-paid\` — restore paid models (takes effect next session)`);
          lines.push(`- \`/reset-models\` — reset all provider health tracking`);
          return lines.join("\n");
        },
      }),

      // /models — show available models
      models: tool({
        description: "Show all available models on this system",
        args: {},
        async execute(args, context) {
          return "Run `opencode models` in terminal to see all available models.";
        },
      }),

      // /switch-to-free — manually force free models
      "switch-to-free": tool({
        description: "Switch oh-my-openagent.json to free models (big-pickle, deepseek-v4-flash-free). Takes effect next session.",
        args: {},
        async execute(args, context) {
          const ok = switchToFree();
          if (ok) {
            health = loadHealth();
            health.mode = "free";
            saveHealth(health);
            return "✅ Switched to **FREE** model config.\nNext session will use free models (big-pickle, deepseek-v4-flash-free).\nRun `/switch-to-paid` to restore paid models anytime.";
          }
          return "❌ Failed to switch to free config. Is `oh-my-openagent.free.json` present?";
        },
      }),

      // /switch-to-paid — restore paid models
      "switch-to-paid": tool({
        description: "Restore oh-my-openagent.json to paid models (sonnet-4-6, opus-4-7, deepseek-v4-flash). Takes effect next session.",
        args: {},
        async execute(args, context) {
          const ok = switchToPaid();
          if (ok) {
            health = loadHealth();
            health.mode = "paid";
            saveHealth(health);
            return "✅ Switched to **PAID** model config.\nNext session will use paid models.\nRun `/switch-to-free` to go back to free models.";
          }
          return "❌ Failed to switch to paid config. Is `oh-my-openagent.paid.json` present?";
        },
      }),

      // /reset-models — reset all health tracking
      "reset-models": tool({
        description: "Reset all provider health tracking and switch back to paid mode",
        args: {},
        async execute(args, context) {
          health = { mode: "paid", providers: {}, updatedAt: Date.now() };
          saveHealth(health);
          switchToPaid();
          return "✅ All provider health tracking reset. Switched to PAID mode.\nRun `/quota` to verify status.";
        },
      }),
    },
  };
};

export default ModelRouterPlugin;
