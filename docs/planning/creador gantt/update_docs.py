#!/usr/bin/env python3
"""Update documentation files to v1.2.0"""
import os, re

# Read current documentacion.md
with open("documentacion.md", "r", encoding="utf-8") as f:
    doc_content = f.read()

# Update version from v1.1.0 to v1.2.0
# Find the version line in the header
lines = doc_content.split('\n')
for i, line in enumerate(lines):
    if '**Versión:**' in line:
        lines[i] = line.replace('v1.1.0', 'v1.2.0')
        break

# Update the index section to include new sections
# Find the index section
for i, line in enumerate(lines):
    if '# Índice' in line:
        # Insert new sections after the existing ones
        insert_idx = i + 1
        new_sections = [
            '1. **Visión General**',
            '2. **Hardware:** sistema, hardware, evolución desde AWTAS.',
            '3. **Arquitectura de Software:** pines, periféricos, NVIC, niveles eléctricos. tareas, colas, eventos, flujo de datos.',
            '4. **Gestión de Memoria:** mapa de memoria, Heap_4, dimensionamiento de cola.',
            '5. **Estado DMA y FPU:** configuración actual, impacto, recomendaciones.',
            '6. **Correcciones Aplicadas:** 13 bugs identificados y resueltos (8 nuevos).',
            '7. **Análisis Comparativo:** 4 versiones del proyecto.',
            '8. **Evaluación UniKnect SDK:** viabilidad técnica.',
            '9. **Actividades Pendientes:** roadmap desde evaluación del prototipo.',
            '10. **Referencia API:** documentación Doxygen.',
            '11. **Compilación y Flash**.',
            '12. **Comandos CLI**.',
            '13. **Estructura del Proyecto**.',
            '14. **Rutas de Upload**.',
            '15. **Conocidos / Limitaciones**.'
        ]
        # Insert new sections
        for j, section in enumerate(new_sections):
            lines.insert(insert_idx + j, section)
        break

# Update the Correcciones Aplicadas section to include new fixes
# Find the Correcciones Aplicadas section
for i, line in enumerate(lines):
    if '# 6. Correcciones Aplicadas:' in line:
        # Find the table
        for j in range(i, len(lines)):
            if '|' in lines[j] and 'Problema' in lines[j]:
                # Insert new rows after existing ones
                insert_idx = j + 1
                new_fixes = [
                    '| 9 | .DONE marker no persiste post-NRST | quectel_drive.c | RTC backup registers BKP0R..BKP19R |',
                    '| 10 | NRST por Modem_PowerOff en ruta erronea | modem_task.c | PowerOff movido DESPUES de queue pop + .DONE |',
                    '| 11 | upload_queue_pop(NULL,0) UB | file_task.c | NULL guard: `if (buf != NULL && size > 0)` |',
                    '| 12 | Boot scan invisible (CONS_DBG disabled) | file_task.c | CONS_DBG → CONS_INFO for skip reasons |',
                    '| 13 | SD .DONE marker NAND timing | quectel_drive.c | HAL_Delay(50) after f_close |'
                ]
                for k, fix in enumerate(new_fixes):
                    lines.insert(insert_idx + k, fix)
                break
        break

# Update the Arquitectura de Software section
# Find the Arquitectura de Software section
for i, line in enumerate(lines):
    if '# 3. Arquitectura de Software:' in line:
        # Find the Tareas FreeRTOS table
        for j in range(i, len(lines)):
            if '| Tarea | Funcion | Prioridad CMSIS | Stack (words) | Stack (bytes) | Entrada |' in lines[j]:
                # Update the table
                lines[j] = '| Tarea | Funcion | Prioridad CMSIS | Stack (bytes) | Entrada |'
                # Update the rows
                for k in range(j + 1, j + 6):
                    if k < len(lines) and '| sensor_task |' in lines[k]:
                        lines[k] = '| sensor_task | StartSensorTask | osPriorityHigh (40) | 4096 | EVT_MOTION_DETECTED (EXTI) |'
                    elif k < len(lines) and '| modem_task |' in lines[k]:
                        lines[k] = '| modem_task | StartModemTask | osPriorityAboveNormal (32) | 8192 | EVT_FILE_READY |'
                    elif k < len(lines) and '| file_task |' in lines[k]:
                        lines[k] = '| file_task | StartFileTask | osPriorityNormal (24) | 6144 | Boot scan + idle loop |'
                    elif k < len(lines) and '| control_task |' in lines[k]:
                        lines[k] = '| control_task | StartControlTask | osPriorityNormal (24) | 4096 | UART2 (CLI) |'
                    elif k < len(lines) and '| defaultTask |' in lines[k]:
                        lines[k] = '| defaultTask | StartDefaultTask | osPriorityNormal (24) | 512 | Idle loop osDelay(1000) |'
                break
        break

# Update the Gestión de Memoria section
# Find the Gestión de Memoria section
for i, line in enumerate(lines):
    if '# 4. Gestión de Memoria:' in line:
        # Find the Distribución del Heap FreeRTOS section
        for j in range(i, len(lines)):
            if 'configTOTAL_HEAP_SIZE = 40960' in lines[j]:
                # Update the table
                lines[j] = 'configTOTAL_HEAP_SIZE = 40960 (40 KB) de los 128 KB de SRAM disponibles.'
                # Find the table
                for k in range(j + 1, j + 20):
                    if k < len(lines) and '| Componente | Tamaño | Cálculo |' in lines[k]:
                        # Update the table
                        lines[k] = '| Componente | Tamaño | Cálculo |'
                        lines[k + 1] = '| sensor_task stack | 4096 B | 1024 words x 4 bytes |'
                        lines[k + 2] = '| modem_task stack | 8192 B | 2048 words x 4 bytes |'
                        lines[k + 3] = '| file_task stack | 6144 B | 1536 words x 4 bytes |'
                        lines[k + 4] = '| control_task stack | 4096 B | 1024 words x 4 bytes |'
                        lines[k + 5] = '| default Task stack | 512 B | 128 words x 4 bytes |'
                        lines[k + 6] = '| upload_queue | 256 B | 8 slots x 32 bytes |'
                        lines[k + 7] = '| Queue overhead | ~200 B | Internal FreeRTOS struct |'
                        lines[k + 8] = '| Mutex + EventFlags | ~200 B | Internal structs |'
                        lines[k + 9] = '| **Total asignado** | **~24,500 B** | **~60% del heap de 40 KB** |'
                        lines[k + 10] = '| **Heap libre** | **~15,500 B** | **Disponible para malloc dinámico** |'
                        break
                break
        break

# Update the Actividades Pendientes section
# Find the Actividades Pendientes section
for i, line in enumerate(lines):
    if '# 9. Actividades Pendientes:' in line:
        # Find the Prioridad 1 section
        for j in range(i, len(lines)):
            if '## 9.1 Prioridad 1 - Crítico' in lines[j]:
                # Update the table
                for k in range(j + 1, j + 30):
                    if k < len(lines) and '| 9.1.1 | DMA para SPI2' in lines[k]:
                        lines[k] = '| 9.1.1 | DMA para SPI2 | ✅ COMPLETADO |'
                    elif k < len(lines) and '| 9.1.2 | Habilitar FPU' in lines[k]:
                        lines[k] = '| 9.1.2 | Habilitar FPU | ✅ COMPLETADO |'
                    elif k < len(lines) and '| 9.1.3 | Adquisición en tiempo continuo' in lines[k]:
                        lines[k] = '| 9.1.3 | Adquisición en tiempo continuo | 🔄 EN PROGRESO (85%) |'
                    elif k < len(lines) and '| 9.1.4 | Calibración del acelerómetro' in lines[k]:
                        lines[k] = '| 9.1.4 | Calibración del acelerómetro | 🔄 INICIADO (10%) |'
                    elif k < len(lines) and '| 9.1.5 | Watchdog' in lines[k]:
                        lines[k] = '| 9.1.5 | Watchdog | ✅ COMPLETADO |'
                    elif k < len(lines) and '| 9.1.6 | Buffer local ante pérdida de conectividad' in lines[k]:
                        lines[k] = '| 9.1.6 | Buffer local ante pérdida de conectividad | ✅ COMPLETADO |'
                    elif k < len(lines) and '| 9.1.7 | Sincronización horaria' in lines[k]:
                        lines[k] = '| 9.1.7 | Sincronización horaria | 🔄 EN PROGRESO (40%) — servers sometimes fail |'
                    elif k < len(lines) and '| 9.1.8 | Optimización del consumo energético' in lines[k]:
                        lines[k] = '| 9.1.8 | Optimización del consumo energético | 🔄 INICIADO (30%) |'
                break
        break

# Write updated documentacion.md
with open("documentacion.md", "w", encoding="utf-8") as f:
    f.write('\n'.join(lines))

print("Updated documentacion.md to v1.2.0")

# Update act pendientes.md
with open("act pendientes.md", "r", encoding="utf-8") as f:
    act_content = f.read()

# Update version from 1.0 to 1.2.0
act_content = act_content.replace("Versión del documento: Versión 1.0", "Versión del documento: Versión 1.2.0")

# Update the activities to mark completed ones
# Find the 1.1.1 section
act_lines = act_content.split('\n')
for i, line in enumerate(act_lines):
    if "**1. Implementación de adquisición continua**" in line:
        # Update status
        act_lines[i + 1] = "Incorporar un modo de operación que permita la adquisición permanente de datos del acelerómetro, registrando señales de vibración de forma continua sin depender exclusivamente de activaciones por evento."
        break

# Write updated act pendientes.md
with open("act pendientes.md", "w", encoding="utf-8") as f:
    f.write('\n'.join(act_lines))

print("Updated act pendientes.md to v1.2.0")

# Generate PDF from HTML
import subprocess
import sys

# Check if we have wkhtmltopdf installed
pdf_path = "documentacion.pdf"
if os.path.exists(pdf_path):
    os.remove(pdf_path)

# Try to generate PDF using wkhtmltopdf
try:
    subprocess.run(["wkhtmltopdf", "documentacion.html", pdf_path], check=True)
    print(f"Generated PDF: {pdf_path}")
except:
    print("wkhtmltopdf not available, skipping PDF generation")

print("Documentation update complete!")