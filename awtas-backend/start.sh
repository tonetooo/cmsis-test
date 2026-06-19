#!/usr/bin/env bash

###########################################################
# AWTAS Backend - Script de inicio
# Maneja la creación del entorno, instalación de 
# dependencias y ejecución del servidor
###########################################################

set -e

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Función para imprimir mensajes
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

log_info "Iniciando AWTAS Backend..."
log_info "Directorio: $SCRIPT_DIR"

# Verificar Python
if ! command -v python3 &> /dev/null; then
    log_error "Python 3 no está instalado"
    exit 1
fi

log_info "Python version: $(python3 --version)"

# Crear entorno virtual si no existe
if [ ! -d ".venv" ]; then
    log_info "Creando entorno virtual..."
    python3 -m venv .venv
fi

# Activar entorno virtual
log_info "Activando entorno virtual..."
source .venv/bin/activate

# Instalar/actualizar dependencias
log_info "Instalando dependencias..."
pip install --upgrade pip
pip install -r requirements.txt

# Verificar archivos críticos
if [ ! -f "credentials.json" ]; then
    log_warn "credentials.json no encontrado"
    log_warn "Descárgalo de Google Cloud Console y colócalo en esta carpeta"
fi

if [ ! -f ".env" ]; then
    log_warn ".env no encontrado"
    log_info "Creando .env desde .env.example..."
    cp .env.example .env
    log_warn "Actualiza .env con tus valores"
fi

# Configurar puerto
PORT="${PORT:-8080}"

# Buscar puerto disponible si está en uso
for p in $(seq "$PORT" 8099); do
    if ! lsof -Pi :"$p" -sTCP:LISTEN -t >/dev/null 2>&1; then
        PORT="$p"
        break
    fi
done

log_info "Usando puerto: $PORT"

# Configurar variables de entorno
export PORT="$PORT"
export PYTHONUNBUFFERED=1

# Cargar variables de .env si existen
if [ -f ".env" ]; then
    log_info "Cargando variables de .env..."
    set -a
    source .env
    set +a
fi

# Crear directorio de logs si no existe
mkdir -p logs

# Limpiar logs anteriores
> logs/backend.log
> logs/tunnel.log

# Iniciar el backend
log_info "Iniciando servidor Flask..."
log_info "Acceder a: http://localhost:$PORT"
log_info "Dashboard: http://localhost:$PORT/dashboard"
log_info ""
log_info "Presiona Ctrl+C para detener"
log_info ""

# Ejecutar con autorestart en desarrollo
if [ "$FLASK_ENV" = "production" ]; then
    python app.py
else
    nohup bash -c '
        while true; do
            python app.py
            echo "[$(date)] main.py exited, restarting in 2s..." >> logs/backend.log
            sleep 2
        done
    ' > logs/backend.log 2>&1 &
    
    sleep 1
    
    # Si hay Cloudflare tunnel configurado, iniciarlo
    if [ -n "$CLOUDFLARE_TUNNEL_NAME" ]; then
        log_info "Iniciando Cloudflare Tunnel: $CLOUDFLARE_TUNNEL_NAME"
        nohup cloudflared tunnel run "$CLOUDFLARE_TUNNEL_NAME" > logs/tunnel.log 2>&1 &
        
        if [ -n "$BACKEND_FIXED_URL" ]; then
            TS=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
            printf "%s\nTIMESTAMP=%s\n" "$BACKEND_FIXED_URL" "$TS" > tunnel_url.txt
            log_info "URL Tunnel guardada en tunnel_url.txt"
        fi
    else
        log_info "Iniciando tunnel ad-hoc con trycloudflare.com..."
        log_info "No se requiere cuenta de Cloudflare"
        nohup cloudflared tunnel --url "http://localhost:$PORT" > logs/tunnel.log 2>&1 &
        
        TUNNEL_URL=""
        for i in $(seq 1 15); do
            TUNNEL_URL=$(grep -m1 -o "https://[A-Za-z0-9.-]*\.trycloudflare\.com" logs/tunnel.log || true)
            if [ -n "$TUNNEL_URL" ]; then
                TS=$(date +"%Y-%m-%d %H:%M:%S %Z")
                printf "URL: %s\nGENERATED_AT: %s\n" "$TUNNEL_URL" "$TS" > tunnel_url.txt
                log_info "Túnel Cloudflare activo: $TUNNEL_URL"
                log_info "URL guardada en tunnel_url.txt"
                
                log_info "Actualizando credentials.h con nueva URL..."
                python3 backend-drive/update_credentials.py "$TUNNEL_URL"
                
                break
            fi
            sleep 1
        done
        
        if [ -z "$TUNNEL_URL" ]; then
            log_warn "No se pudo obtener la URL del túnel en 15 segundos"
            log_warn "Revisa logs/tunnel.log manualmente"
        fi
    fi
    
    log_info "Backend iniciado en background"
    log_info "Ver logs: tail -f logs/backend.log"
    
    # Mantener el script activo
    wait
fi
