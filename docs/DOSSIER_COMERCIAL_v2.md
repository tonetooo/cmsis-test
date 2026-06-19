# HERMES-A1 — Dossier Comercial v2 (Conciso)

**AWTAS — Autonomous Wireless Triaxial Acquisition System**

| | |
|---|---|
| **Versión** | 2.0-lite |
| **Fecha** | Junio 2026 |
| **Estado** | Validado con datos de campo |

---

> **HERMES-A1** es un sistema de adquisición triaxial autónomo con conectividad 4G LTE, diseñado para operar en entornos industriales y remotos sin intervención humana. Captura vibraciones, las almacena localmente y las transmite a la nube automáticamente.

---

## Qué resuelve

| Problema | Solución HERMES-A1 |
|---|---|
| Ubicaciones sin WiFi/Ethernet | Conectividad 4G LTE nativa |
| Costo de desplazamiento para descargar datos | Upload automático a la nube vía celular |
| Datos procesados días después | Transmisión en tiempo real (~5 min por ciclo) |
| Condiciones adversas (-40°C a +85°C) | Diseño industrial, watchdog, reinicio automático |
| Monitoreo solo con presencia física | Dashboard web + configuración remota |




---

## Especificaciones clave

| Componente | Detalle |
|---|---|
| **Sensor** | ADXL355 — 3 ejes, 20-bit, ±2g/±4g/±8g, hasta 4000 Hz |
| **MCU** | STM32F446RE — ARM Cortex-M4 @ 180 MHz, 512 KB Flash, 128 KB SRAM |
| **RTOS** | FreeRTOS (CMSIS-RTOS v2) — 5 tareas concurrentes |
| **Almacenamiento** | SD FAT32 + buffer RAM 32 KB (redundante) |
| **Conectividad** | Quectel EC25 — 4G LTE Cat 4 (150/50 Mbps) |
| **Backend** | Flask API + Dashboard web + Google Drive |
| **Seguridad** | HTTPS TLS 1.3 (Cloudflare Tunnel), API Key |
| **Alimentación** | 5–24 VDC, < 2 W operación, < 0.5 W reposo |
| **Watchdog** | IWDG independiente, timeout ~33 s |

---

## Diferenciadores vs competencia

| | HERMES-A1 | Dataloggers típicos |
|---|---|---|
| **Conectividad** | 4G LTE nativa | USB presencial |
| **Autonomía** | Continua sin intervención | Visita periódica |
| **Config remota** | Cloud (parámetros en vivo) | Solo local por UART |
| **Wake-on-Motion** | Detección por umbral, bajo consumo | Muestreo continuo |
| **Almacenamiento** | SD + RAM + Cloud (3 niveles) | Solo SD |
| **Backend propio** | Flask + Dashboard | Dep. third-party |

---

## Arquitectura

```
ADXL355 ──SPI──► STM32F446RE ──SPI──► SD Card (CSV)
                   │ FreeRTOS
                   └──UART──► Quectel EC25 ──4G──► Cloudflare ──► Flask API ──► Google Drive
                                  │
                                  └──UART──► Consola (menú interactivo)
```

**Flujo de datos**: Sensor → Buffer RAM → SD → Módem Power On → Registro 4G (~60s) → HTTPS POST → Cloud → Power Off. Ciclo completo: ~3–5 min.

---

## Modos de operación

| Modo | Disparo | Consumo |
|---|---|---|
| **Manual** | Comando UART | Normal |
| **Autónomo** | Programa preconfigurado | Normal |
| **Wake-on-Motion** | Umbral de aceleración | < 0.5 W en espera |

---

## Resultados de campo (Marzo–Junio 2026)

| Métrica | Resultado |
|---|---|
| Archivos TRIG generados | 57 (13,896 B c/u) |
| Tasa de éxito upload | 100% (57/57) |
| Tiempo registro 4G | < 60 s típico |
| Voltaje operación | 12 VDC @ ~150 mA (~1.8 W) |
| Calidad señal (CSQ) | RSSI 20–31 / 31 (excelente) |
| Frecuencia muestreo | 125 Hz |
| Rango sensor | ±2g |

---

## Configuración remota (AWTAS_CONFIG.TXT)

| Parámetro | Valores | Defecto |
|---|---|---|
| `RANGE` | 2, 4, 8 | 2 |
| `ODR_HZ` | 31–4000 | 125 |
| `TRIGGER_G` | 0.01–8.0 | 0.50 |
| `HPF` | ON, OFF | OFF |
| `ACT_COUNT` | 1–255 | 5 |
| `OPERATION_MODE` | 1 (Manual), 2 (Auto) | 2 |

---

## Confiabilidad

- **Watchdog IWDG**: Previene hangs indefinidos (~33 s timeout)
- **Buffer RAM**: Protege datos ante corte de energía en SD
- **Reintento automático**: Si POST falla, reintenta en próximo ciclo
- **Modo degradado**: Sin red, datos permanecen en SD hasta recuperar conectividad
- **Reinicialización SD**: Si f_open falla, re-inicializa bus SPI automáticamente
- **Failsafe encendido**: Dual path (HAT_PWR_OFF + PWRKEY fallback)

---

## Modelos

| Modelo | Conectividad | Gabinete | Para |
|---|---|---|---|
| **Base** | 4G LTE | Abierto | Monitoreo remoto estándar |
| **Pro** | 4G LTE | IP65 | Exteriores, minería, industria |
| **Lite** | Solo SD | Abierto | Laboratorio, corta duración |
| **DevKit** | 4G LTE | Abierto + breakout | Desarrollo e integración |

**Paquete Base incluye**: Unidad, antena 4G, fuente 12VDC, guía de inicio rápido.

---

## Stack tecnológico

| Componente | Tecnología |
|---|---|
| MCU | STM32F446RE (ARM Cortex-M4) |
| RTOS | FreeRTOS (CMSIS-RTOS v2) |
| Sensor | ADXL355 (Analog Devices) |
| Módem | Quectel EC25 (4G LTE Cat 4) |
| Backend | Flask + Python |
| Cloud | Google Drive API v3 |
| Tunnel | Cloudflare (HTTPS + TLS 1.3) |
| File System | FatFs (ELM-Chan) |
| IDE | STM32CubeIDE |

---

## Contacto

**HERMES-A1** — AWTAS: Autonomous Wireless Triaxial Acquisition System  
**LIND SpA** · Concepción, Chile  
**Documento**: DOSSIER_COMERCIAL_v2.md · Junio 2026

---

*Información sujeta a cambios. Especificaciones según configuración del producto. Datos de validación: campaña Marzo–Junio 2026.*
