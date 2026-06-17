# AWTAS Backend - Script de inicio para Windows
# Este script configura y ejecuta el backend

# Cambiar al directorio del script
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

Write-Host "========================================" -ForegroundColor Green
Write-Host "AWTAS Backend - Inicio Windows" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Verificar Python
Write-Host "[INFO] Verificando Python..." -ForegroundColor Cyan
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "[ERROR] Python no está instalado o no está en PATH" -ForegroundColor Red
    Exit 1
}
Write-Host "[OK] Python encontrado: $($python.Source)" -ForegroundColor Green

# Crear entorno virtual si no existe
if (-not (Test-Path ".venv")) {
    Write-Host "[INFO] Creando entorno virtual..." -ForegroundColor Cyan
    python -m venv .venv
}

# Activar entorno virtual
Write-Host "[INFO] Activando entorno virtual..." -ForegroundColor Cyan
& .\.venv\Scripts\Activate.ps1

# Instalar dependencias
Write-Host "[INFO] Instalando dependencias..." -ForegroundColor Cyan
pip install --upgrade pip
pip install -r requirements.txt

Write-Host ""
Write-Host "[INFO] Verificando archivos de configuración..." -ForegroundColor Cyan

# Verificar credentials.json
if (-not (Test-Path "credentials.json")) {
    Write-Host "[WARN] credentials.json no encontrado" -ForegroundColor Yellow
    Write-Host "       Descárgalo de Google Cloud Console" -ForegroundColor Yellow
}

# Crear .env si no existe
if (-not (Test-Path ".env")) {
    Write-Host "[INFO] Creando .env desde .env.example..." -ForegroundColor Cyan
    Copy-Item ".env.example" ".env"
    Write-Host "[WARN] Actualiza .env con tus valores" -ForegroundColor Yellow
}

# Crear directorio de logs
if (-not (Test-Path "logs")) {
    New-Item -ItemType Directory -Force -Path "logs" | Out-Null
}

# Encontrar puerto disponible
$port = if ($env:PORT) { $env:PORT } else { "8080" }
$portAvailable = $false

for ($p = [int]$port; $p -lt [int]$port + 100; $p++) {
    $netstat = netstat -ano 2>$null | Select-String ":$p "
    if (-not $netstat) {
        $port = $p
        $portAvailable = $true
        break
    }
}

if (-not $portAvailable) {
    Write-Host "[WARN] No se encontró puerto disponible" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "[INFO] Iniciando servidor en puerto: $port" -ForegroundColor Cyan
Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "Backend AWTAS ejecutándose" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host "URL Local:  http://localhost:$port" -ForegroundColor Cyan
Write-Host "Dashboard:  http://localhost:$port/dashboard" -ForegroundColor Cyan
Write-Host "Health:     http://localhost:$port/health" -ForegroundColor Cyan
Write-Host ""
Write-Host "Presiona Ctrl+C para detener" -ForegroundColor Yellow
Write-Host ""

$env:PORT = $port
$env:PYTHONUNBUFFERED = 1

# Ejecutar
python app.py
