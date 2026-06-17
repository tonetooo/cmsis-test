# ⚡ INICIO RÁPIDO - AWTAS Backend

## 1️⃣ Configuración Inicial (5 minutos)

### Windows

```powershell
# Abre PowerShell en esta carpeta y ejecuta:
.\.start.ps1
```

### Linux / macOS

```bash
# Dale permisos al script
chmod +x start.sh

# Ejecuta:
./start.sh
```

## 2️⃣ Configurar Credenciales de Google

1. **Ir a [Google Cloud Console](https://console.cloud.google.com/)**
2. **Crear una cuenta de servicio**:
   - Ir a "Cuentas de servicio"
   - Crear nueva cuenta
   - Crear una clave JSON
3. **Descargar el JSON**
4. **Renombrar a `credentials.json`**
5. **Copiar al directorio `backend/drive/`**

## 3️⃣ Configurar Variables de Entorno

1. **Editar `.env`** (el script crea una copia de `.env.example`)

```env
# Reemplazar con tus valores:
DRIVE_FOLDER_ID=tu_id_de_carpeta_aqui
DEVICE_API_KEY=tu_api_key_aqui
```

**Para obtener el ID de la carpeta:**
- Abrir la carpeta en Google Drive
- Copiar el ID de la URL: `https://drive.google.com/drive/folders/[ID_AQUI]`
- Compartir la carpeta con el email de la cuenta de servicio (se ve en `credentials.json`)

## 4️⃣ Ejecutar el Backend

### Método 1: Script automático

**Windows:**
```powershell
.\.start.ps1
```

**Linux/macOS:**
```bash
./start.sh
```

### Método 2: Directo con Python

```bash
python app.py
```

## 5️⃣ Acceder al Dashboard

Abre en tu navegador:

```
http://localhost:8080/dashboard
```

## ✅ Verificar que funciona

```bash
# En otra terminal/PowerShell:
curl http://localhost:8080/health
```

Deberías ver algo como:
```json
{
  "status": "healthy",
  "message": "Backend AWTAS is running",
  "drive_configured": true
}
```

## 🔑 Usar la API

### Subir archivo CSV

```bash
curl -X POST \
  -H "X-Api-Key: LIND2026ANTONIO" \
  --data-binary @miarchivo.csv \
  "http://localhost:8080/upload?filename=datos.csv"
```

### Obtener configuración

```bash
curl -H "X-Api-Key: LIND2026ANTONIO" \
  "http://localhost:8080/config"
```

## 🐛 Troubleshooting Rápido

| Problema | Solución |
|----------|----------|
| "Python no encontrado" | Instalar Python 3.8+ desde python.org |
| "credentials.json no encontrado" | Descargar de Google Cloud Console |
| "DRIVE_FOLDER_ID not configured" | Editar `.env` con el ID correcto |
| "Puerto 8080 en uso" | Cambiar `PORT` en `.env` |
| "unauthorized" | Verificar `DEVICE_API_KEY` en `.env` |

## 📚 Más Información

Ver [README.md](README.md) para documentación completa.

## 🎯 Próximos Pasos

1. ✅ Backend corriendo
2. 📱 Conectar sensor a la API
3. 📊 Ver datos en Google Drive
4. 🔧 Configurar sensor remotamente desde Dashboard
