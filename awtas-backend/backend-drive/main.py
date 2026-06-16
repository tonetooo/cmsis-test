import io
import os
from flask import Flask, request, jsonify
from google.oauth2 import service_account
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseUpload
import google.auth

SCOPES = ["https://www.googleapis.com/auth/drive.file"]
BASE_DIR = os.path.dirname(__file__)
LOCAL_CRED = os.path.join(BASE_DIR, "credentials.json")
PARENT_CRED = os.path.join(os.path.dirname(BASE_DIR), "credentials.json")
SERVICE_ACCOUNT_FILE = LOCAL_CRED if os.path.exists(LOCAL_CRED) else PARENT_CRED
DRIVE_FOLDER_ID = os.environ.get("DRIVE_FOLDER_ID", "")
API_KEY_EXPECTED = os.environ.get("DEVICE_API_KEY", "")

app = Flask(__name__)

def get_drive_service():
    if os.path.exists(SERVICE_ACCOUNT_FILE):
        creds = service_account.Credentials.from_service_account_file(
            SERVICE_ACCOUNT_FILE, scopes=SCOPES
        )
    else:
        creds, _ = google.auth.default(scopes=SCOPES)
    return build("drive", "v3", credentials=creds)

@app.route("/upload", methods=["POST"])
def upload():
    api_key = request.headers.get("X-Api-Key") or request.args.get("key", "")
    if API_KEY_EXPECTED and api_key != API_KEY_EXPECTED:
        return jsonify({"error": "unauthorized"}), 401
    filename = request.args.get("filename", "data.csv")
    if not DRIVE_FOLDER_ID:
        return jsonify({"error": "DRIVE_FOLDER_ID not configured"}), 500
    csv_bytes = request.get_data()
    if not csv_bytes:
        return jsonify({"error": "empty body"}), 400
    service = get_drive_service()
    file_metadata = {"name": filename, "parents": [DRIVE_FOLDER_ID]}
    media = MediaIoBaseUpload(io.BytesIO(csv_bytes), mimetype="text/csv", resumable=False)
    file = service.files().create(
        body=file_metadata,
        media_body=media,
        fields="id,name",
        supportsAllDrives=True
    ).execute()
    return jsonify({"id": file["id"], "name": file["name"]})

@app.route("/config", methods=["GET"])
def get_config():
    api_key = request.headers.get("X-Api-Key") or request.args.get("key", "")
    if API_KEY_EXPECTED and api_key != API_KEY_EXPECTED:
        return jsonify({"error": "unauthorized"}), 401
    filename = request.args.get("name", "AWTAS_CONFIG.TXT")
    if not filename:
        return jsonify({"error": "missing name parameter"}), 400
    if not DRIVE_FOLDER_ID:
        return jsonify({"error": "DRIVE_FOLDER_ID not configured"}), 500
    service = get_drive_service()
    query = (
        f"'{DRIVE_FOLDER_ID}' in parents and "
        f"name = '{filename}' and "
        "trashed = false"
    )
    result = service.files().list(
        q=query,
        spaces="drive",
        fields="files(id,name)",
        pageSize=1,
        orderBy="modifiedTime desc",
        supportsAllDrives=True,
        includeItemsFromAllDrives=True,
    ).execute()
    files = result.get("files", [])
    if not files:
        return jsonify({"error": "config_not_found", "name": filename}), 404
    file_id = files[0]["id"]
    raw = service.files().get_media(fileId=file_id).execute()
    text = raw.decode("utf-8", errors="ignore")
    compact = (request.args.get("compact", "1") or "").lower()
    if compact in ("1", "true", "yes"):
        keys = (
            "RANGE=",
            "ODR_HZ=",
            "TRIGGER_G=",
            "HPF=",
            "ACT_COUNT=",
            "OPERATION_MODE=",
            "MODE=",
            "FILE_MANUAL=",
            "FILE_AUTO=",
        )
        filtered = []
        for line in text.splitlines():
            stripped = line.lstrip()
            for k in keys:
                if stripped.startswith(k):
                    filtered.append(stripped.strip())
                    break
        payload = ("\r\n".join(filtered) + "\r\n").encode("ascii", errors="ignore")
        return app.response_class(payload, mimetype="text/plain")
    return app.response_class(raw, mimetype="text/plain")

@app.route("/config/init", methods=["POST", "GET"])
def init_config():
    api_key = request.headers.get("X-Api-Key") or request.args.get("key", "")
    if API_KEY_EXPECTED and api_key != API_KEY_EXPECTED:
        return jsonify({"error": "unauthorized"}), 401
    filename = request.args.get("name", "AWTAS_CONFIG.TXT")
    if not DRIVE_FOLDER_ID:
        return jsonify({"error": "DRIVE_FOLDER_ID not configured"}), 500
    service = get_drive_service()
    # Prefer local file content if present
    local_path = "/Users/pedroavendano/Desktop/LIND/AWTAS_REPO/AWTAS_CONFIG.TXT"
    content_bytes = None
    if os.path.exists(local_path):
        try:
            with open(local_path, "rb") as f:
                content_bytes = f.read()
        except Exception:
            content_bytes = None
    if not content_bytes:
        # Fallback template
        content_bytes = (
            "RANGE=2\r\n"
            "ODR_HZ=125\r\n"
            "TRIGGER_G=0.50\r\n"
            "HPF=OFF\r\n"
            "ACT_COUNT=5\r\n"
        ).encode("ascii", errors="ignore")
    query = (
        f"'{DRIVE_FOLDER_ID}' in parents and "
        f"name = '{filename}' and "
        "trashed = false"
    )
    result = service.files().list(
        q=query,
        spaces="drive",
        fields="files(id,name,modifiedTime)",
        orderBy="modifiedTime desc",
        pageSize=10,
        supportsAllDrives=True,
        includeItemsFromAllDrives=True,
    ).execute()
    files = result.get("files", [])
    if files:
        selected = files[0]
        others = files[1:]
        media = MediaIoBaseUpload(io.BytesIO(content_bytes), mimetype="text/plain", resumable=False)
        updated = service.files().update(
            fileId=selected["id"],
            media_body=media,
            fields="id,name",
            supportsAllDrives=True,
        ).execute()
        return jsonify({
            "status": "updated",
            "id": updated["id"],
            "name": updated["name"],
            "duplicates": [{"id": f["id"], "name": f["name"]} for f in others],
        })
    media = MediaIoBaseUpload(io.BytesIO(content_bytes), mimetype="text/plain", resumable=False)
    metadata = {"name": filename, "parents": [DRIVE_FOLDER_ID]}
    created = service.files().create(
        body=metadata,
        media_body=media,
        fields="id,name",
        supportsAllDrives=True,
    ).execute()
    return jsonify({"status": "created", "id": created["id"], "name": created["name"]})

@app.route("/config/delete", methods=["POST", "GET"])
def delete_config():
    api_key = request.headers.get("X-Api-Key") or request.args.get("key", "")
    if API_KEY_EXPECTED and api_key != API_KEY_EXPECTED:
        return jsonify({"error": "unauthorized"}), 401
    filename = request.args.get("name", "AWTAS_CONFIG.TXT")
    if not DRIVE_FOLDER_ID:
        return jsonify({"error": "DRIVE_FOLDER_ID not configured"}), 500
    service = get_drive_service()
    query = (
        f"'{DRIVE_FOLDER_ID}' in parents and "
        f"name = '{filename}' and "
        "trashed = false"
    )
    result = service.files().list(
        q=query,
        spaces="drive",
        fields="files(id,name)",
        pageSize=10,
        supportsAllDrives=True,
        includeItemsFromAllDrives=True,
    ).execute()
    files = result.get("files", [])
    deleted_count = 0
    for f in files:
        try:
            service.files().delete(fileId=f["id"], supportsAllDrives=True).execute()
            deleted_count += 1
        except Exception as e:
            print(f"Error deleting {f['id']}: {e}")
    return jsonify({"status": "deleted", "count": deleted_count, "name": filename})

if __name__ == "__main__":
    port = int(os.environ.get("PORT", "8080"))
    app.run(host="0.0.0.0", port=port)
