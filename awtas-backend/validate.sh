#!/usr/bin/env bash

###########################################################
# Script de validación para el proyecto AWTAS Backend
# Verifica que todo esté correctamente configurado
###########################################################

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

check_passed() {
    echo -e "${GREEN}✓${NC} $1"
}

check_failed() {
    echo -e "${RED}✗${NC} $1"
}

check_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}AWTAS Backend - Validación de Setup${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Variable para rastrear si hay errores críticos
HAS_ERRORS=0

# 1. Verificar Python
echo -e "${BLUE}[1] Verificando Python...${NC}"
if command -v python3 &> /dev/null; then
    VERSION=$(python3 --version)
    check_passed "Python instalado: $VERSION"
else
    check_failed "Python 3 no encontrado"
    HAS_ERRORS=1
fi
echo ""

# 2. Verificar estructura de directorios
echo -e "${BLUE}[2] Verificando estructura de directorios...${NC}"
DIRS=(
    "api"
    "api/routes"
    "api/services"
    "api/utils"
    "web"
    "web/templates"
    "web/static"
    "web/static/css"
    "web/static/js"
    "config"
    "logs"
)

for dir in "${DIRS[@]}"; do
    if [ -d "$dir" ]; then
        check_passed "Directorio: $dir"
    else
        check_warning "Directorio faltante: $dir (se creará automáticamente)"
    fi
done
echo ""

# 3. Verificar archivos Python críticos
echo -e "${BLUE}[3] Verificando archivos Python...${NC}"
FILES=(
    "app.py"
    "config.py"
    "api/__init__.py"
    "api/routes/__init__.py"
    "api/routes/health.py"
    "api/routes/upload.py"
    "api/routes/config.py"
    "api/services/__init__.py"
    "api/services/drive_service.py"
    "api/services/config_service.py"
    "api/utils/__init__.py"
    "api/utils/auth.py"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        check_passed "Archivo: $file"
    else
        check_failed "Archivo faltante: $file"
        HAS_ERRORS=1
    fi
done
echo ""

# 4. Verificar templates
echo -e "${BLUE}[4] Verificando templates...${NC}"
TEMPLATES=(
    "web/templates/base.html"
    "web/templates/dashboard.html"
)

for template in "${TEMPLATES[@]}"; do
    if [ -f "$template" ]; then
        check_passed "Template: $template"
    else
        check_failed "Template faltante: $template"
        HAS_ERRORS=1
    fi
done
echo ""

# 5. Verificar archivos estáticos
echo -e "${BLUE}[5] Verificando archivos estáticos...${NC}"
STATIC=(
    "web/static/css/style.css"
    "web/static/js/app.js"
)

for static in "${STATIC[@]}"; do
    if [ -f "$static" ]; then
        check_passed "Archivo estático: $static"
    else
        check_failed "Archivo estático faltante: $static"
        HAS_ERRORS=1
    fi
done
echo ""

# 6. Verificar archivos de configuración
echo -e "${BLUE}[6] Verificando archivos de configuración...${NC}"
if [ -f "requirements.txt" ]; then
    check_passed "requirements.txt encontrado"
else
    check_failed "requirements.txt no encontrado"
    HAS_ERRORS=1
fi

if [ -f ".env.example" ]; then
    check_passed ".env.example encontrado"
else
    check_failed ".env.example no encontrado"
    HAS_ERRORS=1
fi

if [ -f ".env" ]; then
    check_passed ".env configurado"
else
    check_warning ".env no encontrado (se creará desde .env.example)"
fi

if [ -f "credentials.json" ]; then
    check_passed "credentials.json encontrado"
else
    check_warning "credentials.json no encontrado (necesario para Google Drive)"
fi

if [ -f "config/AWTAS_CONFIG.TXT" ]; then
    check_passed "config/AWTAS_CONFIG.TXT encontrado"
else
    check_warning "config/AWTAS_CONFIG.TXT no encontrado"
fi
echo ""

# 7. Verificar documentación
echo -e "${BLUE}[7] Verificando documentación...${NC}"
DOCS=(
    "README.md"
    "QUICK_START.md"
)

for doc in "${DOCS[@]}"; do
    if [ -f "$doc" ]; then
        check_passed "Documentación: $doc"
    else
        check_warning "Documentación faltante: $doc"
    fi
done
echo ""

# 8. Verificar scripts
echo -e "${BLUE}[8] Verificando scripts...${NC}"
if [ -f "start.sh" ]; then
    check_passed "Script start.sh encontrado"
    if [ -x "start.sh" ]; then
        check_passed "start.sh es ejecutable"
    else
        check_warning "start.sh no es ejecutable (ejecutar: chmod +x start.sh)"
    fi
else
    check_warning "Script start.sh no encontrado"
fi
echo ""

# 9. Verificar .gitignore
echo -e "${BLUE}[9] Verificando .gitignore...${NC}"
if [ -f ".gitignore" ]; then
    check_passed ".gitignore encontrado"
else
    check_warning ".gitignore no encontrado"
fi
echo ""

# Resumen
echo -e "${BLUE}========================================${NC}"
if [ $HAS_ERRORS -eq 0 ]; then
    echo -e "${GREEN}✓ Setup completado correctamente${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo -e "${GREEN}Próximos pasos:${NC}"
    echo "1. Copiar credentials.json desde Google Cloud Console"
    echo "2. Ejecutar: bash start.sh"
    echo "3. Ir a: http://localhost:8080/dashboard"
    exit 0
else
    echo -e "${RED}✗ Hay errores que deben corregirse${NC}"
    echo -e "${BLUE}========================================${NC}"
    exit 1
fi
