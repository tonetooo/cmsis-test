"""
Herramienta CLI para subir archivos a Google Drive
Usa las credenciales de service account y configuracion existentes del proyecto AWTAS.

Uso:
    python subir_a_drive.py <ruta_del_archivo> [--nombre NOMBRE] [--folder FOLDER_ID]
    
Ejemplos:
    python subir_a_drive.py DOSSIER_COMERCIAL_HERMES-A1.md
    python subir_a_drive.py data.csv --nombre "mediciones/junio2026.csv"
    python subir_a_drive.py reporte.pdf --folder ID_DE_CARPETA_ALTERNATIVA
"""

import os
import sys
import argparse
import json
from pathlib import Path

# Cargar .env manualmente
ENV_PATH = Path(__file__).parent / "awtas-backend" / ".env"
if ENV_PATH.exists():
    with open(ENV_PATH) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, val = line.split("=", 1)
                os.environ.setdefault(key.strip(), val.strip())


def get_config():
    """Obtener configuracion desde variables de entorno."""
    creds_path = Path(__file__).parent / "awtas-backend" / "credentials.json"
    return {
        "credentials_file": str(creds_path),
        "folder_id": os.environ.get("DRIVE_FOLDER_ID", ""),
    }


def upload_file(file_path: str, filename: str = None, folder_id: str = None) -> dict:
    """
    Sube un archivo a Google Drive.

    Args:
        file_path: Ruta al archivo a subir
        filename: Nombre en Drive (opcional, por defecto usa el nombre original)
        folder_id: ID de carpeta destino (opcional, por defecto usa DRIVE_FOLDER_ID del .env)

    Returns:
        dict con id, name, webViewLink, createdTime
    """
    from google.oauth2 import service_account
    from googleapiclient.discovery import build
    from googleapiclient.http import MediaFileUpload

    config = get_config()
    folder_id = folder_id or config["folder_id"]

    if not folder_id:
        raise ValueError(
            "No se especifico DRIVE_FOLDER_ID. "
            "Configuralo en awtas-backend/.env o pasalo con --folder"
        )

    # Verificar credenciales
    if not os.path.exists(config["credentials_file"]):
        raise FileNotFoundError(
            f"No se encuentra credentials.json en {config['credentials_file']}"
        )

    # Verificar archivo a subir
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"El archivo no existe: {file_path}")

    if filename is None:
        filename = file_path.name

    # Inicializar servicio
    creds = service_account.Credentials.from_service_account_file(
        config["credentials_file"],
        scopes=["https://www.googleapis.com/auth/drive.file"],
    )
    service = build("drive", "v3", credentials=creds)

    # Detectar MIME type basico
    mime_types = {
        ".csv": "text/csv",
        ".txt": "text/plain",
        ".md": "text/markdown",
        ".pdf": "application/pdf",
        ".json": "application/json",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".zip": "application/zip",
        ".bin": "application/octet-stream",
        ".html": "text/html",
        ".css": "text/css",
        ".js": "application/javascript",
        ".py": "text/x-python",
        ".xlsx": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        ".docx": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    }
    mime = mime_types.get(file_path.suffix.lower(), "application/octet-stream")

    # Subir archivo
    print(f"Subiendo {file_path.name} ({file_path.stat().st_size} bytes) a Google Drive...")
    print(f"  Carpeta destino: {folder_id}")
    print(f"  Nombre en Drive: {filename}")
    print(f"  MIME type: {mime}")

    media = MediaFileUpload(str(file_path), mimetype=mime, resumable=False)
    file = (
        service.files()
        .create(
            body={
                "name": filename,
                "parents": [folder_id],
            },
            media_body=media,
            fields="id,name,webViewLink,createdTime",
            supportsAllDrives=True,
        )
        .execute()
    )

    print(f"  OK! Archivo subido exitosamente.")
    print(f"  ID: {file.get('id')}")
    print(f"  Link: {file.get('webViewLink')}")

    return file


def main():
    parser = argparse.ArgumentParser(
        description="Subir archivos a Google Drive usando credenciales AWTAS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos:
  python subir_a_drive.py DOSSIER_COMERCIAL_HERMES-A1.md
  python subir_a_drive.py data.csv --nombre "mediciones/resultado.csv"
  python subir_a_drive.py reporte.pdf --folder ID_DE_CARPETA
        """,
    )
    parser.add_argument("archivo", help="Ruta al archivo que se quiere subir")
    parser.add_argument("--nombre", "-n", help="Nombre con el que se guardara en Drive (opcional)")
    parser.add_argument(
        "--folder", "-f",
        help="ID de la carpeta destino en Drive (opcional, por defecto usa .env)"
    )

    args = parser.parse_args()

    try:
        result = upload_file(
            file_path=args.archivo,
            filename=args.nombre,
            folder_id=args.folder,
        )
        # Salida JSON limpia para piping
        print(f"\n--- JSON ---")
        print(json.dumps(result, indent=2, ensure_ascii=False))
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
