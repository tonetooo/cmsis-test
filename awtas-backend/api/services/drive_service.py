"""
Servicio para interactuar con Google Drive
"""
import os
import io
import time
from google.oauth2 import service_account
from googleapiclient.discovery import build
from googleapiclient.http import MediaIoBaseUpload
import google.auth
from config import get_config

config = get_config()

class DriveService:
    """Servicio para manejar operaciones con Google Drive"""
    
    def __init__(self):
        self.service = None
        self.initialize()
    
    def initialize(self):
        """Inicializar la conexión a Google Drive"""
        try:
            if os.path.exists(config.SERVICE_ACCOUNT_FILE):
                creds = service_account.Credentials.from_service_account_file(
                    config.SERVICE_ACCOUNT_FILE,
                    scopes=config.DRIVE_SCOPES
                )
            else:
                creds, _ = google.auth.default(scopes=config.DRIVE_SCOPES)
            
            self.service = build("drive", "v3", credentials=creds)
        except Exception as e:
            raise Exception(f"Error initializing Drive service: {str(e)}")
    
    def get_service(self):
        """Obtener instancia del servicio"""
        if self.service is None:
            self.initialize()
        return self.service
    
    def upload_file(self, file_content, filename, folder_id, max_retries=3):
        """
        Subir archivo a una carpeta específica en Google Drive
        
        Args:
            file_content: Contenido del archivo en bytes
            filename: Nombre del archivo
            folder_id: ID de la carpeta destino
            max_retries: Número máximo de reintentos
            
        Returns:
            Dict con información del archivo creado
        """
        last_exception = None
        
        for attempt in range(max_retries):
            try:
                service = self.get_service()
                file_metadata = {
                    "name": filename,
                    "parents": [folder_id]
                }
                media = MediaIoBaseUpload(
                    io.BytesIO(file_content),
                    mimetype="text/csv",
                    resumable=False
                )
                file = service.files().create(
                    body=file_metadata,
                    media_body=media,
                    fields="id,name,createdTime",
                    supportsAllDrives=True
                ).execute()
                return file
            except Exception as e:
                last_exception = e
                if attempt < max_retries - 1:
                    wait_time = 2 ** attempt
                    print(f"Upload attempt {attempt + 1} failed: {str(e)}. Retrying in {wait_time}s...")
                    time.sleep(wait_time)
                    self.service = None
                else:
                    raise Exception(f"Error uploading file after {max_retries} attempts: {str(last_exception)}")
    
    def get_file_by_name(self, filename, folder_id):
        """
        Obtener archivo por nombre dentro de una carpeta
        
        Args:
            filename: Nombre del archivo a buscar
            folder_id: ID de la carpeta
            
        Returns:
            Dict con información del archivo o None
        """
        try:
            service = self.get_service()
            query = (
                f"'{folder_id}' in parents and "
                f"name = '{filename}' and "
                f"trashed = false"
            )
            result = service.files().list(
                q=query,
                spaces="drive",
                fields="files(id,name,modifiedTime)",
                pageSize=10,
                orderBy="modifiedTime desc",
                supportsAllDrives=True,
                includeItemsFromAllDrives=True
            ).execute()
            files = result.get("files", [])
            return files
        except Exception as e:
            raise Exception(f"Error getting file: {str(e)}")
    
    def get_file_content(self, file_id):
        """
        Obtener contenido de un archivo
        
        Args:
            file_id: ID del archivo
            
        Returns:
            Contenido del archivo en bytes
        """
        try:
            service = self.get_service()
            content = service.files().get_media(fileId=file_id).execute()
            return content
        except Exception as e:
            raise Exception(f"Error getting file content: {str(e)}")
    
    def update_file(self, file_id, file_content):
        """
        Actualizar contenido de un archivo
        
        Args:
            file_id: ID del archivo a actualizar
            file_content: Nuevo contenido en bytes
            
        Returns:
            Dict con información del archivo actualizado
        """
        try:
            service = self.get_service()
            media = MediaIoBaseUpload(
                io.BytesIO(file_content),
                mimetype="text/plain",
                resumable=False
            )
            updated = service.files().update(
                fileId=file_id,
                media_body=media,
                fields="id,name,modifiedTime",
                supportsAllDrives=True
            ).execute()
            return updated
        except Exception as e:
            raise Exception(f"Error updating file: {str(e)}")
    
    def delete_file(self, file_id):
        """
        Eliminar un archivo
        
        Args:
            file_id: ID del archivo a eliminar
        """
        try:
            service = self.get_service()
            service.files().delete(
                fileId=file_id,
                supportsAllDrives=True
            ).execute()
        except Exception as e:
            raise Exception(f"Error deleting file: {str(e)}")
    
    def create_file(self, filename, file_content, folder_id):
        """
        Crear un nuevo archivo
        
        Args:
            filename: Nombre del archivo
            file_content: Contenido en bytes
            folder_id: ID de la carpeta destino
            
        Returns:
            Dict con información del archivo creado
        """
        try:
            service = self.get_service()
            metadata = {
                "name": filename,
                "parents": [folder_id]
            }
            media = MediaIoBaseUpload(
                io.BytesIO(file_content),
                mimetype="text/plain",
                resumable=False
            )
            created = service.files().create(
                body=metadata,
                media_body=media,
                fields="id,name,createdTime",
                supportsAllDrives=True
            ).execute()
            return created
        except Exception as e:
            raise Exception(f"Error creating file: {str(e)}")


# Instancia global del servicio
_drive_service = None

def get_drive_service():
    """Obtener instancia del servicio de Drive (singleton)"""
    global _drive_service
    if _drive_service is None:
        _drive_service = DriveService()
    return _drive_service
