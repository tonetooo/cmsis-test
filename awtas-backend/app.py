"""
Aplicación principal Flask para el backend AWTAS
"""
import os
from flask import Flask, render_template
from config import get_config

config = get_config()

def create_app():
    """Factory para crear la aplicación Flask"""
    
    app = Flask(__name__, 
                template_folder='web/templates',
                static_folder='web/static')
    
    # Configuración
    app.config.from_object(config)
    
    # Registrar rutas API
    from api.routes import create_routes
    for blueprint in create_routes():
        app.register_blueprint(blueprint)
    
    # Ruta para el dashboard web
    @app.route('/dashboard')
    def dashboard():
        """Página de dashboard"""
        return render_template('dashboard.html', config={
            'api_key_enabled': config.ENABLE_API_KEY,
            'drive_configured': bool(config.DRIVE_FOLDER_ID)
        })
    
    # Manejo de errores
    @app.errorhandler(404)
    def not_found(error):
        return {"error": "not_found", "message": "Endpoint no encontrado"}, 404
    
    @app.errorhandler(500)
    def internal_error(error):
        return {"error": "internal_server_error", "message": "Error interno del servidor"}, 500
    
    return app

if __name__ == '__main__':
    app = create_app()
    port = int(os.environ.get('PORT', 8080))
    app.run(
        host=config.HOST,
        port=port,
        debug=config.DEBUG
    )
