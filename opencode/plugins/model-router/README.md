# OpenCode Model Router Plugin

Plugin de OpenCode para routing automático de modelos con **fallback inteligente** cuando se agota la cuota de OpenCode Go.

## Funcionalidad

1. **Clasifica automáticamente** cada tarea (implementación vs. consulta)
2. **Selecciona el mejor modelo disponible** según la cuota
3. **Detecta errores de cuota** y cambia automáticamente a modelos gratuitos
4. **Reintenta periódicamente** los modelos de pago cuando expira el cooldown

## Modelos por tipo de tarea

### `deep` (implementación) — orden de prioridad
1. ✅ **OpenCode Go** (`deepseek-v4-pro`) → si hay cuota
2. 🔄 **OpenCode** (`big-pickle`) → fallback automático
3. 🆓 **Google Gemini 2.0 Flash** → último recurso

### `qa` / `quick` (consulta)
1. 🆓 **Google Gemini 2.0 Flash** → siempre gratuito

## Fallback automático

Cuando OpenCode Go devuelve un error de cuota (`quota exhausted`, `rate limit`, `429`, etc.):

1. El plugin detecta el error vía el hook de eventos
2. Marca `opencode-go` como no saludable
3. Aplica **backoff exponencial**: 5 min → 10 min → 20 min → máx 1 hora
4. Las siguientes solicitudes usan `big-pickle` automáticamente
5. Cuando el cooldown expira, reintenta OpenCode Go
6. Después de 3 llamadas exitosas, lo marca como saludable nuevamente

## Comandos

| Comando | Descripción |
|---------|-------------|
| `/quota` | Muestra el estado de salud de cada proveedor |
| `/fallback` | Fuerza el uso de modelos gratuitos ahora |
| `/reset-models` | Restablece todos los proveedores como saludables |
| `/qa <pregunta>` | Fuerza modo consulta (Gemini Flash gratis) |
| `/deep <tarea>` | Fuerza modo implementación (Go o fallback) |

## Ejemplo de uso

```
# Automático — decide solo
"Haz tal cosa" → deep (usa Go o big-pickle según cuota)
"¿Qué es X?" → qa (Gemini Flash)

# Manual
/fallback                  → cambia a modelos gratis ahora
/quota                     → muestra: opencode-go: ⛔, opencode: ✅
/reset-models              → vuelve a intentar Go
```

## Clasificación de tareas

### Alta calidad (deep)
- Verbos: implement, add, create, fix, refactor, debug, review
- Keywords: multi-file, complex, architecture

### Bajo consumo (qa)
- Verbos: what, how, why, explain, compare, when, where
- Keywords: concept, definition, overview, summary

## Configuración

Edita `MODEL_CHAINS` en `index.js` para cambiar los modelos y su orden de prioridad:

```javascript
const MODEL_CHAINS = {
  deep: [
    { providerID: 'opencode-go', modelID: 'deepseek-v4-pro', label: '...' },
    { providerID: 'opencode', modelID: 'big-pickle', label: '...' },
    { providerID: 'google', modelID: 'gemini-2.0-flash', label: '...' },
  ],
  qa: [...],
  quick: [...],
};
```

## Logs

El plugin imprime en consola:
- `[model-router] Classified as: deep|qa`
- `[model-router] → OpenCode Go (deepseek-v4-pro)`
- `[model-router] ⛔ opencode-go marked unhealthy for 5min`
- `[model-router] ✅ opencode-go recovered — back online`
- `[model-router] 🔄 opencode-go cooldown expired — retrying`

## Detección de errores de cuota

El plugin detecta estos patrones en errores:
- `quota exhausted`, `insufficient quota`
- `rate limit`, `429`
- `credits exhausted`, `api_key exhausted`
- `payment required`
- Español: `cuota agotada`, `cupo insuficiente`, `límite excedido`
