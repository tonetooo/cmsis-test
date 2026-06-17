"""
Listar archivos en la carpeta de Google Drive configurada.
"""
import os, sys, json
from pathlib import Path
from google.oauth2 import service_account
from googleapiclient.discovery import build

ENV_PATH = Path(__file__).parent / "awtas-backend" / ".env"
if ENV_PATH.exists():
    with open(ENV_PATH) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                os.environ.setdefault(k.strip(), v.strip())

creds_file = str(Path(__file__).parent / "awtas-backend" / "credentials.json")
folder_id = os.environ.get("DRIVE_FOLDER_ID", "")

creds = service_account.Credentials.from_service_account_file(
    creds_file, scopes=["https://www.googleapis.com/auth/drive.readonly"]
)
service = build("drive", "v3", credentials=creds)

# Listar archivos en la carpeta
page_token = None
all_files = []
while True:
    result = service.files().list(
        q=f"'{folder_id}' in parents and trashed = false",
        spaces="drive",
        fields="nextPageToken, files(id, name, createdTime, size, mimeType, webViewLink)",
        pageSize=100,
        orderBy="createdTime desc",
        supportsAllDrives=True,
        includeItemsFromAllDrives=True,
        pageToken=page_token,
    ).execute()
    all_files.extend(result.get("files", []))
    page_token = result.get("nextPageToken")
    if not page_token:
        break

print(f"Total archivos: {len(all_files)}\n")
for f in all_files:
    sz = int(f.get('size', 0))
    print(f"  {f['createdTime'][:19]}  {sz:>8,d} B  {f['name']}")
    print(f"    ID: {f['id']}")
    print(f"    Link: {f.get('webViewLink', 'N/A')}")
    print()
