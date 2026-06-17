"""API Routes"""
from flask import Blueprint

def create_routes():
    """Crear y registrar todas las rutas"""
    from .upload import upload_bp
    from .config import config_bp
    from .health import health_bp
    from .memory import memory_bp

    return [upload_bp, config_bp, health_bp, memory_bp]
