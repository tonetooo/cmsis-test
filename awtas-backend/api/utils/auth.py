"""
Servicio de autenticación y autorización
"""
from functools import wraps
from flask import request, jsonify
from config import get_config

config = get_config()

def require_api_key(f):
    """Decorador para requerir API key"""
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not config.ENABLE_API_KEY:
            return f(*args, **kwargs)
        
        api_key = request.headers.get('X-Api-Key') or request.args.get('key', '')
        
        if not api_key or api_key != config.DEVICE_API_KEY:
            return jsonify({"error": "unauthorized", "message": "Invalid or missing API key"}), 401
        
        return f(*args, **kwargs)
    return decorated_function

def check_drive_config():
    """Verificar que Drive está configurado"""
    if not config.DRIVE_FOLDER_ID:
        return jsonify({"error": "DRIVE_FOLDER_ID not configured"}), 500
    return None
