# AWTAS Backend - Script de inicio para Windows
# Arranca Flask + cloudflared tunnel + actualiza credentials.h del STM32
# Uso: .\start.ps1

param(
    [switch]$NoTunnel,
    [switch]$NoUpdateCredentials
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

function Log($msg, $color = "Green") { Write-Host "[AWTAS] $msg" -ForegroundColor $color }

# ── 1. Verificar dependencias ──────────────────────────────────────
Log "Verificando dependencias..."
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { Log "Python no encontrado en PATH" Red; exit 1 }
Log "Python: $($python.Source)"

$cloudflared = $null
if (-not $NoTunnel) {
    $cloudflared = Get-Command cloudflared -ErrorAction SilentlyContinue
    if (-not $cloudflared) { Log "cloudflared no encontrado. Usa -NoTunnel para omitir." Red; exit 1 }
    Log "cloudflared: $($cloudflared.Source)"
}

# ── 2. Entorno virtual + dependencias ──────────────────────────────
if (-not (Test-Path ".venv")) {
    Log "Creando entorno virtual..."
    python -m venv .venv
}

Log "Activando venv..."
& .\.venv\Scripts\Activate.ps1

Log "Instalando dependencias..."
pip install -q --upgrade pip
pip install -q -r requirements.txt

# ── 3. Verificar archivos críticos ─────────────────────────────────
if (-not (Test-Path "credentials.json")) {
    Log "credentials.json no encontrado (necesario para Google Drive)" Yellow
}
if (-not (Test-Path ".env")) {
    if (Test-Path ".env.example") {
        Copy-Item ".env.example" ".env"
        Log ".env creado desde .env.example — verifica los valores" Yellow
    }
}

# Cargar .env
if (Test-Path ".env") {
    Get-Content ".env" | ForEach-Object {
        if ($_ -match '^\s*([^#][^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1].Trim(), $matches[2].Trim(), "Process")
        }
    }
}

# ── 4. Puerto ──────────────────────────────────────────────────────
$port = if ($env:PORT) { [int]$env:PORT } else { 8080 }
$portInUse = netstat -ano 2>$null | Select-String ":$port\s+.*LISTENING"
if ($portInUse) {
    Log "Puerto $port en uso, buscando alternativo..." Yellow
    for ($p = $port + 1; $p -lt $port + 20; $p++) {
        $check = netstat -ano 2>$null | Select-String ":$p\s+.*LISTENING"
        if (-not $check) { $port = $p; break }
    }
}
$env:PORT = $port
$env:PYTHONUNBUFFERED = 1

# ── 5. Crear directorio logs ───────────────────────────────────────
if (-not (Test-Path "logs")) { New-Item -ItemType Directory -Force -Path "logs" | Out-Null }

# ── 6. Matar procesos anteriores ───────────────────────────────────
Log "Limpiando procesos anteriores..."
Get-Process -Name python -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -like "*app.py*" } |
    Stop-Process -Force -ErrorAction SilentlyContinue
Get-Process -Name cloudflared -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# ── 7. Arrancar Flask en background ────────────────────────────────
Log "Arrancando Flask en puerto $port..."
$flaskLog = Join-Path $scriptDir "logs\flask.log"
$flaskProcess = Start-Process -PassThru -NoNewWindow `
    -FilePath ".\.venv\Scripts\python.exe" `
    -ArgumentList "app.py" `
    -RedirectStandardOutput $flaskLog `
    -RedirectStandardError (Join-Path $scriptDir "logs\flask_error.log")

# Esperar a que Flask responda
Log "Esperando Flask..."
$flaskReady = $false
for ($i = 0; $i -lt 15; $i++) {
    Start-Sleep -Seconds 1
    try {
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:$port/health" -TimeoutSec 2 -UseBasicParsing -ErrorAction SilentlyContinue
        if ($resp.StatusCode -ge 200) { $flaskReady = $true; break }
    } catch {
        try {
            $resp = Invoke-WebRequest -Uri "http://127.0.0.1:$port/config" -TimeoutSec 2 -UseBasicParsing -ErrorAction SilentlyContinue
            if ($resp.StatusCode -ge 200 -or $resp.StatusCode -eq 401) { $flaskReady = $true; break }
        } catch {}
    }
}

if (-not $flaskReady) {
    Log "Flask no respondió en 15s. Revisa logs\flask.log" Red
    Get-Content $flaskLog -Tail 10 -ErrorAction SilentlyContinue
    exit 1
}
Log "Flask OK en http://127.0.0.1:$port"

# ── 8. Arrancar cloudflared tunnel ─────────────────────────────────
$tunnelUrl = $null

if (-not $NoTunnel) {
    Log "Arrancando cloudflared tunnel..."
    $tunnelLog = Join-Path $scriptDir "logs\tunnel.log"

    $tunnelProcess = Start-Process -PassThru -NoNewWindow `
        -FilePath "cloudflared" `
        -ArgumentList "tunnel","--url","http://localhost:$port" `
        -RedirectStandardOutput (Join-Path $scriptDir "logs\tunnel_stdout.log") `
        -RedirectStandardError $tunnelLog

    # Esperar a que aparezca la URL en el log
    Log "Esperando URL del túnel..."
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Seconds 1
        if (Test-Path $tunnelLog) {
            $match = Select-String -Path $tunnelLog -Pattern "https://[a-z0-9-]+\.trycloudflare\.com" | Select-Object -First 1
            if ($match) {
                $tunnelUrl = [regex]::Match($match.Line, "https://[a-z0-9-]+\.trycloudflare\.com").Value
                break
            }
        }
    }

    if (-not $tunnelUrl) {
        Log "No se obtuvo URL del túnel en 20s. Revisa logs\tunnel.log" Yellow
    } else {
        Log "Túnel activo: $tunnelUrl"
    }
}

# ── 9. Actualizar credentials.h del STM32 ──────────────────────────
if ($tunnelUrl -and -not $NoUpdateCredentials) {
    $credFile = Join-Path $scriptDir "..\Core\Inc\credentials.h"
    if (Test-Path $credFile) {
        $hostOnly = $tunnelUrl -replace "https://", ""
        $content = Get-Content $credFile -Raw

        $content = $content -replace '(#define\s+BACKEND_CONFIG_URL\s+")[^"]*(")', "`$1${tunnelUrl}/config`$2"
        $content = $content -replace '(#define\s+BACKEND_UPLOAD_URL\s+")[^"]*(")', "`$1${tunnelUrl}/upload`$2"
        $content = $content -replace '(#define\s+BACKEND_HOST\s+")[^"]*(")', "`$1${hostOnly}`$2"

        Set-Content $credFile $content -NoNewline
        Log "credentials.h actualizado con nueva URL"
    } else {
        Log "credentials.h no encontrado en $credFile" Yellow
    }
}

# ── 10. Guardar tunnel_url.txt ─────────────────────────────────────
if ($tunnelUrl) {
    $ts = Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ"
    "$tunnelUrl`nTIMESTAMP=$ts" | Set-Content "tunnel_url.txt"
}

# ── 11. Resumen ────────────────────────────────────────────────────
Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host " AWTAS Backend ACTIVO" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host " Local:     http://localhost:$port" -ForegroundColor Cyan
Write-Host " Dashboard: http://localhost:$port/dashboard" -ForegroundColor Cyan
if ($tunnelUrl) {
    Write-Host " Tunnel:    $tunnelUrl" -ForegroundColor Yellow
    Write-Host " Upload:    $tunnelUrl/upload" -ForegroundColor Yellow
    Write-Host " Config:    $tunnelUrl/config" -ForegroundColor Yellow
}
Write-Host ""
Write-Host " PIDs: Flask=$($flaskProcess.Id) Tunnel=$($tunnelProcess.Id)" -ForegroundColor DarkGray
Write-Host " Para detener: Stop-Process -Id $($flaskProcess.Id),$($tunnelProcess.Id)" -ForegroundColor DarkGray
Write-Host "================================================" -ForegroundColor Green
Write-Host ""

# Mantener vivo — Ctrl+C para detener
try {
    Wait-Process -Id $flaskProcess.Id
} catch {
    Log "Deteniendo..." Yellow
    Stop-Process -Id $flaskProcess.Id -Force -ErrorAction SilentlyContinue
    if ($tunnelProcess) { Stop-Process -Id $tunnelProcess.Id -Force -ErrorAction SilentlyContinue }
}
