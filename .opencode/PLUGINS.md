# HERMES-A1 — OpenCode Plugins & Config

> **Config files incluidos en el repo:**  
> `opencode/` contiene TODOS los archivos de configuración de OpenCode listos para copiar a `~/.config/opencode/`.  
> Ver `opencode/README.md` para instrucciones de instalación en el PC local.

## Contexto

Este repositorio (`cmsis-test`) contiene el firmware STM32F446RE para el adquisidor de datos **HERMES-A1 (AWTAS)** con módem Quectel EC25. El desarrollo se realiza desde dos PCs:

- **PC Remota** — investigación, debugging, análisis (ésta)
- **PC Local** — tiene el microcontrolador conectado, compila y flashea

Ambas instancias de OpenCode deben tener los mismos plugins para funcionar sincronizadas.

---

## Plugins Instalados

Configurados en `~/.config/opencode/opencode.json`:

| Plugin | Propósito | Comandos clave |
|--------|-----------|----------------|
| `oh-my-openagent` | Orquestación multi-agente (Sísifo, Metis, Momus, Oracle, etc.) | `/agents`, delegación automática |
| `opencode-supermemory` | Memoria persistente entre sesiones | — |
| `@tarquinen/opencode-dcp` | Compresión automática de contexto | Se activa solo |
| `@nick-vi/opencode-type-inject` | Type safety en tool calls | — |
| `opencode-mempalace` | Memoria estructurada con grafo de conocimiento | `/memory`, búsqueda semántica |
| `opencode-pty` | Gestión de procesos background (dev servers) | `pty_spawn`, `pty_write` |
| `@zenobius/opencode-skillful` | Sistema de skills bajo demanda | `skill()`, `skill_find()` |
| `opencode-background-agents` | Agentes en background para exploración paralela | `run_in_background=true` |
| `opencode-websearch-cited` | Búsqueda web con citas | `websearch`, `websearch_cited` |
| `./plugins/model-router` | Enrutamiento automático de modelos | — |
| `./plugins/token-goat.ts` | Control de consumo de tokens | — |
| `opencode-raven` | Firewall de contexto: enruta búsquedas web/docs/MCP a un agente dedicado | `/raven`, `raven_seek()` |
| `opencode-dux` | Orquestación multi-agente con descubrimiento automático de skills/MCPs | Se activa solo |
| `@mohak34/opencode-notifier` | Notificaciones del sistema al completar/errores | Config en `~/.config/opencode/opencode-notifier.json` |

---

## Cómo usar Raven

Raven enruta herramientas ruidosas (web search, fetch, MCPs) a un agente dedicado para ahorrar contexto:

```bash
/raven           # Ver estado
/raven on        # Activar routing
/raven off       # Desactivar routing (usa herramientas directamente)
/raven stats     # Ver contexto ahorrado
```

En los prompts de Sísifo, puedes llamar a `raven_seek()` para búsqueda externalizada.

---

## Cómo usar Dux

Dux se activa automáticamente. Mejora la delegación multi-agente con:

- Descubrimiento automático de skills (locales + npm)
- Descubrimiento de MCPs desde el registro npm
- Continuación automática de TODOs
- Monitoreo de presión de contexto

No requiere comandos manuales — funciona en background.

---

## Notificaciones

El plugin notifier envía notificaciones OS cuando OpenCode:

- Necesita permiso para ejecutar algo
- Completa una sesión/generación
- Encuentra un error
- Usa el tool `question`

Para personalizar, crear `~/.config/opencode/opencode-notifier.json`:

```json
{
  "sound": true,
  "notification": true,
  "events": {
    "permission": { "sound": true, "notification": true },
    "complete": { "sound": true, "notification": true },
    "error": { "sound": true, "notification": true }
  }
}
```

---

## Backend URL (Cloudflare Tunnel)

El firmware usa esta URL para uploads:

```
BACKEND_UPLOAD_URL="https://affected-christmas-bureau-heating.trycloudflare.com/upload"
```

Si el túnel cambia, actualizar en `Core/Inc/credentials.h`.

---

## Experimento: Ruta 1 (Direct Drive) vs Ruta 2 (Backend)

El firmware soporta dos rutas de upload:

| Ruta | Requisito | Estado |
|------|-----------|--------|
| 1 — Google Drive directo | `GDRIVE_TOKEN` + `GDRIVE_FOLDER_ID` | Token pendiente |
| 2 — Backend Flask | `BACKEND_UPLOAD_URL` + `BACKEND_API_KEY` | Activo |

Ver `quectel_drive.c` líneas 521-591 (Ruta 1) y 594-849 (Ruta 2).

Para obtener un token GDrive para Ruta 1, ejecutar desde `awtas-backend/`:

```python
from google.oauth2 import service_account
import google.auth.transport.requests

SCOPES = ['https://www.googleapis.com/auth/drive.file']
creds = service_account.Credentials.from_service_account_file(
    'credentials.json', scopes=SCOPES)
request = google.auth.transport.requests.Request()
creds.refresh(request)
print("GDRIVE_TOKEN:", creds.token)
```
