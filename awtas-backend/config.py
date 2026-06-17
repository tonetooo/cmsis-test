"""
Configuración centralizada para el backend AWTAS
"""
import os
from dotenv import load_dotenv

# Cargar variables de entorno desde .env si existe
load_dotenv()

class Config:
    """Configuración base"""
    
    # Flask
    DEBUG = os.environ.get('DEBUG', 'False').lower() in ('true', '1', 'yes')
    HOST = os.environ.get('HOST', '0.0.0.0')
    PORT = int(os.environ.get('PORT', 8080))
    
    # Google Drive
    DRIVE_FOLDER_ID = os.environ.get('DRIVE_FOLDER_ID', '')
    SERVICE_ACCOUNT_FILE = os.environ.get('SERVICE_ACCOUNT_FILE', 'credentials.json')
    
    # API Security
    DEVICE_API_KEY = os.environ.get('DEVICE_API_KEY', '')
    ENABLE_API_KEY = os.environ.get('ENABLE_API_KEY', 'True').lower() in ('true', '1', 'yes')
    
    # Google Drive Scopes
    DRIVE_SCOPES = ["https://www.googleapis.com/auth/drive.file"]
    
    # Paths
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))
    CONFIG_DIR = os.path.join(BASE_DIR, 'config')
    
    # Default config file
    DEFAULT_CONFIG_FILENAME = 'AWTAS_CONFIG.TXT'
    
    # Cloudflare Tunnel
    CLOUDFLARE_TUNNEL_NAME = os.environ.get('CLOUDFLARE_TUNNEL_NAME', '')
    BACKEND_FIXED_URL = os.environ.get('BACKEND_FIXED_URL', '')

class DevelopmentConfig(Config):
    """Configuración para desarrollo"""
    DEBUG = True

class ProductionConfig(Config):
    """Configuración para producción"""
    DEBUG = False

# Seleccionar configuración según el ambiente
config_by_name = {
    'development': DevelopmentConfig,
    'production': ProductionConfig,
    'default': Config
}

def get_config():
    """Obtener la configuración actual"""
    env = os.environ.get('FLASK_ENV', 'development')
    return config_by_name.get(env, Config)
