/**
 * Script principal para el dashboard AWTAS
 */

// Configuración
const CONFIG = {
    API_BASE: window.location.origin,
    API_KEY_ENABLED: document.querySelector('[data-api-key-enabled]')?.dataset.apiKeyEnabled === 'true'
};

// Elementos del DOM
const elements = {
    uploadForm: document.getElementById('uploadForm'),
    fileInput: document.getElementById('fileInput'),
    filenameInput: document.getElementById('filenameInput'),
    apiKeyInput: document.getElementById('apiKeyInput'),
    uploadResult: document.getElementById('uploadResult'),
    
    getConfigBtn: document.getElementById('getConfigBtn'),
    updateConfigBtn: document.getElementById('updateConfigBtn'),
    deleteConfigBtn: document.getElementById('deleteConfigBtn'),
    configApiKey: document.getElementById('configApiKey'),
    configOutput: document.getElementById('configOutput'),
    configContent: document.getElementById('configContent'),
    configResult: document.getElementById('configResult'),
    
    statusIndicator: document.getElementById('status-indicator'),
    driveStatus: document.getElementById('drive-status'),
    apiKeyStatus: document.getElementById('api-key-status')
};

/**
 * Mostrar mensaje de resultado
 */
function showResult(element, message, isSuccess = true) {
    element.textContent = message;
    element.className = `${element.id.includes('upload') ? 'upload-result' : 'config-result'} ${isSuccess ? 'success' : 'error'}`;
    element.classList.remove('hidden');
}

/**
 * Obtener headers para las peticiones API
 */
function getApiHeaders() {
    const headers = {
        'Content-Type': 'application/json'
    };
    
    if (CONFIG.API_KEY_ENABLED && elements.apiKeyInput) {
        const apiKey = elements.apiKeyInput.value || elements.configApiKey.value;
        if (apiKey) {
            headers['X-Api-Key'] = apiKey;
        }
    }
    
    return headers;
}

/**
 * Manejar envío de archivo
 */
if (elements.uploadForm) {
    elements.uploadForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        try {
            const file = elements.fileInput.files[0];
            if (!file) {
                showResult(elements.uploadResult, 'Por favor selecciona un archivo', false);
                return;
            }
            
            const filename = elements.filenameInput.value || file.name;
            const fileContent = await file.arrayBuffer();
            
            // Obtener API Key si es requerida
            let url = `${CONFIG.API_BASE}/upload?filename=${encodeURIComponent(filename)}`;
            let headers = {};
            
            if (CONFIG.API_KEY_ENABLED && elements.apiKeyInput) {
                const apiKey = elements.apiKeyInput.value;
                if (!apiKey) {
                    showResult(elements.uploadResult, 'API Key es requerida', false);
                    return;
                }
                headers['X-Api-Key'] = apiKey;
            }
            
            const response = await fetch(url, {
                method: 'POST',
                headers: headers,
                body: fileContent
            });
            
            const data = await response.json();
            
            if (response.ok) {
                showResult(
                    elements.uploadResult,
                    `✓ Archivo subido exitosamente. ID: ${data.id}`,
                    true
                );
                elements.uploadForm.reset();
            } else {
                showResult(
                    elements.uploadResult,
                    `Error: ${data.error || 'Error desconocido'}`,
                    false
                );
            }
        } catch (error) {
            showResult(elements.uploadResult, `Error: ${error.message}`, false);
        }
    });
}

/**
 * Obtener configuración
 */
if (elements.getConfigBtn) {
    elements.getConfigBtn.addEventListener('click', async () => {
        try {
            let url = `${CONFIG.API_BASE}/config?compact=0`;
            const headers = {};
            
            if (CONFIG.API_KEY_ENABLED && elements.configApiKey) {
                const apiKey = elements.configApiKey.value;
                if (!apiKey) {
                    showResult(elements.configResult, 'API Key es requerida', false);
                    return;
                }
                headers['X-Api-Key'] = apiKey;
            }
            
            const response = await fetch(url, {
                method: 'GET',
                headers: headers
            });
            
            if (response.ok) {
                const content = await response.text();
                elements.configContent.textContent = content;
                elements.configOutput.classList.remove('hidden');
                showResult(elements.configResult, '✓ Configuración obtenida', true);
            } else {
                const data = await response.json();
                showResult(
                    elements.configResult,
                    `Error: ${data.error || 'Error desconocido'}`,
                    false
                );
            }
        } catch (error) {
            showResult(elements.configResult, `Error: ${error.message}`, false);
        }
    });
}

/**
 * Actualizar configuración
 */
if (elements.updateConfigBtn) {
    elements.updateConfigBtn.addEventListener('click', async () => {
        try {
            let url = `${CONFIG.API_BASE}/config/init`;
            const headers = {};
            
            if (CONFIG.API_KEY_ENABLED && elements.configApiKey) {
                const apiKey = elements.configApiKey.value;
                if (!apiKey) {
                    showResult(elements.configResult, 'API Key es requerida', false);
                    return;
                }
                headers['X-Api-Key'] = apiKey;
            }
            
            const response = await fetch(url, {
                method: 'POST',
                headers: headers
            });
            
            const data = await response.json();
            
            if (response.ok) {
                showResult(
                    elements.configResult,
                    `✓ Configuración ${data.status} exitosamente. ID: ${data.file.id}`,
                    true
                );
            } else {
                showResult(
                    elements.configResult,
                    `Error: ${data.error || 'Error desconocido'}`,
                    false
                );
            }
        } catch (error) {
            showResult(elements.configResult, `Error: ${error.message}`, false);
        }
    });
}

/**
 * Eliminar configuración
 */
if (elements.deleteConfigBtn) {
    elements.deleteConfigBtn.addEventListener('click', async () => {
        if (!confirm('¿Estás seguro de que deseas eliminar la configuración?')) {
            return;
        }
        
        try {
            let url = `${CONFIG.API_BASE}/config/delete`;
            const headers = {};
            
            if (CONFIG.API_KEY_ENABLED && elements.configApiKey) {
                const apiKey = elements.configApiKey.value;
                if (!apiKey) {
                    showResult(elements.configResult, 'API Key es requerida', false);
                    return;
                }
                headers['X-Api-Key'] = apiKey;
            }
            
            const response = await fetch(url, {
                method: 'POST',
                headers: headers
            });
            
            const data = await response.json();
            
            if (response.ok) {
                showResult(
                    elements.configResult,
                    `✓ ${data.count} archivo(s) eliminado(s)`,
                    true
                );
                elements.configOutput.classList.add('hidden');
            } else {
                showResult(
                    elements.configResult,
                    `Error: ${data.error || 'Error desconocido'}`,
                    false
                );
            }
        } catch (error) {
            showResult(elements.configResult, `Error: ${error.message}`, false);
        }
    });
}

/**
 * Verificar estado del servidor al cargar
 */
document.addEventListener('DOMContentLoaded', async () => {
    try {
        const response = await fetch(`${CONFIG.API_BASE}/health`);
        const data = await response.json();
        
        if (response.ok) {
            if (elements.statusIndicator) {
                elements.statusIndicator.textContent = 'Operativo';
                elements.statusIndicator.className = 'status-badge healthy';
            }
            
            if (elements.driveStatus && data.drive_configured) {
                elements.driveStatus.textContent = 'Configurado';
                elements.driveStatus.className = 'status-badge healthy';
            }
        }
    } catch (error) {
        console.error('Error checking server health:', error);
        if (elements.statusIndicator) {
            elements.statusIndicator.textContent = 'Error';
            elements.statusIndicator.className = 'status-badge error';
        }
    }
});
