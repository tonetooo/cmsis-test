# OpenCode Config — HERMES-A1

Configuración completa de OpenCode para el proyecto HERMES-A1 (AWTAS).

## Instalación en el PC Local (Mac)

```bash
# 1. Copiar todos los archivos a ~/.config/opencode/
cp -r opencode/* ~/.config/opencode/

# 2. Instalar dependencias
cd ~/.config/opencode && bun install

# 3. Copiar plugins locales
# (ya se copiaron en el paso 1)

# 4. Verificar plugins en opencode.json
```

## Contenido

| Archivo | Descripción |
|---------|-------------|
| `opencode.json` | Plugins y MCPs |
| `opencode.jsonc` | Provider Ollama local |
| `oh-my-openagent.json` | Definición de agentes Sisyphus |
| `oh-my-openagent.paid.json` | Backup config paga |
| `oh-my-openagent.free.json` | Config gratuita (fallback) |
| `dcp.jsonc` | Dynamic Context Pruning |
| `tui.json` | TUI config |
| `raven-config.json` | Raven: routing grep/glob/websearch/bash |
| `AGENTS.md` | Documentación de tools del agente |
| `package.json` | Dependencia `@opencode-ai/plugin` |
| `plugins/model-router/` | Auto-fallback quota/router |
| `plugins/token-goat.ts` | Token tracking bridge |

> **Nota:** `credentials.json` NO está incluido (contiene clave privada).  
> Copiarlo manualmente a `awtas-backend/credentials.json` desde el PC remoto.
