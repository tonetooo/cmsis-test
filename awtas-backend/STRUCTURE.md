# 📁 Estructura del Proyecto AWTAS Backend

## Árbol completo de directorios

```
backend/drive/
│
├── 📄 app.py                         # Aplicación Flask principal
├── 📄 config.py                      # Configuración centralizada
├── 📄 requirements.txt               # Dependencias Python
├── 📄 .env.example                   # Plantilla de variables de entorno
├── 📄 .env                           # Variables de entorno (local, no en git)
├── 📄 credentials.json               # Credenciales Google (local, no en git)
├── 📄 .gitignore                     # Archivos a ignorar en git
│
├── 📄 start.sh                       # Script de inicio (Linux/macOS)
├── 📄 start.ps1                      # Script de inicio (Windows PowerShell)
├── 📄 validate.sh                    # Script de validación
│
├── 📚 README.md                      # Documentación completa
├── 📚 QUICK_START.md                 # Guía rápida de inicio
├── 📚 STRUCTURE.md                   # Este archivo
│
├── 📁 api/                           # Módulo API
│   ├── 📄 __init__.py                # Inicialización del paquete
│   │
│   ├── 📁 routes/                    # Rutas/Endpoints
│   │   ├── 📄 __init__.py            # Factory de rutas
│   │   ├── 📄 health.py              # GET /health, GET /
│   │   ├── 📄 upload.py              # POST /upload
│   │   └── 📄 config.py              # GET /config, POST /config/*
│   │
│   ├── 📁 services/                  # Servicios (lógica de negocio)
│   │   ├── 📄 __init__.py            # Exporta servicios
│   │   ├── 📄 drive_service.py       # Interacción con Google Drive
│   │   └── 📄 config_service.py      # Lógica de configuración AWTAS
│   │
│   └── 📁 utils/                     # Utilidades
│       ├── 📄 __init__.py            # Exporta utilidades
│       └── 📄 auth.py                # Autenticación y autorización
│
├── 📁 web/                           # Interfaz web
│   ├── 📁 templates/                 # Plantillas HTML
│   │   ├── 📄 base.html              # Plantilla base común
│   │   └── 📄 dashboard.html         # Dashboard principal
│   │
│   └── 📁 static/                    # Archivos estáticos
│       ├── 📁 css/
│       │   └── 📄 style.css          # Estilos principales
│       │
│       └── 📁 js/
│           └── 📄 app.js             # Lógica JavaScript del dashboard
│
├── 📁 config/                        # Configuración local
│   └── 📄 AWTAS_CONFIG.TXT          # Configuración del sensor (copia local)
│
├── 📁 logs/                          # Archivos de log (generado)
│   ├── 📄 backend.log               # Logs del servidor
│   └── 📄 tunnel.log                # Logs del tunnel (si aplica)
│
└── 📁 .venv/                         # Entorno virtual Python (generado)
```

## Descripción por carpeta

### 📁 `api/` - Módulo principal de la API

**Propósito**: Contiene toda la lógica de la API REST

#### 📁 `api/routes/` - Endpoints
- **`health.py`**: Rutas de estado y información
  - `GET /` - Información del servidor
  - `GET /health` - Estado de salud

- **`upload.py`**: Subida de archivos
  - `POST /upload` - Subir CSV a Google Drive

- **`config.py`**: Gestión de configuración
  - `GET /config` - Obtener configuración
  - `POST /config/init` - Crear/actualizar configuración
  - `POST /config/delete` - Eliminar configuración

#### 📁 `api/services/` - Servicios (Lógica)
- **`drive_service.py`**: Servicio de Google Drive
  - Gestión de conexión
  - Upload/descarga de archivos
  - Búsqueda y eliminación

- **`config_service.py`**: Servicio de configuración
  - Lectura/escritura de configs en Drive
  - Formateo compacto de configuración
  - Validación de configuración

#### 📁 `api/utils/` - Utilidades
- **`auth.py`**: Decoradores y funciones de autenticación
  - `@require_api_key` - Decorador para proteger rutas
  - Validación de API Key

### 📁 `web/` - Interfaz gráfica

**Propósito**: Dashboard web interactivo

#### 📁 `web/templates/` - HTML
- **`base.html`**: Plantilla base con navbar y footer
- **`dashboard.html`**: Dashboard principal con todos los widgets

#### 📁 `web/static/` - Assets
- **`css/style.css`**: Estilos (diseño responsive)
- **`js/app.js`**: Lógica interactiva (fetch API, eventos)

### 📁 `config/` - Configuración local
- **`AWTAS_CONFIG.TXT`**: Copia local de la configuración del sensor
- Usada como default si no existe en Drive

### 📁 `logs/` - Registros (se crea automáticamente)
- **`backend.log`**: Salida del servidor Flask
- **`tunnel.log`**: Salida de Cloudflare Tunnel

## Archivos clave

### 📄 `app.py` - Punto de entrada
```
- Crea aplicación Flask
- Registra blueprints (rutas)
- Configura manejo de errores
- Inicia el servidor
```

### 📄 `config.py` - Configuración centralizada
```
- Carga variables de .env
- Define diferentes configuraciones (dev/prod)
- Centraliza constantes y rutas
```

### 📄 `requirements.txt` - Dependencias
```
Flask              # Framework web
google-auth        # Autenticación Google
google-api-python-client  # API de Google Drive
python-dotenv      # Carga de .env
```

## Flujo de datos

### Subir archivo CSV
```
Cliente (curl/navegador)
    ↓
[Ruta POST /upload]
    ↓
[Decorador @require_api_key]
    ↓
[upload.py - validar]
    ↓
[drive_service.py - subir a Drive]
    ↓
Google Drive
    ↓
JSON {id, name, status}
    ↓
Cliente
```

### Obtener configuración
```
Cliente (curl/navegador)
    ↓
[Ruta GET /config]
    ↓
[Decorador @require_api_key]
    ↓
[config.py - obtener]
    ↓
[drive_service.py - descargar]
    ↓
Google Drive
    ↓
[config_service.py - formatear]
    ↓
Texto plano (AWTAS_CONFIG.TXT)
    ↓
Cliente
```

## Convenciones del proyecto

### Nombrado
- Archivos: `snake_case.py`
- Clases: `PascalCase`
- Funciones: `snake_case()`
- Constantes: `SNAKE_CASE`

### Estructura de carpetas
- Un servicio por archivo
- Una ruta por endpoint
- Código relacionado agrupado lógicamente

### Seguridad
- API Key en headers o query
- Validación en cada endpoint
- Archivos sensibles en .gitignore

## Cómo agregar nuevas funcionalidades

### Agregar nuevo endpoint
1. Crear archivo en `api/routes/nuevo.py`
2. Definir Blueprint y rutas
3. Agregar en `api/routes/__init__.py`
4. Documentar en `README.md`

### Agregar nuevo servicio
1. Crear `api/services/nuevo_servicio.py`
2. Definir clase con métodos
3. Exportar en `api/services/__init__.py`
4. Usar en rutas según sea necesario

### Agregar nueva página web
1. Crear `web/templates/nueva_pagina.html`
2. Crear ruta en `app.py` o routes
3. Agregar estilos en `web/static/css/style.css`
4. Agregar lógica en `web/static/js/app.js`

## Validación

Ejecutar antes de deployar:
```bash
bash validate.sh
```

Verifica:
- ✓ Estructura de directorios
- ✓ Archivos críticos
- ✓ Configuración
- ✓ Documentación
