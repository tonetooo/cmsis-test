import sys
import re
import os

def update_credentials(url):
    # Remover protocolo y slashes finales para el HOST
    host = url.replace("https://", "").replace("http://", "").strip("/")
    
    # Rutas relativas desde backend/drive/backend-drive
    # Asumiendo estructura: backend/drive/backend-drive -> ../../../CODIGOS ACTUALES/C CODES/AWTAS_DEFINITIVE/Core/Inc/credentials.h
    # O ruta absoluta si es mas seguro
    
    # Intentar buscar la ruta relativa común
    # Desde: /Users/pedroavendano/Desktop/LIND/AWTAS_REPO/backend/drive/backend-drive
    # Hasta: /Users/pedroavendano/Desktop/LIND/CODIGOS ACTUALES/C CODES/AWTAS_DEFINITIVE/Core/Inc/credentials.h
    
    # Mejor usar ruta absoluta o relativa flexible
    # Buscamos 'credentials.h' subiendo niveles
    
    target_file = None
    
    # Ruta absoluta conocida del usuario (hardcoded para este entorno específico)
    possible_paths = [
        "/Users/pedroavendano/Desktop/LIND/CODIGOS ACTUALES/C CODES/AWTAS_DEFINITIVE/Core/Inc/credentials.h",
        "../../../CODIGOS ACTUALES/C CODES/AWTAS_DEFINITIVE/Core/Inc/credentials.h"
    ]
    
    for p in possible_paths:
        if os.path.exists(p):
            target_file = p
            break
            
    if not target_file:
        print("Error: credentials.h not found in known paths.")
        sys.exit(1)
        
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
