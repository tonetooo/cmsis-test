#!/usr/bin/env python3
"""Generate HERMES-A1 documentation HTML v1.2.0"""
import os, sys

CSS = r"""
:root {
  --bg: #0d1117; --bg2: #161b22; --bg3: #21262d;
  --text: #c9d1d9; --muted: #8b949e; --bright: #f0f6fc;
  --accent: #58a6ff; --accent2: #79c0ff;
  --green: #238636; --yellow: #9e6a03; --blue: #1f6feb;
  --border: #30363d;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);line-height:1.7;font-size:15px}
a{color:var(--accent);text-decoration:none}a:hover{color:var(--accent2);text-decoration:underline}
.ctn{max-width:1000px;margin:0 auto;padding:20px 24px}
header{text-align:center;padding:48px 24px 32px;border-bottom:1px solid var(--border)}
header h1{font-size:2.2em;color:var(--bright);margin-bottom:8px;letter-spacing:-.5px}
header .sub{color:var(--accent);font-size:1.1em;margin-bottom:4px}
header .meta{color:var(--muted);font-size:.9em}
h2{color:var(--accent);font-size:1.5em;margin:40px 0 16px;padding-bottom:8px;border-bottom:1px solid var(--border)}
h3{color:var(--bright);font-size:1.15em;margin:24px 0 12px}
h4{color:var(--bright);font-size:1em;margin:16px 0 8px}
p{margin-bottom:12px}
table{width:100%;border-collapse:collapse;margin:12px 0 20px;font-size:.9em}
th{background:var(--bg2);color:var(--bright);padding:10px 12px;text-align:left;border:1px solid var(--border);font-weight:600}
td{padding:8px 12px;border:1px solid var(--border);vertical-align:top}
tr:nth-child(even) td{background:var(--bg2)}
tr:hover td{background:var(--bg3)}
code{font-family:'JetBrains Mono','Fira Code',Consolas,monospace;background:var(--bg2);padding:2px 6px;border-radius:4px;font-size:.88em;color:var(--accent)}
pre{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:16px;overflow-x:auto;margin:12px 0 20px;font-size:.88em;line-height:1.5}
pre code{background:none;padding:0;color:var(--text)}
.bdg{display:inline-block;padding:2px 10px;border-radius:12px;font-size:.82em;font-weight:600;white-space:nowrap}
.bdg-ok{background:var(--green);color:#fff}
.bdg-wip{background:var(--yellow);color:#fff}
.bdg-todo{background:var(--blue);color:#fff}
.toc{background:var(--bg2);border:1px solid var(--border);border-radius:8px;padding:20px 24px;margin:24px 0}
.toc h3{margin-top:0;color:var(--accent)}
.toc ol{padding-left:20px}.toc li{margin:4px 0}.toc ol ol{margin-top:4px}
blockquote{border-left:3px solid var(--accent);padding:12px 16px;background:var(--bg2);border-radius:0 8px 8px 0;margin:12px 0;color:var(--muted);font-style:italic}
hr{border:none;border-top:1px solid var(--border);margin:32px 0}
.note{background:var(--bg2);border-left:3px solid var(--yellow);padding:12px 16px;border-radius:0 8px 8px 0;margin:12px 0}
.note strong{color:var(--yellow)}
footer{text-align:center;padding:32px;margin-top:40px;border-top:1px solid var(--border);color:var(--muted);font-size:.85em}
@media print{body{background:#fff;color:#1a1a1a;font-size:11pt}.ctn{max-width:none;padding:0}h2{color:#0066cc;border-bottom-color:#ccc;page-break-after:avoid}h3{page-break-after:avoid}table{page-break-inside:avoid}pre{background:#f5f5f5;border-color:#ccc}.toc{page-break-after:always}.bdg-ok{background:#d4edda;color:#155724}.bdg-wip{background:#fff3cd;color:#856404}.bdg-todo{background:#cce5ff;color:#004085}a{color:#0066cc}.note{background:#f8f9fa}}
@media(max-width:768px){header h1{font-size:1.5em}table{font-size:.8em}th,td{padding:6px 8px}}
"""

# Section group headers for pinout table
GRP_SPI1 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">SPI1 &mdash; microSD</td></tr>'
GRP_SPI2 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">SPI2 &mdash; ADXL355</td></tr>'
GRP_U1 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">USART1 &mdash; Quectel EC25</td></tr>'
GRP_U2 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">USART2 &mdash; CLI / Debug</td></tr>'
GRP_G1 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">GPIO &mdash; Control Modem</td></tr>'
GRP_G2 = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">GPIO &mdash; ADXL355</td></tr>'
GRP_DBG = '<tr><td colspan="5" style="background:var(--bg3);font-weight:600">Depuracion &mdash; SWD</td></tr>'

def tbl(rows, header=True):
    """Build a <table> from list of lists. First row = header."""
    h = "<table>\n"
    for i, r in enumerate(rows):
        tag = "th" if (i == 0 and header) else "td"
        h += "<tr>" + "".join(f"<{tag}>{c}</{tag}>" for c in r) + "</tr>\n"
    h += "</table>\n"
    return h

def bdg(text, cls):
    return f'<span class="bdg bdg-{cls}">{text}</span>'

BODY = []
def w(s=""): BODY.append(s)

def build():
    # ── Header ──
    w("<!DOCTYPE html>")
    w('<html lang="es">')
    w("<head>")
    w('<meta charset="UTF-8">')
    w('<meta name="viewport" content="width=device-width, initial-scale=1.0">')
    w("<title>HERMES A1 &mdash; Documentacion Tecnica v1.2.0</title>")
    w(f"<style>{CSS}</style>")
    w("</head>")
    w("<body>")
    w("<header>")
    w("<h1>HERMES A1</h1>")
    w('<div class="sub">Sistema Autonomo de Adquisicion y Telemetria</div>')
    w('<div class="meta">STM32F446RET6 &bull; FreeRTOS &bull; ADXL355 &bull; Quectel EC25</div>')
    w('<div class="meta" style="margin-top:12px"><strong>Documentacion Tecnica v1.2.0</strong></div>')
    w('<div class="meta">Antonio Avendano Sanhueza &bull; LIND Engineering &bull; Junio 2026 &bull; Concepcion, Chile</div>')
    w("</header>")

    w('<div class="ctn">')

    # ── TOC ──
    toc_items = [
        ("s1", "Vision General"),
        ("s2", "Hardware", [("s2.1","Asignacion de Perifericos"),("s2.2","Pinout Completo"),("s2.3","Niveles Electricos"),("s2.4","Configuracion NVIC")]),
        ("s3", "Arquitectura de Software", [("s3.1","Tareas FreeRTOS"),("s3.2","Objetivos de Sincronizacion"),("s3.3","Event Flags"),("s3.4","Flujo de Datos"),("s3.5","Tracking de Upload &mdash; Backup Registers"),("s3.6","Boot Scan &mdash; Recuperacion post-reset"),("s3.7","Root Cause del NRST")]),
        ("s4", "Gestion de Memoria"),
        ("s5", "Estado DMA y FPU"),
        ("s6", "Correcciones de Codigo"),
        ("s7", "Analisis Comparativo"),
        ("s8", "Evaluacion UniKnect SDK"),
        ("s9", "Actividades Pendientes"),
        ("s10", "Referencia API"),
        ("s11", "Compilacion y Flash"),
        ("s12", "Comandos CLI"),
        ("s13", "Estructura del Proyecto"),
        ("s14", "Rutas de Upload"),
        ("s15", "Conocidos y Limitaciones"),
    ]
    w('<nav class="toc"><h3>Indice</h3><ol>')
    for item in toc_items:
        if len(item) == 2:
            w(f'<li><a href="#{item[0]}">{item[1]}</a></li>')
        else:
            w(f'<li><a href="#{item[0]}">{item[1]}</a><ol>')
            for sub in item[2]:
                w(f'<li><a href="#{sub[0]}">{sub[1]}</a></li>')
            w("</ol></li>")
    w("</ol></nav><hr>")

    # ── S1: Vision General ──
    w('<h2 id="s1">1. Vision General</h2>')
    w("<p>Firmware para STM32F446RET6 (Cortex-M4) del sistema HERMES A1. Adquisicion autonomica de vibraciones y telemetria via 4G/LTE. Desarrollado por Antonio Avendano Sanhueza para LIND Engineering, Concepcion, Chile.</p>")
    w("<h3>1.1 Componentes del Sistema</h3>")
    w(tbl([["Componente","Modelo","Interfaz","Funcion"],
           ["MCU","STM32F446RET6","N/A","Procesamiento, control, RTOS"],
           ["Acelerometro","ADXL355","SPI2 (8 MHz)","Medicion de vibracion 3 ejes, 2/4/8g"],
           ["Modem","Quectel EC25","USART1 (115200 baud)","Conectividad 4G/LTE, HTTP POST"],
           ["Almacenamiento","microSD","SPI1 (8 MHz) + FatFs","Registro CSV de lecturas"],
           ["Debug/CLI","N/A","USART2 (115200 baud)","Interfaz de comandos, diagnostico"]]))

    w("<h3>1.2 Principio de Operacion</h3>")
    w('<p>El sistema opera en modo <strong>triggered acquisition</strong>: el ADXL355 genera una interrupcion (INT1) cuando la aceleracion supera un umbral configurable. El firmware lee el FIFO del acelerometro, almacena las lecturas en SD como archivos CSV, y las transmite a un servidor remoto via HTTP/4G.</p>')
    w('<p>El pipeline de datos es <strong>asincrono y concurrente</strong>: la adquisicion del sensor, la escritura en SD, la transmision por modem y la interfaz de usuario operan como tareas independientes de FreeRTOS, coordinadas mediante colas, mutex y event flags.</p>')
    w('<p><strong>V1.2.0:</strong> La cola de upload es ahora un buffer circular en RAM (8 slots) que reemplaza la anterior cola basada en SD. El tracking de uploads se realiza con RTC backup registers (BKP0R..BKP19R) que sobreviven resets NRST. La adquisicion del sensor y la transmision por modem operan concurrentemente.</p>')

    w("<h3>1.3 Evolucion: AWTAS &rarr; HERMES A1</h3>")
    w(tbl([["Dimension","AWTAS (predecesor)","HERMES A1 (actual)"],
           ["Arquitectura","Baremetal, super-loop en main.c (~58 KB)","FreeRTOS + CMSIS-RTOS v2, 5 tareas"],
           ["Concurrencia","Secuencial: sensor &rarr; SD &rarr; modem (bloqueante)","Concurrente: cola RAM + event flags + mutex"],
           ["CLI","Menu de 1 caracter (m, I, r, o, t, i, q)","Comandos completos (help, status, accel, trigger, sdtest)"],
           ["Sincronizacion","Variables volatile + busy-wait","osMessageQueue, osEventFlags, osMutex"],
           ["Tracking Upload","QUEUE.TXT en SD","RAM queue + RTC backup registers"],
           ["Reset Safety","Perdida de estado en reset","Backup registers sobreviven NRST/IWDG"]]))
    w("<hr>")

    # ── S2: Hardware ──
    w('<h2 id="s2">2. Hardware</h2>')
    w('<h3 id="s2.1">2.1 Asignacion de Perifericos</h3>')
    w(tbl([["Periferico","Pines","Modo","Velocidad","Target","Nivel Logico"],
           ["SPI1","PA5, PA6, PA7, PB6(CS)","Full-Duplex Master","8.0 MB/s","microSD","3.3V"],
           ["SPI2","PB13, PB14, PB15, PA4(CS)","Full-Duplex Master","8.0 MB/s","ADXL355","3.3V"],
           ["USART1","PA9(TX), PA10(RX)","Asincrono","115200 baud","Quectel EC25","1.8V (modem)"],
           ["USART2","PA2(TX), PA3(RX)","Asincrono","115200 baud","CLI/Debug","3.3V"]]))

    w('<h3 id="s2.2">2.2 Pinout Completo</h3>')
    w(tbl([["Pin","Etiqueta","Funcion","Configuracion","Destino"],
           [GRP_SPI1],
           ["PA5","SPI1_SCK","Clock","AF5, Push-Pull","SD CLK"],
           ["PA6","SPI1_MISO","Data In","AF5","SD MISO"],
           ["PA7","SPI1_MOSI","Data Out","AF5","SD MOSI"],
           ["PB6","SD_CS","Chip Select","Output PP, High, Speed HIGH","SD CS"],
           [GRP_SPI2],
           ["PA4","ADXL_CS","Chip Select","Output PP, High, Speed HIGH","ADXL355 CS"],
           ["PB13","SPI2_SCK","Clock","AF5","ADXL355 SCLK"],
           ["PB14","SPI2_MISO","Data In","AF5","ADXL355 MISO"],
           ["PB15","SPI2_MOSI","Data Out","AF5","ADXL355 MOSI"],
           [GRP_U1],
           ["PA9","USART1_TX","Transmit","AF7","EC25 RX"],
           ["PA10","USART1_RX","Receive","AF7","EC25 TX"],
           [GRP_U2],
           ["PA2","USART2_TX","Transmit","AF7","USB-serial RX"],
           ["PA3","USART2_RX","Receive","AF7","USB-serial TX"],
           [GRP_G1],
           ["PB0","HAT_PWR_OFF","Apagado EC25","Output PP, Low, Pull Down","EC25 PWR_OFF"],
           ["PB1","MODEM_PWRKEY","Encendido EC25","Output PP, Low, Pull Down","EC25 PWRKEY"],
           ["PB2","MODEM_RI","Ring Indicator","Input EXTI Falling, Pull Up","EC25 RI"],
           [GRP_G2],
           ["PC0","ADXL_DRDY","Data Ready","Input","ADXL355 DRDY"],
           ["PC7","ADXL_INT1","Interrupcion","Input EXTI (prio 5), Pull Down","ADXL355 INT1"],
           [GRP_DBG],
           ["PA13","SYS_JTMS-SWDIO","SWD Data","Alternate","ST-Link"],
           ["PA14","SYS_JTCK-SWCLK","SWD Clock","Alternate","ST-Link"],
           ["PB3","SYS_JTDO-SWO","SWO Trace","Alternate","ST-Link"]], header=True))

    w('<h3 id="s2.3">2.3 Niveles Electricos y Tolerancias</h3>')
    w(tbl([["Senal","Nivel MCU","Nivel Target","Notas"],
           ["SPI1 (SD)","3.3V (VDD)","3.3V","Compatible directo"],
           ["SPI2 (ADXL355)","3.3V (VDD)","3.3V","Compatible directo"],
           ["USART1 (EC25)","3.3V (VDD)","1.8V (modem)","Requiere level shifter o resistencias serie"],
           ["USART2 (CLI)","3.3V (VDD)","3.3V (USB-serial)","Compatible directo"],
           ["ADXL_INT1","3.3V (tolerante 5V)","3.3V","EXTI falling edge, pull-down"],
           ["MODEM_RI","3.3V (tolerante 5V)","1.8V-2.8V (EC25)","EXTI falling edge, pull-up"]]))
    w('<div class="note"><strong>ATENCION:</strong> USART1 opera a 3.3V en el STM32 pero el EC25 espera 1.8V en sus pines UART. Verificar que el hardware incluye level shifting o resistencias serie de adaptacion.</div>')

    w('<h3 id="s2.4">2.4 Configuracion NVIC</h3>')
    w(tbl([["IRQ","Fuente","Preempt Priority","Sub Priority","Proposito"],
           ["EXTI9_5_IRQn","PC7 (ADXL_INT1)","5","0","Deteccion de movimiento (motion trigger)"],
           ["TIM1_UP_TIM10_IRQn","TIM1","15","0","Base de tiempo del RTOS (SysTick sustituto)"],
           ["PendSV_IRQn","FreeRTOS","15","0","Context switch del scheduler"],
           ["SysTick_IRQn","SysTick","15","0","Timebase HAL"]]))
    w("<hr>")

    # ── S3: Arquitectura de Software ──
    w('<h2 id="s3">3. Arquitectura de Software</h2>')
    w('<h3 id="s3.1">3.1 Tareas FreeRTOS</h3>')
    w(tbl([["Tarea","Funcion","Prioridad CMSIS","Stack (bytes)","Entrada"],
           ["sensor_task","StartSensorTask",bdg("osPriorityHigh","wip"),"4096","EVT_MOTION_DETECTED (EXTI)"],
           ["modem_task","StartModemTask",bdg("osPriorityAboveNormal","wip"),"8192","EVT_FILE_READY"],
           ["file_task","StartFileTask",bdg("osPriorityNormal","todo"),"6144","Boot scan + idle loop"],
           ["control_task","StartControlTask",bdg("osPriorityNormal","todo"),"4096","UART2 (CLI)"],
           ["defaultTask","StartDefaultTask",bdg("osPriorityNormal","todo"),"512","Idle loop osDelay(1000)"]]))
    w("<p><strong>V1.2.0:</strong> Stack sizes actualizados: sensor 4096B (antes 1024B), modem 8192B (antes 2048B), file 6144B (antes 1024B). El modem_task necesita mas stack para HTTP/TCP y parseo de respuestas.</p>")

    w('<h3 id="s3.2">3.2 Objetivos de Sincronizacion</h3>')
    w(tbl([["Objeto","Tipo","Configuracion","Uso"],
           ["upload_queue","RAM circular buffer","8 slots x 32 bytes","file_task &rarr; modem_task (TRIG filenames)"],
           ["sd_mutex","osMutex","Recursive","Proteccion acceso SD (FatFs)"],
           ["sensor_event_flags","osEventFlags","5 flags","Coordinacion entre tareas"]]))
    w("<p><strong>V1.2.0:</strong> La <code>sensor_queue</code> (50 slots x 28 bytes) fue reemplazada por la <code>upload_queue</code> (8 slots x 32 bytes en RAM). El sensor_task escribe directo a SD en vez de encolar muestras, y la cola maneja solo los nombres de archivos TRIG pendientes de upload.</p>")

    w('<h3 id="s3.3">3.3 Event Flags</h3>')
    w(tbl([["Flag","Bit","Productor","Consumidor","Semantica"],
           ["EVT_MOTION_DETECTED","(1<<0)","EXTI ISR / control_task","sensor_task","Iniciar adquisicion"],
           ["EVT_ACQSTN_DONE","(1<<1)","sensor_task","(reservado)","Adquisicion completada"],
           ["EVT_UPLOAD_DONE","(1<<2)","modem_task","control_task","Upload completado"],
           ["EVT_CFG_CHECK","(1<<3)","Timer periodico","control_task","Verificar config remota"],
           ["EVT_FILE_READY","(1<<4)","file_task / modem_task","modem_task","Archivo listo para upload"]]))

    w('<h3 id="s3.4">3.4 Flujo de Datos</h3>')
    w("<p><strong>V1.2.0 &mdash; Pipeline completo:</strong></p>")
    w("<pre><code>1. ADXL_INT1 (EXTI) &rarr; sensor_task (Motion detect)\n"
      "2. sensor_task &rarr; acquire data &rarr; write CSV to SD (takes sd_mutex briefly)\n"
      "3. sensor_task &rarr; upload_queue_push(filename) &rarr; EVT_FILE_READY\n"
      "4. modem_task wakes &rarr; upload_queue_peek &rarr; Modem_UploadFile (3 attempts)\n"
      "5. On success: queue_pop &rarr; .DONE marker &rarr; Modem_PowerOff &rarr; check next file\n"
      "6. On NRST (from modem TCP close): queue already updated, .DONE already created\n"
      "7. On reboot: file_task boot scan &rarr; backup register check &rarr; .DONE check &rarr; requeue</code></pre>")

    w('<h3 id="s3.5">3.5 Tracking de Upload &mdash; Backup Registers RTC</h3>')
    w("<p>El sistema HERMES A1 utiliza un mecanismo de <strong>doble capa</strong> para rastrear cuales archivos TRIG ya fueron subidos con exito:</p>")
    w("<h4>Capa Primaria: RTC Backup Registers (BKP0R..BKP19R)</h4>")
    w(tbl([["Propiedad","Detalle"],
           ["Registros","BKP0R hasta BKP19R (STM32F446)"],
           ["Tamaño","32 bits por registro"],
           ["Capacidad","19 registros x 32 bits = 608 indices TRIG"],
           ["Bitmap","BKP0R bits 0-31 = TRIG_001..032, BKP1R = TRIG_033..064, etc."],
           ["Supervivencia","Sobreviven NRST, IWDG reset, software reset"],
           ["Limpieza","Solo se limpian en Power-On Reset (POR)"],
           ["Escritura","Instantanea (un solo registro de 32 bits)"],
           ["Funciones","Modem_MarkUploaded(idx), Modem_IsUploaded(idx)"]]))
    w("<h4>Capa Fallback: .DONE Marker File en SD</h4>")
    w(tbl([["Propiedad","Detalle"],
           ["Formato","TRIG_NNN.DONE (archivo vacio en FatFs)"],
           ["Creacion","f_open + f_close con HAL_Delay(50) post-write"],
           ["Verificacion","Boot scan abre archivo para lectura"],
           ["Limitacion","Puede no persistir si NRST golpea durante escritura NAND"]]))
    w("<p><strong>Flujo en exito:</strong> <code>Modem_CreateDoneMarker()</code> &rarr; <code>Modem_MarkUploaded()</code> PRIMERO (instantaneo) &rarr; archivo .DONE (best-effort con delay de 50ms).</p>")

    w('<h3 id="s3.6">3.6 Boot Scan &mdash; Recuperacion post-reset</h3>')
    w("<p>Al reiniciar (post-NRST), <code>file_task</code> ejecuta un escaneo del directorio SD para reconstruir la cola de upload:</p>")
    w("<pre><code>1. Modem_BackupInit() &mdash; habilita RTC APB1 clock + DBP\n"
      "2. f_mount + f_opendir en 0:/\n"
      "3. Para cada TRIG_NNN.CSV encontrado:\n"
      "   a. PRIMARY CHECK: Modem_IsUploaded(idx) &rarr; saltar si backup register set\n"
      "   b. FALLBACK CHECK: .DONE file exists &rarr; saltar\n"
      "   c. Si ninguno: agregar a pending_indices[]\n"
      "4. Bubble sort pending_indices ascending\n"
      "5. upload_queue_push para cada pendiente\n"
      "6. Si hay pendientes: osEventFlagsSet(EVT_FILE_READY)</code></pre>")
    w("<p>Este mecanismo garantiza que ningun TRIG se pierda, incluso despues de un NRST reset.</p>")

    w('<h3 id="s3.7">3.7 Root Cause del NRST &mdash; Modem Power Transient</h3>')
    w(tbl([["Propiedad","Detalle"],
           ["Reset reason","0x00000012 = PINRSTF + IWDGRSTF"],
           ["Causa raiz","Modem EC25 cierra TCP post-upload &rarr; actividad RF &rarr; pico de corriente en rail 3.3V compartido"],
           ["Acoplamiento","Se acopla al pin NRST (~40k ohm pull-up interno)"],
           ["Trigger especifico","AT+QPOWD &rarr; PDP deactivation (+QIURC: pdpdeact) &rarr; spike"],
           ["Evidencia","CONS_OK('Upload successful') NUNCA aparece en log &rarr; reset ocurre en ~5us"],
           ["Mitigacion","Modem_PowerOff() reubicada a modem_task DESPUES de queue pop + .DONE"],
           ["Code comment","quectel_drive.c:431 &mdash; 'Software shutdown only &mdash; NO HAT_PWR_OFF toggle'"]]))
    w("<hr>")

    # ── S4: Gestion de Memoria ──
    w('<h2 id="s4">4. Gestion de Memoria</h2>')
    w("<h3>4.1 Mapa de Memoria del STM32F446RE</h3>")
    w(tbl([["Region","Direccion","Tamano","Uso"],
           ["Flash","0x0800 0000","512 KB","Codigo + constantes"],
           ["SRAM","0x2000 0000","128 KB","Variables globales + heap + stacks"],
           ["CCM RAM","0x1000 0000","64 KB","No utilizado (podria optimizarse)"]]))

    w("<h3>4.2 Distribucion del Heap FreeRTOS (Heap_4) &mdash; V1.2.0</h3>")
    w("<p><code>configTOTAL_HEAP_SIZE = 40960</code> (40 KB) de los 128 KB de SRAM disponibles.</p>")
    w(tbl([["Componente","Tamano","Calculo"],
           ["sensor_task stack","4096 B","1024 words x 4 bytes"],
           ["modem_task stack","8192 B","2048 words x 4 bytes"],
           ["file_task stack","6144 B","1536 words x 4 bytes"],
           ["control_task stack","4096 B","1024 words x 4 bytes"],
           ["default Task stack","512 B","128 words x 4 bytes"],
           ["upload_queue","256 B","8 slots x 32 bytes"],
           ["Queue overhead","~200 B","Internal FreeRTOS struct"],
           ["Mutex + EventFlags","~200 B","Internal structs"],
           ["<strong>Total asignado</strong>","<strong>~24,500 B</strong>","<strong>~60% del heap de 40 KB</strong>"],
           ["<strong>Heap libre</strong>","<strong>~15,500 B</strong>","<strong>Disponible para malloc dinamico</strong>"]]))

    w("<h3>4.3 Justificacion de Heap_4</h3>")
    w("<p>FreeRTOS ofrece 5 esquemas de heap. Se selecciono Heap_4 por: coalescencia (fusiona bloques libres adyacentes), alloc + free (soporta liberacion segura), first-fit (algoritmo determinista).</p>")
    w("<p>Riesgo residual: en operacion continua (24/7), la fragmentacion puede acumularse. Mitigacion: usar <code>xPortGetFreeHeapSize()</code> periodicamente para monitorear.</p>")
    w("<hr>")

    # ── S5: DMA y FPU ──
    w('<h2 id="s5">5. Estado DMA y FPU</h2>')
    w("<h3>5.1 FPU Habilitada " + bdg("COMPLETADO","ok") + "</h3>")
    w(tbl([["Parametro","Valor Actual","Recomendado"],
           ["configENABLE_FPU","1 (habilitada)","1 (habilitada)"]]))
    w("<p>Impacto: sensor_task ejecuta operaciones float por hardware (~1-2 ciclos/operacion). A 10 Hz con 3-5 operaciones float por muestra, el overhead es de ~30-50 ciclos/s.</p>")

    w("<h3>5.2 DMA SPI2 " + bdg("COMPLETADO","ok") + "</h3>")
    w(tbl([["Interfaz","Metodo Actual","DMA Configurado","Impacto"],
           ["SPI1 (SD)","Polling (USE_DMA = 0)","No","CPU bloqueada durante transferencias SPI"],
           ["SPI2 (ADXL355)","DMA (HAL_SPI_TransmitReceive_DMA)","Si (DMA1 Stream 3/4, Ch 0)","Transferencia continua sin bloquear CPU"],
           ["USART1 (EC25)","Polling","No","CPU bloqueada durante comandos AT"],
           ["USART2 (CLI)","Polling (printf)","No","Menor impacto (solo debug)"]]))
    w("<p>DMA1 Stream 3 (RX) + Stream 4 (TX), Channel 0. Sincronizacion via semaforo binario task&rarr;ISR.</p>")

    w("<h3>5.3 Wake-on-Motion (WoM) " + bdg("COMPLETADO","ok") + "</h3>")
    w(tbl([["Parametro","Valor","Estado"],
           ["INT1 (PC7)","EXTI Falling Edge, Pull-Down",bdg("OK","ok")],
           ["Activity Detection","X+Y axes, HPF OFF",bdg("OK","ok")],
           ["Threshold","Configurable via ADXL355_Config_WakeOnMotion()",bdg("OK","ok")],
           ["CLI Test","Comando wom",bdg("OK","ok")]]))
    w("<hr>")

    # ── S6: Correcciones ──
    w('<h2 id="s6">6. Correcciones de Codigo Aplicadas</h2>')
    w(tbl([["#","Problema","Archivo","Solucion","Estado"],
           ["1","strcasecmp() no disponible en ARM GCC","main.c","Implementacion local strcasecmp_custom()",bdg("v1.0","ok")],
           ["2","Includes con mayusculas incorrectas","6 archivos","Corregido a minusculas consistentes",bdg("v1.0","ok")],
           ["3","Escrituras a registros EXTI sin deshabilitar IRQ","control_task.c","__disable_irq() / __enable_irq() alrededor de EXTI",bdg("v1.0","ok")],
           ["4","Falso positivo EVT_ACQSTN_DONE en sdtest","sensor_task.c","Guard !sdbg_abort_acq antes de osEventFlagsSet",bdg("v1.0","ok")],
           ["5","Variable muerta FRESULT fres","main.c","Eliminada, llamada directa a sd_mount()",bdg("v1.0","ok")],
           ["6","Credenciales vacias sin validacion","credentials.h","Funcion Credentials_Validate() inline",bdg("v1.0","ok")],
           ["7","FIFO ADXL355 leido con CS low/high por muestra","adxl355.c","Burst read: CS bajo una vez, leer todas las muestras",bdg("v1.0","ok")],
           ["8","Llamadas al modem antes de osKernelStart()","main.c","Comentadas; mover a modem_task",bdg("v1.0","ok")],
           ["9",".DONE marker no persiste post-NRST","quectel_drive.c","RTC backup registers BKP0R..BKP19R",bdg("v1.2","ok")],
           ["10","NRST por Modem_PowerOff en ruta erronea","modem_task.c","PowerOff movido DESPUES de queue pop + .DONE",bdg("v1.2","ok")],
           ["11","upload_queue_pop(NULL,0) UB","file_task.c","NULL guard: if (buf != NULL && size > 0)",bdg("v1.2","ok")],
           ["12","Boot scan invisible (CONS_DBG disabled)","file_task.c","CONS_DBG &rarr; CONS_INFO for skip reasons",bdg("v1.2","ok")],
           ["13","SD .DONE marker NAND timing","quectel_drive.c","HAL_Delay(50) after f_close",bdg("v1.2","ok")]]))
    w("<hr>")

    # ── S7: Comparativo ──
    w('<h2 id="s7">7. Analisis Comparativo de Versiones</h2>')
    w(tbl([["Proyecto","Ubicacion","Arquitectura","Estado"],
           ["cmsis-test (local)","Directorio actual","FreeRTOS, 5 tareas, backup registers",bdg("v1.2.0","ok")],
           ["cmsis-test (GitHub)","github.com/tonetooo/cmsis-test","FreeRTOS, 4 tareas",bdg("Semi-estable","wip")],
           ["cmsis (GitHub)","github.com/tonetooo/cmsis","FreeRTOS, 4 tareas, sin test suite",bdg("Version limpia","todo")],
           ["AWTAS","github.com/tonetooo/AWTAS","Baremetal, superloop, state machines",bdg("Predecesor","todo")]]))
    w(tbl([["Feature","Local","cmsis-test (GH)","cmsis (GH)","AWTAS"],
           ["Archivos .c/.h","16/19","14/17","13/16","10/10"],
           ["Tests unitarios","47","No","No","No"],
           ["RTOS","FreeRTOS 40KB","FreeRTOS","FreeRTOS","Baremetal"],
           ["quectel_drive.c","~1155 lineas","~800","~800","744 lineas"],
           ["Upload queue","RAM circular","QUEUE.TXT","QUEUE.TXT","SD-based"],
           ["Backup registers","BKP0R..BKP19R","No","No","No"]]))
    w("<hr>")

    # ── S8: UniKnect ──
    w('<h2 id="s8">8. Evaluacion de Quectel UniKnect SDK</h2>')
    w(tbl([["Componente","UniKnect SDK","Nuestro Proyecto","Veredicto"],
           ["MCU","STM32F413RGT6/VGT6","STM32F446RET6","No compatible"],
           ["Modem","EC200U, BG96","Quectel EC25","AT similar, no verificado"],
           ["Build","CMake","CubeMX Makefile","Requiere migracion"],
           ["RTOS","FreeRTOS bundled","FreeRTOS + CMSIS-RTOS v2","Posible conflicto"]]))
    w('<div class="note"><strong>ATENCION:</strong> No recomendado para la version actual. El costo de portar supera los beneficios.</div>')
    w("<hr>")

    # ── S9: Actividades Pendientes ──
    w('<h2 id="s9">9. Actividades Pendientes - Roadmap</h2>')
    w("<h3>9.1 Prioridad 1 - Critico</h3>")
    w(tbl([["#","Actividad","Estado"],
           ["9.1.1","DMA para SPI2",bdg("COMPLETADO","ok")],
           ["9.1.2","Habilitar FPU",bdg("COMPLETADO","ok")],
           ["9.1.3","Adquisicion en tiempo continuo",bdg("EN PROGRESO (85%)","wip")],
           ["9.1.4","Calibracion del acelerometro",bdg("INICIADO (10%)","wip")],
           ["9.1.5","Watchdog IWDG",bdg("COMPLETADO","ok")],
           ["9.1.6","Buffer local ante perdida de conectividad",bdg("COMPLETADO","ok")],
           ["9.1.7","Sincronizacion horaria NTP",bdg("EN PROGRESO (40%)","wip")],
           ["9.1.8","Optimizacion de consumo energetico",bdg("INICIADO (30%)","wip")]]))

    w("<h3>9.2 Prioridad 2 - Alto</h3>")
    w(tbl([["#","Actividad","Estado"],
           ["9.2.1","Metricas de calidad de senal",bdg("PENDIENTE","todo")],
           ["9.2.2","Telemetria energetica integrada",bdg("PENDIENTE","todo")],
           ["9.2.3","Gestion remota de configuracion",bdg("PENDIENTE","todo")],
           ["9.2.4","Sistema de alarmas operativas",bdg("PENDIENTE","todo")],
           ["9.2.5","Monitoreo del estado interno",bdg("PENDIENTE","todo")],
           ["9.2.6","Compensacion termica",bdg("PENDIENTE","todo")],
           ["9.2.7","Caracterizacion del enlace celular",bdg("PENDIENTE","todo")]]))

    w("<h3>9.3 Prioridad 3 - Evolucion</h3>")
    w(tbl([["#","Actividad","Estado"],
           ["9.3.1","Arquitectura multisensor I2C",bdg("PENDIENTE","todo")],
           ["9.3.2","Reinicio remoto",bdg("PENDIENTE","todo")],
           ["9.3.3","Migracion del protocolo de transmision",bdg("PENDIENTE","todo")]]))
    w("<hr>")

    # ── S10: API ──
    w('<h2 id="s10">10. Referencia API</h2>')
    w(tbl([["Modulo","Archivos","Responsabilidad"],
           ["Driver ADXL355","adxl355.c/h","SPI2, lectura FIFO, conversion raw g, LevelToZero, DMA, WoM"],
           ["Driver Quectel EC25","quectel_drive.c/h","PowerOn, BringUpNetwork, SendAT, UploadFile, DownloadConfig, BackupInit, MarkUploaded, IsUploaded, CreateDoneMarker, PowerOff, Sleep"],
           ["Driver SD","sd_spi.c/h","SPI1, bloques 512B, FatFs diskio"],
           ["Tareas RTOS","tasks/*.c","sensor, modem, file, control pipeline de datos"],
           ["Algoritmos","algo/*.c/h","Logica pura: motion detection, settling, CLI parser, CSV format"],
           ["CLI","control_task.c","Parser de comandos, ejecucion, output por UART2"],
           ["Tests","test_suite.c/h","Diagnostico: SD test, modem test, sensor test"]]))

    w("<h3>Funciones Clave Nuevas en V1.2.0</h3>")
    w(tbl([["Funcion","Archivo","Descripcion"],
           ["Modem_BackupInit()","quectel_drive.c","Habilita RTC APB1 clock + HAL_PWR_EnableBkUpAccess"],
           ["Modem_MarkUploaded(idx)","quectel_drive.c","Set bit en BKP0R..BKP19R para TRIG index dado"],
           ["Modem_IsUploaded(idx)","quectel_drive.c","Lee bit de BKP0R..BKP19R para TRIG index dado"],
           ["Modem_CreateDoneMarker(fn)","quectel_drive.c","MarkUploaded() + .DONE file + HAL_Delay(50)"],
           ["Modem_PowerOff()","modem_task.c","AT+QPOWD=1 + delay, modem_powered=0"],
           ["upload_queue_push(fn)","file_task.c","Enqueue TRIG filename a RAM circular buffer"],
           ["upload_queue_peek(buf,sz)","file_task.c","Read first filename sin remover"],
           ["upload_queue_pop(buf,sz)","file_task.c","Read + remove first filename, NULL guard"]]))
    w("<hr>")

    # ── S11: Compilacion ──
    w('<h2 id="s11">11. Compilacion y Flash</h2>')
    w("<h3>Compilar</h3>")
    w("<pre><code>make -j4</code></pre>")
    w("<h3>Flashear</h3>")
    w('<pre><code>openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program build/HERMES-A1-CMSIS.elf verify reset exit"</code></pre>')
    w("<h3>Conexion CLI</h3>")
    w("<pre><code>picocom -b 115200 /dev/tty.usbmodem*\nscreen /dev/tty.usbmodem* 115200</code></pre>")
    w("<h3>Tests Unitarios</h3>")
    w("<pre><code>cd test && ceedling test:all  # 47 tests</code></pre>")
    w("<hr>")

    # ── S12: CLI ──
    w('<h2 id="s12">12. Comandos CLI</h2>')
    w(tbl([["Comando","Descripcion"],
           ["help","Lista de comandos disponibles"],
           ["status","Estado del sistema y umbral actual"],
           ["accel","Lectura actual del acelerometro"],
           ["trigger","Umbral de disparo actual en G"],
           ["trigger <valor>","Establecer umbral (0-10 G)"],
           ["log","Lista archivos CSV en SD"],
           ["test","Simula evento de movimiento (prueba de pipeline)"],
           ["sdtest","Adquisicion forzada 10s + tabla ASCII en SD"],
           ["modem_on","Enciende modem + prueba sincronizacion AT"],
           ["sensstat","Lee registro STATUS del ADXL355 (ACT/DRDY bits)"],
           ["readreg <hex>","Lee cualquier registro ADXL355"],
           ["at <cmd>","Envia comando AT crudo al modem"],
           ["debug","Alterna salida diagnostica (on/off)"],
           ["fpu","Test operaciones FPU (hardware float)"],
           ["dma","Test transferencia SPI2 DMA"],
           ["wom","Test interrupcion Wake-on-Motion"]]))
    w("<hr>")

    # ── S13: Estructura ──
    w('<h2 id="s13">13. Estructura del Proyecto</h2>')
    w(tbl([["Ruta","Contenido"],
           ["Core/Inc/","Headers: main.h, adxl355.h, quectel_drive.h, sd_spi.h, tasks.h, credentials.h, wdt.h, console.h"],
           ["Core/Inc/algo/","Logica pura (test host): sensor_algo.h, cli_algo.h, csv_algo.h"],
           ["Core/Src/","Implementacion: main.c, adxl355.c, quectel_drive.c, sd_spi.c, freertos.c, gpio.c, spi.c, usart.c"],
           ["Core/Src/algo/","Logica pura: sensor_algo.c, cli_algo.c, csv_algo.c"],
           ["Core/Src/tasks/","Tareas RTOS: sensor_task.c, file_task.c, modem_task.c, control_task.c"],
           ["Core/Startup/","Startup assembly + linker script"],
           ["Drivers/","STM32 HAL + CMSIS"],
           ["FATFS/","FatFs middleware"],
           ["Middlewares/","FreeRTOS + FatFs"],
           ["test/","Ceedling + Unity: 47 tests"],
           ["docs/","Documentacion: HTML, PDF, planning, GANTT"],
           ["docs/planning/creador gantt/","GANTT, planificacion, documentacion HTML"],
           ["Doxyfile","Configuracion Doxygen"],
           ["Makefile","Build system (arm-none-eabi-gcc)"],
           ["HERMES-A1-CMSIS.ioc","STM32CubeMX project config"],
           ["STM32F446RETX_FLASH.ld","Linker script"]]))
    w("<hr>")

    # ── S14: Rutas de Upload ──
    w('<h2 id="s14">14. Rutas de Upload</h2>')
    w("<p>HERMES A1 soporta dos rutas de upload para subir archivos CSV al servidor remoto:</p>")
    w(tbl([["Ruta","Destino","Protocolo","Implementacion"],
           ["Ruta 1","Google Drive API","HTTP multipart/form-data","quectel_drive.c (lineas 630-711)"],
           ["Ruta 2","Backend Flask (LIND)","HTTP POST con headers manuales","quectel_drive.c (lineas 714-1010)"]]))
    w("<h3>Ruta 2 &mdash; Backend Flask (Principal)</h3>")
    w("<p>El upload se realiza via HTTP POST a un servidor Flask accesible a traves de un Cloudflare Tunnel. El backend recibe el CSV, lo convierte a formato interno, y lo sube a Google Drive.</p>")
    w("<pre><code>POST /upload HTTP/1.1\n"
      "Content-Type: application/octet-stream\n"
      "X-Upload-Id: LIND2026ANTONIO\n"
      "Content-Length: NNNN\n\n"
      "[CSV data]\n\n"
      "&rarr; 201 Created (exitoso)\n"
      "&rarr; 400 Bad Request (error de formato)</code></pre>")
    w("<h3>Ruta 1 &mdash; Google Drive (Fallback)</h3>")
    w("<p>Upload directo a Google Drive via multipart/form-data con token GDRIVE_TOKEN renovado.</p>")
    w("<hr>")

    # ── S15: Conocidos ──
    w('<h2 id="s15">15. Conocidos y Limitaciones</h2>')
    w(tbl([["#","Issue","Impacto","Mitigacion"],
           ["1","NRST post-upload","Reset despues de cada upload exitoso","Modem_PowerOff after queue pop + backup registers"],
           ["2","SD mount falla primer intento","FRESULT=1 en primer mount","Reintento automatico en file_task"],
           ["3","NTP sync falla","Ambos servidores no responden","No critico &mdash; timestamps usa HAL_GetTick()"],
           ["4",".DONE marker FR=6","FatFs FR_INVALID_NAME","Backup registers compensan"],
           ["5","sensor_queue eliminada","Cambo de arq. v1.1.0 &rarr; v1.2.0","upload_queue RAM circular reemplaza sensor_queue"],
           ["6","CCM RAM no utilizada","64 KB disponibles","Podria optimizarse para buffer o DMA"]]))
    w("<hr>")

    # ── Footer ──
    w("<footer>")
    w("<p><strong>HERMES A1 &mdash; Documentacion Tecnica v1.2.0</strong></p>")
    w("<p>Antonio Avendano Sanhueza &bull; LIND Engineering &bull; Junio 2026</p>")
    w("<p>Generado automaticamente desde documentacion.md + codigo fuente</p>")
    w("</footer>")

    w("</div>")  # .ctn
    w("</body>")
    w("</html>")

def main():
    build()
    outpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), "documentacion.html")
    with open(outpath, "w", encoding="utf-8") as f:
        f.write("\n".join(BODY))
    sz = os.path.getsize(outpath)
    print(f"OK: {outpath} ({sz} bytes, {len(BODY)} lines)")

if __name__ == "__main__":
    main()
