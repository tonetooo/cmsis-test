# AWTAS Backend - Sensor de Google Drive

Backend para el sensor ADXL355 que permite:
- Subir datos CSV a Google Drive automáticamente
- Gestionar configuración remota del sensor
- Dashboard web interactivo
- API REST segura con autenticación por API Key

## 📋 Requisitos Previos

- Python 3.8 o superior
- Google Cloud Console con credenciales de servicio configuradas
- pip (gestor de paquetes de Python)

## 🚀 Instalación

### 1. Clonar o descargar el proyecto

```bash
cd backend/drive
```

### 2. Crear entorno virtual

```bash
python -m venv .venv

# En Windows:
.venv\Scripts\activate

# En macOS/Linux:
source .venv/bin/activate
```

### 3. Instalar dependencias

```bash
pip install -r requirements.txt
```

### 4. Configurar credenciales

1. **Descargar JSON de credenciales de Google**:
   - Ir a [Google Cloud Console](https://console.cloud.google.com/)
   - Crear una cuenta de servicio
   - Descargar el JSON
   - Renombrar a `credentials.json` y colocar en esta carpeta

2. **Compartir carpeta de Drive**:
   - Obtener ID de la carpeta en Google Drive
   - Compartirla con el email de la cuenta de servicio
   - Copiar el ID de la carpeta

### 5. Configurar variables de entorno

Copiar `.env.example` a `.env` y actualizar:

```bash
cp .env.example .env
```

Editar `.env` con tus valores:

```env
DRIVE_FOLDER_ID=tu_id_de_carpeta_aqui
DEVICE_API_KEY=tu_api_key_aqui
ENABLE_API_KEY=True
DEBUG=True
PORT=8080
```

## 🏃 Ejecutar el Backend

### Opción 1: Ejecutar directamente

```bash
python app.py
```

El servidor estará disponible en `http://localhost:8080`

### Opción 2: Usar script de inicio (Linux/macOS)

```bash
bash start.sh
```

### Opción 3: Con Cloudflare Tunnel

```bash
export CLOUDFLARE_TUNNEL_NAME=tu-tunnel-name
bash start.sh
```

## 🌐 Dashboard Web

Acceder a: `http://localhost:8080/dashboard`

Funcionalidades:
- ✅ Ver estado del servidor
- ✅ Subir archivos CSV
- ✅ Obtener/actualizar/eliminar configuración
- ✅ Documentación de API integrada

## 📡 Endpoints API

### 1. Health Check

```bash
curl http://localhost:8080/health
```

Respuesta:
```json
{
  "status": "healthy",
  "message": "Backend AWTAS is running",
  "drive_configured": true
}
```

### 2. Subir Archivo CSV

```bash
curl -X POST \
  -H "X-Api-Key: LIND2026ANTONIO" \
  --data-binary @data.csv \
  "http://localhost:8080/upload?filename=datos.csv"
```

Respuesta:
```json
{
  "status": "success",
  "id": "1abc123...",
  "name": "datos.csv",
  "createdTime": "2026-05-05T10:30:00.000Z"
}
```

### 3. Obtener Configuración

```bash
# Configuración completa
curl -H "X-Api-Key: LIND2026ANTONIO" \
  "http://localhost:8080/config?compact=0"

# Configuración compacta (solo claves importantes)
curl -H "X-Api-Key: LIND2026ANTONIO" \
  "http://localhost:8080/config?compact=1"
```

### 4. Inicializar/Actualizar Configuración

```bash
curl -X POST \
  -H "X-Api-Key: LIND2026ANTONIO" \
  "http://localhost:8080/config/init"
```

Respuesta:
```json
{
  "status": "created",
  "file": {
    "id": "1xyz789...",
    "name": "AWTAS_CONFIG.TXT",
    "createdTime": "2026-05-05T10:30:00.000Z"
  }
}
```

### 5. Eliminar Configuración

```bash
curl -X POST \
  -H "X-Api-Key: LIND2026ANTONIO" \
  "http://localhost:8080/config/delete"
```

Respuesta:
```json
{
  "status": "deleted",
  "count": 1,
  "name": "AWTAS_CONFIG.TXT"
}
```

## 📁 Estructura del Proyecto

```
backend/drive/
├── app.py                 # Aplicación principal Flask
├── config.py              # Configuración centralizada
├── requirements.txt       # Dependencias Python
├── .env.example          # Plantilla de variables de entorno
├── .env                  # Variables de entorno (local, no subir a git)
├── credentials.json      # Credenciales de Google (local, no subir a git)
├── start.sh              # Script de inicio
│
├── api/                  # Módulo API
│   ├── __init__.py
│   ├── routes/           # Rutas/Endpoints
│   │   ├── __init__.py
│   │   ├── health.py     # Health check
│   │   ├── upload.py     # Subir archivos
│   │   └── config.py     # Gestionar configuración
│   ├── services/         # Servicios/Lógica de negocio
│   │   ├── __init__.py
│   │   ├── drive_service.py    # Interacción con Google Drive
│   │   └── config_service.py   # Lógica de configuración
│   └── utils/            # Utilidades
│       ├── __init__.py
│       └── auth.py       # Autenticación y autorización
│
├── web/                  # Interfaz web
│   ├── templates/        # Plantillas HTML
│   │   ├── base.html     # Plantilla base
│   │   └── dashboard.html # Dashboard principal
│   └── static/           # Archivos estáticos
│       ├── css/
│       │   └── style.css
│       └── js/
│           └── app.js
│
└── config/              # Archivos de configuración local
    └── AWTAS_CONFIG.TXT # Configuración del sensor (si existe localmente)
```

## 🔧 Configuración del Sensor

El archivo `AWTAS_CONFIG.TXT` contiene la configuración del sensor ADXL355:

```
RANGE=2                 # Rango: ±2g, ±4g o ±8g
ODR_HZ=125             # Frecuencia de muestreo (Hz)
TRIGGER_G=0.50         # Umbral de activación (g)
HPF=OFF                # Filtro pasa-altos
ACT_COUNT=5            # Muestras consecutivas
OPERATION_MODE=2       # 1=Manual, 2=Autónomo
FILE_MANUAL=CSV        # Formato manual
FILE_AUTO=CSV          # Formato automático
```

## 🔐 Seguridad

- **API Key**: Requerida en header `X-Api-Key` o parámetro `key`
- **Google Drive**: Solo acceso a carpeta específica
- **HTTPS**: Recomendado en producción (usar Cloudflare Tunnel)

## 🐛 Solución de Problemas

### Error: "DRIVE_FOLDER_ID not configured"
✓ Verificar que `.env` tiene `DRIVE_FOLDER_ID` configurado

### Error: "credentials.json not found"
✓ Descargar credenciales de Google Cloud Console
✓ Verificar que están en la carpeta correcta

### Error: "unauthorized"
✓ Verificar que `DEVICE_API_KEY` en `.env` coincide con la del cliente
✓ Usar header `X-Api-Key` correctamente

### Puerto ya en uso
✓ Cambiar `PORT` en `.env`
✓ O matar el proceso: `lsof -i :8080` (Linux/macOS)

## 📝 Logs

- **backend.log**: Logs del servidor Flask
- **tunnel.log**: Logs del Cloudflare Tunnel (si aplica)

## 🤝 Contribuir

Este proyecto es parte del proyecto LIND.

## 📄 Licencia

Todos los derechos reservados © 2026 LIND Project
