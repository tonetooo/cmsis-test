"""
Download file from Drive by file ID.
"""
import os, sys
from pathlib import Path
from google.oauth2 import service_account
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseDownload
import io

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

files_to_check = {
    "TEST_BIN2.CSV (ultimo)": "1dczuQW9fV8LvbPo3cx7w0_yDxgqcgtza",
    "TEST_BIN2.CSV (penultimo)": "1hCwDg1ZX_ZA6hDekwwjzARcQXFcLBm20",
    "TEST_BIN.CSV": "1n234RsRhbUP4LZ17M87B2DR7MBnc8dCt",
    "TEST_001.CSV": "17XcfLvEe0zj1RGA2K8kjK9RC3KaiC6Mj",
}

for label, fid in files_to_check.items():
    print(f"\n{'='*60}")
    print(f"=== {label} (ID: {fid})")
    print(f"{'='*60}")
    try:
        request = service.files().get_media(fileId=fid, supportsAllDrives=True)
        fh = io.BytesIO()
        downloader = MediaIoBaseDownload(fh, request)
        done = False
        while not done:
            status, done = downloader.next_chunk()
        content = fh.getvalue().decode('utf-8', errors='replace')
        print(content)
    except Exception as e:
        print(f"ERROR: {e}")
