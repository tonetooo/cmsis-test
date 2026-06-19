import sys
import re
import os
from pathlib import Path

def find_credentials_h():
    """Buscar credentials.h relativo a la ubicación de este script.
    
    Estructura esperada:
        awtas-backend/backend-drive/update_credentials.py
        Core/Inc/credentials.h
    
    Desde el script: ../../Core/Inc/credentials.h
    """
    script_dir = Path(__file__).resolve().parent
    
    # Desde backend-drive/ subir 2 niveles hasta project root, luego bajar a Core/Inc/
    candidates = [
        script_dir / "../../Core/Inc/credentials.h",
        script_dir / "../Core/Inc/credentials.h",          # si se copia a awtas-backend/
        script_dir / "../../../Core/Inc/credentials.h",     # un nivel extra por si hay nesting
    ]
    
    for p in candidates:
        resolved = p.resolve()
        if resolved.exists():
            return str(resolved)
    
    # Fallback: buscar en todo el subtree hacia arriba
    # Buscar hasta 5 niveles hacia arriba
    for parent in [script_dir] + list(script_dir.parents)[:5]:
        candidate = parent / "Core/Inc/credentials.h"
        if candidate.exists():
            return str(candidate)
    
    print(f"Error: credentials.h not found. Busqué desde: {script_dir}")
    print("Candidates checked:")
    for p in candidates:
        print(f"  - {p.resolve()}")
    sys.exit(1)


def update_credentials(url):
    # Remover protocolo y slashes finales para el HOST
    host = url.replace("https://", "").replace("http://", "").strip("/")
    
    target_file = find_credentials_h()
        
    print(f"Updating {target_file} with host: {host}")
    
    with open(target_file, 'r') as f:
        content = f.read()
        
    # Reemplazar BACKEND_CONFIG_URL
    content = re.sub(
        r'#define BACKEND_CONFIG_URL ".*?"',
        f'#define BACKEND_CONFIG_URL "{url}/config"',
        content
    )
    
    # Reemplazar BACKEND_UPLOAD_URL
    content = re.sub(
        r'#define BACKEND_UPLOAD_URL ".*?"',
        f'#define BACKEND_UPLOAD_URL "{url}/upload"',
        content
    )
    
    # Reemplazar BACKEND_HOST
    content = re.sub(
        r'#define BACKEND_HOST       ".*?"',
        f'#define BACKEND_HOST       "{host}"',
        content
    )
    
    with open(target_file, 'w') as f:
        f.write(content)
        
    print("credentials.h updated successfully.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python update_credentials.py <url>")
        sys.exit(1)
    update_credentials(sys.argv[1])
