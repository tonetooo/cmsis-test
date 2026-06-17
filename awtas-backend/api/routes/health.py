"""
Ruta de estado del servidor
"""
from flask import Blueprint, jsonify
from config import get_config

health_bp = Blueprint('health', __name__)
config = get_config()

@health_bp.route('/health', methods=['GET'])
def health():
    """Verificar estado del servidor"""
    return jsonify({
        "status": "healthy",
        "message": "Backend AWTAS is running",
        "drive_configured": bool(config.DRIVE_FOLDER_ID)
    }), 200

@health_bp.route('/', methods=['GET'])
def index():
    """Página de inicio"""
    return jsonify({
        "name": "AWTAS Backend",
        "version": "1.0.0",
        "endpoints": {
            "health": "/health",
            "upload": "/upload",
            "config": "/config",
            "dashboard": "/dashboard"
        }
    }), 200
