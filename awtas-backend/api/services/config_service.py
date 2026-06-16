"""
Servicio para manejar la configuración del sensor AWTAS
"""
import os
from config import get_config
from .drive_service import get_drive_service

config = get_config()

class ConfigService:
    """Servicio para manejar la configuración AWTAS"""
    
    COMPACT_KEYS = [
        "RANGE=",
        "ODR_HZ=",
        "TRIGGER_G=",
        "HPF=",
        "ACT_COUNT=",
        "OPERATION_MODE=",
        "MODE=",
        "FILE_MANUAL=",
        "FILE_AUTO=",
    ]
    
    DEFAULT_CONFIG = """###########################################################
# AWTAS CONFIG - Archivo de configuración remota del sensor
#
# Formato general: CLAVE=VALOR (una por línea)
###########################################################
RANGE=2
ODR_HZ=125
TRIGGER_G=0.50
HPF=OFF
ACT_COUNT=5
OPERATION_MODE=2
FILE_MANUAL=CSV
FILE_AUTO=CSV
"""
    
    @staticmethod
    def get_config_from_drive(filename=None):
        """
        Obtener configuración desde Google Drive
        
        Args:
            filename: Nombre del archivo de configuración
            
        Returns:
            Contenido del archivo de configuración
        """
        if not filename:
            filename = config.DEFAULT_CONFIG_FILENAME
        
        if not config.DRIVE_FOLDER_ID:
            raise ValueError("DRIVE_FOLDER_ID not configured")
        
        drive_service = get_drive_service()
        files = drive_service.get_file_by_name(filename, config.DRIVE_FOLDER_ID)
        
        if not files:
            raise ValueError(f"Configuration file '{filename}' not found in Drive")
        
        file_id = files[0]["id"]
        content = drive_service.get_file_content(file_id)
        return content.decode('utf-8', errors='ignore')
    
    @staticmethod
    def get_compact_config(full_text):
        """
        Obtener configuración compacta (solo las claves importantes)
        
        Args:
            full_text: Texto completo de la configuración
            
        Returns:
            Configuración compacta
        """
        filtered = []
        for line in full_text.splitlines():
            stripped = line.lstrip()
            for key in ConfigService.COMPACT_KEYS:
                if stripped.startswith(key):
                    filtered.append(stripped.strip())
                    break
        
        return "\r\n".join(filtered) + "\r\n"
    
    @staticmethod
    def update_config_in_drive(content, filename=None):
        """
        Actualizar o crear configuración en Google Drive
        
        Args:
            content: Contenido de la configuración
            filename: Nombre del archivo de configuración
            
        Returns:
            Información del archivo actualizado
        """
        if not filename:
            filename = config.DEFAULT_CONFIG_FILENAME
        
        if not config.DRIVE_FOLDER_ID:
            raise ValueError("DRIVE_FOLDER_ID not configured")
        
        content_bytes = content.encode('utf-8', errors='ignore')
        drive_service = get_drive_service()
        
        # Buscar si ya existe
        files = drive_service.get_file_by_name(filename, config.DRIVE_FOLDER_ID)
        
        if files:
            # Actualizar el primer archivo encontrado
            file_id = files[0]["id"]
            updated = drive_service.update_file(file_id, content_bytes)
            
            # Eliminar duplicados si existen
            for f in files[1:]:
                try:
                    drive_service.delete_file(f["id"])
                except:
                    pass
            
            return {"status": "updated", "file": updated}
        else:
            # Crear nuevo archivo
            created = drive_service.create_file(filename, content_bytes, config.DRIVE_FOLDER_ID)
            return {"status": "created", "file": created}
    
    @staticmethod
    def delete_config_from_drive(filename=None):
        """
        Eliminar configuración de Google Drive
        
        Args:
            filename: Nombre del archivo de configuración
            
        Returns:
            Número de archivos eliminados
        """
        if not filename:
            filename = config.DEFAULT_CONFIG_FILENAME
        
        if not config.DRIVE_FOLDER_ID:
            raise ValueError("DRIVE_FOLDER_ID not configured")
        
        drive_service = get_drive_service()
        files = drive_service.get_file_by_name(filename, config.DRIVE_FOLDER_ID)
        
        deleted_count = 0
        for f in files:
            try:
                drive_service.delete_file(f["id"])
                deleted_count += 1
            except:
                pass
        
        return deleted_count
    
    @staticmethod
    def get_local_config(local_path=None):
        """
        Obtener configuración desde archivo local
        
        Args:
            local_path: Ruta del archivo local
            
        Returns:
            Contenido del archivo o None
        """
        if not local_path:
            local_path = os.path.join(config.CONFIG_DIR, config.DEFAULT_CONFIG_FILENAME)
        
        if os.path.exists(local_path):
            try:
                with open(local_path, 'r') as f:
                    return f.read()
            except:
                return None
        return None
