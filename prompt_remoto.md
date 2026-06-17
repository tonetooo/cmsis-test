# 📋 PROMPT PARA PC REMOTO — Estado Actual + Acciones

## Estado del Proyecto Local (HERMES-A1-CMSIS)

### ✅ Completado
- Rsync merge de cmsis-test (220 archivos actualizados)
- awtas-backend checkout del repo (commit 446c1da, staged en git)
- Project structure verificado: Core/, Drivers/, algo/, vaulted_codes/, entities.json, Debug/ — todo intacto
- Backend Python: venv creado, dependencias instaladas
- .env configurado (DRIVE_FOLDER_ID, API key, etc.)
- Cloudflare tunnel anterior CAÍDO (DNS no resuelve)

### ⏳ Pendiente
- credentials.json NO está en local Mac — necesario para backend y para generar GDRIVE_TOKEN
- quectel_drive.c pendiente de revisión
- Tunnel Cloudflare nuevo por configurar

---

## 🎯 Acciones que necesito de ti

### 1. GENERAR GDRIVE_TOKEN desde la Service Account
Las credenciales están en `awtas-backend/credentials.json` (confirmado, mismo hash SHA256).
Service Account: `enviodatostest@enviodatos.iam.gserviceaccount.com`

Ejecuta este script para generar un token:

```python
from google.oauth2 import service_account
import google.auth.transport.requests
import json

SCOPES = ['https://www.googleapis.com/auth/drive.file']
creds = service_account.Credentials.from_service_account_file(
    'awtas-backend/credentials.json', scopes=SCOPES)
request = google.auth.transport.requests.Request()
creds.refresh(request)
print("GDRIVE_TOKEN:", creds.token)
```

Copia el token impreso. Lo necesito para activar la Ruta 1 (modem→Drive directo).

### 2. ENVIARME credentials.json (PC Remoto → Mac Local)
Necesito el archivo `credentials.json` para que el backend Flask funcione.
- Cópialo a: `awtas-backend/credentials.json`
- Haz lo que tengas que hacer (scp, airdrop, etc.)
- También pon una copia en `awtas-backend/backend-drive/credentials.json`

### 3. CONFIGURAR CLOUDFLARE TUNNEL
El tunnel anterior (`affected-christmas-bureau-heating.trycloudflare.com`) está caído.

**Opción A — cloudflared tunnel (recomendada):**
```bash
# Crear tunnel con nombre
cloudflared tunnel create awtas-tunnel

# Mapear al puerto 8080
# ~/.cloudflared/config.yml:
# tunnel: awtas-tunnel
# credentials-file: /root/.cloudflared/awtas-tunnel.json
# ingress:
#   - hostname: awtas.tudominio.com
#     service: http://localhost:8080
#   - service: http_status:404

# Iniciar
cloudflared tunnel run awtas-tunnel
```

**Opción B — Quick Tunnel (para test rápido):**
```bash
cloudflared tunnel --url http://localhost:8080
```

Me dices la URL que genere.

### 4. ACTUALIZAR GIT LOCAL
```bash
git pull origin master
```
Para asegurar que estamos al día.

---

## Resumen de lo que necesito de vuelta

| Item | Formato | Urgencia |
|------|---------|----------|
| GDRIVE_TOKEN | String tipo "ya29.xxx" | Alta (para credentials.h) |
| credentials.json | Archivo | Alta (para backend) |
| Tunnel URL | String URL https:// | Alta (para credentials.h y backend) |
| Confirmación git pull | OK/Error | Media |
