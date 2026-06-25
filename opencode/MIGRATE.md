# Migración Plug & Play — OpenCode + Sisyphus

Guía para migrar desde una instancia limpia de OpenCode en **Linux + VS Studio** (VS Code + OpenCode extension) a la configuración completa de Sisyphus con todos los plugins, MCPs y agentes.

---

## Prerequisitos

- **OpenCode extension** instalada en VS Code / VS Studio
- **Node.js >= 18** (para bun: `curl -fsSL https://bun.sh/install | bash`)
- **Python >= 3.10** (para mempalace MCP)
- **Git**

## Quick Start (1 min)

```bash
# 1. Clonar el repo
git clone https://github.com/tonetooo/cmsis-test.git
cd cmsis-test

# 2. Ejecutar setup automatizado
chmod +x opencode/setup.sh
./opencode/setup.sh

# 3. Recargar VS Code → OpenCode listo
```

---

## Paso a Paso

### Paso 1: Backup de tu config actual (opcional)

```bash
mv ~/.config/opencode ~/.config/opencode.backup.$(date +%Y%m%d)
```

### Paso 2: Copiar la configuración

```bash
# Crear directorio de configuración
mkdir -p ~/.config/opencode

# Copiar todo el pack
cp -r opencode/* ~/.config/opencode/
cp -r opencode/.* ~/.config/opencode/ 2>/dev/null || true
```

### Paso 3: Instalar dependencias

```bash
cd ~/.config/opencode
bun install    # plugins npm
```

### Paso 4: Instalar MCPs locales

```bash
# codebase-memory-mcp (análisis estructural del código)
curl -L https://github.com/tonetooo/cmsis-test/releases/download/latest/codebase-memory-mcp-linux -o ~/.local/bin/codebase-memory-mcp
chmod +x ~/.local/bin/codebase-memory-mcp

# mempalace (memoria persistente entre sesiones)
pip install mempalace
```

### Paso 5: Instalar plugins npm globales

```bash
# oh-my-openagent (orquestación de agentes)
bun install -g oh-my-openagent

# opencode-mempalace (memoria persistente)
bun install -g opencode-mempalace

# opencode-pty (gestión de procesos)
bun install -g opencode-pty

# opencode-skillful (sistema de skills)
bun install -g @zenobius/opencode-skillful

# opencode-background-agents (tareas asíncronas)
bun install -g opencode-background-agents

# opencode-websearch-cited (búsqueda web)
bun install -g opencode-websearch-cited

# opencode-supermemory (memoria de sesiones)
bun install -g opencode-supermemory

# @nick-vi/opencode-type-inject (type safety)
bun install -g @nick-vi/opencode-type-inject

# DCP (Dynamic Context Pruning)
bun install -g @tarquinen/opencode-dcp

# opencode-dux (multi-agent orchestration)
bun install -g opencode-dux

# opencode-notifier (notificaciones desktop)
bun install -g @mohak34/opencode-notifier
```

### Paso 6: Indexar el proyecto en codebase-memory

```bash
# Abrir cmsis-test/ en VS Code
# En OpenCode, ejecutar:
codebase-memory-mcp index_repository repo_path=/home/usuario/cmsis-test mode=fast
```

### Paso 7: Verificar instalación

En OpenCode, probar:

```
/quota                          # ver modelos disponibles
/skill                          # listar skills cargados
"diagnóstico repo"              # probar macro (funciona en cmsis-test/)
```

---

## Archivos del Pack

```
opencode/
├── opencode.json               # Plugins + MCPs (entry point)
├── opencode.jsonc              # Provider Ollama local
├── oh-my-openagent.json        # Agentes Sisyphus (modelos pago)
├── oh-my-openagent.paid.json   # Backup pago
├── oh-my-openagent.free.json   # Fallback gratuito
├── dcp.jsonc                   # Dynamic Context Pruning config
├── tui.json                    # TUI config (con Dux UI)
├── raven-config.json           # Raven: routing grep/glob/websearch/bash
├── opencode-notifier.json      # Notificaciones desktop (deshabilitadas por defecto)
├── AGENTS.md                   # Documentación de herramientas del agente
├── package.json                # Dependencias npm locales
├── MIGRATE.md                  # Esta guía
├── setup.sh                    # Script de instalación automatizada
├── README.md                   # Resumen del pack
├── plugins/
│   ├── model-router/           # Auto-fallback entre modelos
│   │   ├── index.js
│   │   ├── package.json
│   │   └── README.md
│   └── token-goat.ts           # Token tracking bridge
```

---

## Modelos Disponibles

| Archivo | Propósito | Modelos |
|---------|-----------|---------|
| `oh-my-openagent.json` | Principal | Claude Sonnet 4-6 (pago) |
| `oh-my-openagent.free.json` | Fallback | Big Pickle + DeepSeek flash (gratis) |
| `oh-my-openagent.paid.json` | Backup pago | Idéntico al principal |

Para cambiar a gratis:
```bash
cp ~/.config/opencode/oh-my-openagent.free.json ~/.config/opencode/oh-my-openagent.json
```

Para recargar en OpenCode: `/switch-to-free` o `/switch-to-paid`

---

## Macros Disponibles

| Comando | Acción |
|---------|--------|
| `diagnóstico repo` | git status + log + diff + remote |
| `subir a drive` | Upload a Google Drive vía service account |
| `sincronizar local` | Coordinar con el agente en Mac local |
| `dossier v2` | Abrir el dossier comercial HERMES-A1 |
| `commit+ppush` | git add → commit → push |
| `tunnel check` | Verificar URL del Cloudflare tunnel |
| `upload test` | Probar pipeline de upload end-to-end |

---

## Solución de Problemas

### "Plugin X not found"
```bash
bun install -g opencode-X
```

### "codebase-memory-mcp: command not found"
```bash
# Descargar el binario para tu arquitectura
curl -L https://github.com/.../codebase-memory-mcp-linux -o ~/.local/bin/codebase-memory-mcp
chmod +x ~/.local/bin/codebase-memory-mcp
```

### "SnoreToast" error en Linux
_no aplica_ — Linux usa notify-send. Si aparecen errores de notificación:
```bash
# Editar ~/.config/opencode/opencode-notifier.json y poner:
{ "notification": false, "sound": false, "bell": false }
```

### "mempalace connection error"
El MCP de mempalace a veces tarda en inicializar. No es crítico — reintentar o esperar.

### "API key not configured"
Los modelos de Claude/Gemini/DeepSeek requieren API keys configuradas en OpenCode:
- Claude: `ANTHROPIC_API_KEY`
- Gemini: `GEMINI_API_KEY`  
- DeepSeek: `DEEPSEEK_API_KEY`

Configurar en VS Code → settings → OpenCode, o variables de entorno.

---

## Estructura del Proyecto (post-migración)

```
workspace/
├── cmsis-test/                # Repo principal (firmware STM32 + backend)
│   ├── Core/                  # Código fuente STM32 (intocable)
│   ├── awtas-backend/         # Flask API + Google Drive
│   ├── opencode/              # Config OpenCode (este pack)
│   ├── tools/                 # Scripts de utilidad
│   └── docs/                  # Documentación
├── awtas-backend/             # Backend activo (fuera del repo)
└── AGENTS.md                  # Contexto raíz del workspace
```
