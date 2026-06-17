#!/usr/bin/env bash
set -e

PORT="${PORT:-8080}"
for p in $(seq "$PORT" 8099); do
  if ! lsof -Pi :"$p" -sTCP:LISTEN -t >/dev/null; then
    PORT="$p"
    break
  fi
done

export DEVICE_API_KEY="LIND2026ANTONIO"
export DRIVE_FOLDER_ID="1iulcI1atbQN6lAs-lnjq9w9npcfWOX9N"
export PORT="$PORT"

nohup bash -c '
  while true; do
    python main.py
    echo "[BACKEND] main.py exited, restarting in 2s..." >> backend.log
    sleep 2
  done
' > backend.log 2>&1 &

sleep 1
: > tunnel.log

# NUEVO: Subir configuración al iniciar (borrar anterior + crear nueva)
echo "[START] Updating AWTAS_CONFIG.TXT in Drive..."
# Esperar a que el backend esté listo (máximo 10 intentos)
for i in $(seq 1 10); do
  if curl -s "http://localhost:$PORT/config" >/dev/null; then
     echo "[START] Backend ready. Uploading config..."
     # 1. Borrar configuración existente
     curl -s -X POST "http://localhost:$PORT/config/delete?name=AWTAS_CONFIG.TXT&key=$DEVICE_API_KEY"
     # 2. Subir nueva configuración desde archivo local
     curl -s -X POST "http://localhost:$PORT/config/init?name=AWTAS_CONFIG.TXT&key=$DEVICE_API_KEY"
     echo "[START] Config updated."
     break
  fi
  sleep 2
done &

if [ -n "$CLOUDFLARE_TUNNEL_NAME" ]; then
  nohup cloudflared tunnel run "$CLOUDFLARE_TUNNEL_NAME" > tunnel.log 2>&1 &
  if [ -n "$BACKEND_FIXED_URL" ]; then
    TS=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    printf "%s\nTIMESTAMP=%s\n" "$BACKEND_FIXED_URL" "$TS" > tunnel_url.txt
  fi
else
  nohup cloudflared tunnel --url "http://localhost:$PORT" > tunnel.log 2>&1 &
  for i in $(seq 1 15); do
    URL=$(grep -m1 -o "https://[A-Za-z0-9.-]*\.trycloudflare\.com" tunnel.log || true)
    if [ -n "$URL" ]; then
      TS=$(date +"%Y-%m-%d %H:%M:%S %Z")
      printf "URL: %s\nGENERATED_AT: %s\n" "$URL" "$TS" > tunnel_url.txt
      
      # NUEVO: Actualizar credentials.h automáticamente
      echo "[START] Updating firmware credentials..."
      python3 update_credentials.py "$URL"
      
      break
    fi
    sleep 1
  done
fi
