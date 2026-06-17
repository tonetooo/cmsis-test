"""
Rutas para manejar la configuración del sensor AWTAS en Google Drive
"""
from flask import Blueprint, request, jsonify
from config import get_config
from api.services import ConfigService
from api.utils import require_api_key, check_drive_config

config_bp = Blueprint('config', __name__)
config_inst = get_config()

@config_bp.route('/config', methods=['GET'])
@require_api_key
def get_config():
    """
    Obtener configuración del sensor desde Google Drive
    
    Parámetros query:
        - name: Nombre del archivo de configuración (por defecto: AWTAS_CONFIG.TXT)
        - compact: Si es 1/true, retorna solo las claves importantes (por defecto: 1)
    
    Headers:
        - X-Api-Key: API key para autenticación
    
    Retorna:
        Configuración en texto plano
    """
    # Verificar configuración
    error_response = check_drive_config()
    if error_response:
        return error_response
    
    try:
        filename = request.args.get('name', config_inst.DEFAULT_CONFIG_FILENAME)
        compact = (request.args.get('compact', '1') or '').lower()
        
        # Obtener configuración
        text = ConfigService.get_config_from_drive(filename)
        
        # Retornar configuración compacta si se solicita
        if compact in ('1', 'true', 'yes'):
            text = ConfigService.get_compact_config(text)
        
        return text, 200, {'Content-Type': 'text/plain; charset=utf-8'}
    
    except ValueError as e:
        return jsonify({"error": "config_not_found", "message": str(e)}), 404
    except Exception as e:
        return jsonify({"error": "get_config_failed", "message": str(e)}), 500

@config_bp.route('/config/init', methods=['POST', 'GET'])
@require_api_key
def init_config():
    """
    Inicializar o actualizar configuración en Google Drive
    
    Parámetros query:
        - name: Nombre del archivo de configuración (por defecto: AWTAS_CONFIG.TXT)
    
    Headers:
        - X-Api-Key: API key para autenticación
    
    Retorna:
        {
            "status": "created" | "updated",
            "file": { información del archivo }
        }
    """
    # Verificar configuración
    error_response = check_drive_config()
    if error_response:
        return error_response
    
    try:
        filename = request.args.get('name', config_inst.DEFAULT_CONFIG_FILENAME)
        
        # Intentar obtener configuración local
        local_config = ConfigService.get_local_config()
        content = local_config or ConfigService.DEFAULT_CONFIG
        
        # Actualizar o crear en Drive
        result = ConfigService.update_config_in_drive(content, filename)
        
        return jsonify({
            "status": result['status'],
            "file": {
                "id": result['file'].get('id'),
                "name": result['file'].get('name'),
                "createdTime": result['file'].get('createdTime')
            }
        }), 201
    
    except Exception as e:
        return jsonify({"error": "init_config_failed", "message": str(e)}), 500

@config_bp.route('/config/delete', methods=['POST', 'GET'])
@require_api_key
def delete_config():
    """
    Eliminar configuración de Google Drive
    
    Parámetros query:
        - name: Nombre del archivo a eliminar (por defecto: AWTAS_CONFIG.TXT)
    
    Headers:
        - X-Api-Key: API key para autenticación
    
    Retorna:
        {
            "status": "deleted",
            "count": Número de archivos eliminados,
            "name": Nombre del archivo
        }
    """
    # Verificar configuración
    error_response = check_drive_config()
    if error_response:
        return error_response
    
    try:
        filename = request.args.get('name', config_inst.DEFAULT_CONFIG_FILENAME)
        
        # Eliminar configuración
        deleted_count = ConfigService.delete_config_from_drive(filename)
        
        return jsonify({
            "status": "deleted",
            "count": deleted_count,
            "name": filename
        }), 200
    
    except Exception as e:
        return jsonify({"error": "delete_config_failed", "message": str(e)}), 500
