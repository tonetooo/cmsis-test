# OpenCode Config Pack — Sisyphus

Configuración completa de OpenCode con plugins, MCPs, agentes y skills para el proyecto HERMES-A1 (AWTAS).

## Contenido

| Archivo | Descripción |
|---------|-------------|
| `opencode.json` | Plugins y MCPs (entry point) |
| `opencode.jsonc` | Provider Ollama local |
| `oh-my-openagent.json` | Definición de agentes Sisyphus |
| `oh-my-openagent.paid.json` | Backup config paga |
| `oh-my-openagent.free.json` | Config gratuita (fallback) |
| `dcp.jsonc` | Dynamic Context Pruning |
| `tui.json` | TUI config |
| `raven-config.json` | Raven: routing grep/glob/websearch/bash |
| `opencode-notifier.json` | Notificaciones desktop (deshabilitadas) |
| `AGENTS.md` | Documentación de tools del agente |
| `package.json` | Dependencia `@opencode-ai/plugin` |
| `MIGRATE.md` | **Guía de migración Linux + VS Studio** |
| `setup.sh` | **Script de instalación automatizada** |
| `plugins/model-router/` | Auto-fallback quota/router |
| `plugins/token-goat.ts` | Token tracking bridge |

## Instalación Rápida

```bash
# Desde el directorio del proyecto
chmod +x opencode/setup.sh
./opencode/setup.sh
```

## Migración Manual

Ver `MIGRATE.md` para instrucciones detalladas paso a paso.

## Modelos

- `oh-my-openagent.json` → Claude Sonnet 4-6 (pago, default)
- `oh-my-openagent.free.json` → Big Pickle + DeepSeek flash (gratis)
- Cambiar con `/switch-to-free` o `/switch-to-paid`

## Notas

- `credentials.json` NO está incluido (contiene clave privada)
- `codebase-memory-mcp` requiere binario Linux (ver setup.sh)
- El pack se genera desde `cmsis-test/opencode/`
