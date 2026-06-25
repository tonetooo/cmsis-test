#!/usr/bin/env bash
set -euo pipefail

# ==============================================================
#  OpenCode + Sisyphus — Setup automatizado para Linux + VS Studio
#  Uso: chmod +x setup.sh && ./setup.sh
#  Ejecutar desde cmsis-test/ (o pasar PROJECT_DIR como argumento)
# ==============================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log()  { echo -e "${CYAN}[setup]${NC} $1"; }
ok()   { echo -e "${GREEN}[  OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
fail() { echo -e "${RED}[FAIL]${NC} $1"; exit 1; }

# --- Config ---
OPENCODE_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/opencode"
BACKUP_DIR="${OPENCODE_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
PACK_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${1:-$(pwd)}"

echo -e "${CYAN}"
echo "  ╔═══════════════════════════════════════════╗"
echo "  ║   OpenCode Sisyphus — Setup               ║"
echo "  ║   Linux + VS Studio                       ║"
echo "  ╚═══════════════════════════════════════════╝"
echo -e "${NC}"

# --- Step 1: Verificar prerequisitos ---
log "Verificando prerequisitos..."

for cmd in node git python3; do
    if command -v $cmd &>/dev/null; then
        ok "$cmd encontrado ($($cmd --version 2>&1 | head -c 30))"
    else
        fail "$cmd no está instalado. Instálalo primero."
    fi
done

# Bun opcional pero recomendado
if command -v bun &>/dev/null; then
    ok "bun encontrado ($(bun --version))"
    PKG_MANAGER="bun"
elif command -v npm &>/dev/null; then
    warn "bun no encontrado, usando npm (más lento)"
    PKG_MANAGER="npm"
else
    fail "npm no encontrado. Instala Node.js primero."
fi

# --- Step 2: Backup de config existente ---
if [ -d "$OPENCODE_DIR" ]; then
    log "Respaldando configuración existente → $BACKUP_DIR"
    mkdir -p "$BACKUP_DIR"
    cp -r "$OPENCODE_DIR"/* "$BACKUP_DIR"/ 2>/dev/null || true
    cp -r "$OPENCODE_DIR"/.* "$BACKUP_DIR"/ 2>/dev/null || true
    ok "Backup creado en $BACKUP_DIR"
fi

# --- Step 3: Copiar configuración ---
log "Copiando configuración a $OPENCODE_DIR"
mkdir -p "$OPENCODE_DIR"
cp -r "$PACK_DIR"/* "$OPENCODE_DIR"/
cp -r "$PACK_DIR"/.* "$OPENCODE_DIR"/ 2>/dev/null || true
ok "Configuración copiada"

# --- Step 4: Instalar dependencias npm ---
log "Instalando dependencias locales..."
cd "$OPENCODE_DIR"
if [ "$PKG_MANAGER" = "bun" ]; then
    bun install 2>&1 | tail -1
else
    npm install 2>&1 | tail -3
fi
ok "Dependencias locales instaladas"

# --- Step 5: Instalar plugins npm globales ---
log "Instalando plugins globales..."
PLUGINS=(
    "oh-my-openagent"
    "opencode-mempalace"
    "opencode-pty"
    "@zenobius/opencode-skillful"
    "opencode-background-agents"
    "opencode-websearch-cited"
    "opencode-supermemory"
    "@nick-vi/opencode-type-inject"
    "@tarquinen/opencode-dcp"
    "opencode-dux"
    "@mohak34/opencode-notifier"
)

for plugin in "${PLUGINS[@]}"; do
    if [ "$PKG_MANAGER" = "bun" ]; then
        bun install -g "$plugin" 2>/dev/null && ok "$plugin" || warn "$plugin falló (no crítico)"
    else
        npm install -g "$plugin" 2>/dev/null && ok "$plugin" || warn "$plugin falló (no crítico)"
    fi
done

# --- Step 6: Instalar MCPs ---
log "Instalando MCPs locales..."

# codebase-memory-mcp
if ! command -v codebase-memory-mcp &>/dev/null; then
    mkdir -p "$HOME/.local/bin"
    warn "codebase-memory-mcp no instalado. Descárgalo manualmente:"
    echo "  curl -L https://github.com/tonetooo/cmsis-test/releases/download/latest/codebase-memory-mcp-linux -o ~/.local/bin/codebase-memory-mcp"
    echo "  chmod +x ~/.local/bin/codebase-memory-mcp"
else
    ok "codebase-memory-mcp encontrado"
fi

# mempalace
if python3 -c "import mempalace" 2>/dev/null; then
    ok "mempalace (python) instalado"
else
    warn "Instalando mempalace..."
    pip install mempalace 2>&1 | tail -1 && ok "mempalace instalado" || warn "falló mempalace (pip install mempalace manualmente)"
fi

# --- Step 7: Configurar proyecto en codebase-memory ---
log "Configurando indexación del proyecto..."
if command -v codebase-memory-mcp &>/dev/null; then
    codebase-memory-mcp add-project --path "$PROJECT_DIR" 2>/dev/null || warn "No se pudo indexar automáticamente (hacerlo desde OpenCode después)"
fi

# --- Step 8: Limpiar __MACOSX si existe ---
rm -rf "$OPENCODE_DIR"/__MACOSX 2>/dev/null || true

# --- Final ---
echo ""
echo -e "${GREEN}  ✅ Migración completada${NC}"
echo ""
echo "  Próximos pasos:"
echo "  1. Recarga VS Code (Ctrl+Shift+P → Developer: Reload Window)"
echo "  2. Abre el proyecto: cd $PROJECT_DIR"
echo "  3. En OpenCode, prueba: /quota"
echo "  4. Indexa el proyecto: (se hará automáticamente al abrir)"
echo ""
echo "  Si usas modelos gratis:"
echo "    cp ~/.config/opencode/oh-my-openagent.free.json ~/.config/opencode/oh-my-openagent.json"
echo ""
echo "  Backup de tu config anterior: $BACKUP_DIR"
