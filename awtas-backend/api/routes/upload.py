"""
Ruta para subir archivos a Google Drive
"""
import struct
from flask import Blueprint, request, jsonify
from config import get_config
from api.services import get_drive_service
from api.utils import require_api_key, check_drive_config

upload_bp = Blueprint('upload', __name__)
config = get_config()

# Binary format: packed struct per sample (28 bytes)
# uint32_t timestamp_ms, float x_g, y_g, z_g, voltage, current, power
BIN_SAMPLE_FMT = '<I6f'  # little-endian: 1 uint32 + 6 floats (timestamp, x,y,z, voltage,current,power)
BIN_SAMPLE_SIZE = struct.calcsize(BIN_SAMPLE_FMT)  # 28 bytes

def convert_bin_to_csv(bin_data):
    """Convert binary sensor data to CSV format."""
    header = "timestamp_rel_s;timestamp_abs;unix_time;x_g;y_g;z_g;voltaje;corriente;potencia\r\n"
    lines = [header]
    
    sample_count = len(bin_data) // BIN_SAMPLE_SIZE
    for i in range(sample_count):
        offset = i * BIN_SAMPLE_SIZE
        sample = struct.unpack_from(BIN_SAMPLE_FMT, bin_data, offset)
        
        ts_ms = sample[0]
        x_g, y_g, z_g = sample[1], sample[2], sample[3]
        voltage, current, power = sample[4], sample[5], sample[6]
        
        rel_sec = ts_ms // 1000
        rel_ms = ts_ms % 1000
        abs_sec = 1767817653 + rel_sec  # Unix epoch offset
        
        line = f"{rel_sec}.{rel_ms:03d};{abs_sec}.{rel_ms:03d};{abs_sec}.{rel_ms:03d};{x_g:.6f};{y_g:.6f};{z_g:.6f};{voltage:.2f};{current:.2f};{power:.2f}\r\n"
        lines.append(line)
    
    return ''.join(lines).encode('utf-8')


@upload_bp.route('/upload', methods=['POST'])
@require_api_key
def upload():
    """
    Subir archivo a Google Drive (CSV o BIN→CSV)
    
    Parámetros query:
        - filename: Nombre del archivo (por defecto: data.csv)
        - key: API key alternativa (query param)
    
    Headers:
        - X-Api-Key: API key para autenticación
    
    Body: Contenido del archivo en bytes
    
    Si filename termina en .BIN, convierte a CSV automáticamente.
    
    Retorna:
        {
            "id": "ID del archivo en Drive",
            "name": "Nombre del archivo",
            "status": "success",
            "samples": N (solo para BIN)
        }
    """
    # Verificar configuración
    error_response = check_drive_config()
    if error_response:
        return error_response
    
    try:
        filename = request.args.get('filename', 'data.csv')
        file_content = request.get_data()
        
        if not file_content:
            return jsonify({"error": "empty body", "message": "El cuerpo de la solicitud está vacío"}), 400
        
        # BIN → CSV conversion
        samples_count = None
        if filename.upper().endswith('.BIN'):
            if len(file_content) % BIN_SAMPLE_SIZE != 0:
                return jsonify({
                    "error": "invalid_binary",
                    "message": f"Binary data size {len(file_content)} is not a multiple of {BIN_SAMPLE_SIZE}"
                }), 400
            
            samples_count = len(file_content) // BIN_SAMPLE_SIZE
            file_content = convert_bin_to_csv(file_content)
            # Rename to .CSV for Drive
            filename = filename[:-4] + '.CSV'
        
        # Subir archivo
        drive_service = get_drive_service()
        file_info = drive_service.upload_file(
            file_content,
            filename,
            config.DRIVE_FOLDER_ID
        )
        
        result = {
            "status": "success",
            "id": file_info.get('id'),
            "name": file_info.get('name'),
            "createdTime": file_info.get('createdTime')
        }
        if samples_count is not None:
            result["samples"] = samples_count
            result["original_format"] = "binary"
        
        return jsonify(result), 201
    
    except Exception as e:
        return jsonify({"error": "upload_failed", "message": str(e)}), 500
