# HERMES A1[cite: 2]
**Actividades Pendientes y Líneas de Desarrollo Futuro**[cite: 2]
**Sistema de Monitoreo Estructural**[cite: 2]
Documento técnico de desarrollo del sistema Hermes A1[cite: 2]

* **Área:** Instrumentación electrónica y monitoreo estructural[cite: 2]
* **Ámbito tecnológico:** Structural Health Monitoring (SHM)[cite: 2]
* **Autor:** Equipo Electrónico[cite: 2]
* **Fecha:** Marzo 2026[cite: 2]
* **Ubicación:** Concepción, Chile[cite: 2]
* **Versión del documento:** Versión 1.0[cite: 2]

---

## Descripción del documento[cite: 2]

Este documento presenta las actividades pendientes y las líneas de desarrollo tecnológico identificadas a partir de la evaluación del prototipo Hermes A1 desarrollado durante la práctica profesional[cite: 2]. Se detallan mejoras en los sistemas de adquisición de datos, gestión energética, integración electrónica y validación operativa, con el objetivo de evolucionar el prototipo hacia una plataforma robusta para aplicaciones de monitoreo estructural en terreno[cite: 2].

---

## Índice[cite: 2]

1. Pendientes del sistema de adquisición de datos (proyectado a 6 meses)[cite: 2]
1.1 Implementación de adquisición en tiempo continuo[cite: 2]
1.2 Integración de métricas de calidad de señal[cite: 2]
1.3 Optimización del sistema de adquisición mediante DMA[cite: 2]
1.4 Calibración y caracterización del acelerómetro[cite: 2]
1.5 Integración de telemetría energética en el sistema de adquisición[cite: 2]
1.6 Gestión remota del archivo de configuración[cite: 2]
1.7 Sistema de alarmas operativas[cite: 2]
1.8 Diseño de PCB dedicado para adquisición[cite: 2]
1.9 Expansión hacia arquitectura multisensor I2C[cite: 2]
1.10 Monitoreo del estado interno del sistema[cite: 2]
1.11 Reinicio remoto del sistema[cite: 2]
1.12 Compensación térmica de mediciones[cite: 2]
1.13 Implementación de watchdog[cite: 2]
1.14 Buffer local de datos ante pérdida de conectividad[cite: 2]
1.15 Sincronización horaria del sistema[cite: 2]
1.16 Caracterización del sistema de comunicaciones celulares[cite: 2]
1.17 Optimización del consumo energético del sistema de adquisición[cite: 2]
1.18 Migración del protocolo de transmisión de datos[cite: 2]
2. Pendientes del sistema energético (proyectado a 2 meses)[cite: 2]
2.1 Validación del banco de baterías[cite: 2]
2.2 Sistema modular de baterías[cite: 2]
2.3 PCB compacto para el sistema de alimentación[cite: 2]
2.4 Conectores rápidos para panel solar[cite: 2]
2.5 Interfaz de configuración local mediante USB-C[cite: 2]
2.6 Monitoreo del sistema fotovoltaico[cite: 2]
2.7 Monitoreo del estado de baterías[cite: 2]
2.8 Detección de fallas del sistema fotovoltaico[cite: 2]
2.9 Gestión energética adaptativa del sistema[cite: 2]
2.10 Arquitectura modular de salidas de energía[cite: 2]
2.11 Integración de protecciones eléctricas del sistema[cite: 2]
2.12 Optimización del sistema de bypass energético[cite: 2]
2.13 Validación energética en condiciones reales de operación[cite: 2]
3. Integración, validación y pruebas del sistema[cite: 2]
3.1 Integración electrónica en encapsulado robusto[cite: 2]
3.2 Validación con sistema de referencia[cite: 2]
3.3 Generación de línea base de comportamiento estructural[cite: 2]
3.4 Pruebas operacionales en terreno[cite: 2]
4. Evolución tecnológica del sistema[cite: 2]

---

## Actividades Pendientes y Líneas de Desarrollo Futuro Hermes A1[cite: 2]

A partir de la evaluación del prototipo desarrollado durante la práctica, se identifican diversas actividades orientadas a consolidar el sistema, mejorar su robustez operativa y avanzar hacia un diseño definitivo apto para implementación en terreno[cite: 2].

### 1. Pendientes del sistema de adquisición de datos[cite: 2]

**1. Implementación de adquisición en tiempo continuo**[cite: 2]
Incorporar un modo de operación que permita la adquisición permanente de datos del acelerómetro, registrando señales de vibración de forma continua sin depender exclusivamente de activaciones por evento[cite: 2].

**2. Integración de métricas de calidad de señal**[cite: 2]
Incorporar en los registros indicadores de calidad de la señal tales como:[cite: 2]
* relación señal-ruido (SNR)[cite: 2]
* nivel de ruido de fondo[cite: 2]
* indicadores de saturación o clipping[cite: 2]

Adicionalmente, integrar métricas de calidad del enlace de comunicaciones celulares (4G/5G):[cite: 2]
* intensidad de señal recibida (RSSI)[cite: 2]
* calidad de señal reportada por el módem[cite: 2]
* tipo de red disponible[cite: 2]

Estos parámetros permitirán evaluar la confiabilidad de las mediciones, analizar la estabilidad de la transmisión de datos en terreno y optimizar la conectividad del dispositivo[cite: 2].

**3. Optimización del sistema de adquisición mediante DMA**[cite: 2]
Implementar el uso de Direct Memory Access (DMA) para la transferencia de datos entre el acelerómetro y la memoria del microcontrolador durante los procesos de adquisición[cite: 2]. El uso de DMA permitirá:[cite: 2]
* reducir la carga de procesamiento del microcontrolador[cite: 2]
* aumentar la eficiencia del sistema durante adquisiciones continuas[cite: 2]
* mejorar la estabilidad del sistema en modos de adquisición de alta frecuencia[cite: 2]

Esta optimización resulta especialmente relevante en escenarios de adquisición multisensor o configuraciones que requieran altas tasas de muestreo[cite: 2].

**4. Calibración y caracterización del acelerómetro**[cite: 2]
Realizar la calibración y caracterización experimental del acelerómetro utilizado en el sistema de adquisición, con el objetivo de determinar parámetros como:[cite: 2]
* sensibilidad efectiva del sensor[cite: 2]
* offset o bias de medición[cite: 2]
* linealidad de respuesta[cite: 2]
* ruido propio del sensor[cite: 2]

La calibración deberá realizarse mediante comparación con sensores de referencia o utilizando sistemas de excitación controlada como mesas vibratorias o calibradores de acelerómetros[cite: 2].

**5. Integración de telemetría energética en el sistema de adquisición**[cite: 2]
Incorporar dentro del flujo de adquisición de datos el registro de variables energéticas del sistema tales como:[cite: 2]
* voltaje del sistema o batería[cite: 2]
* corriente de consumo del sistema[cite: 2]
* potencia estimada del sistema de adquisición[cite: 2]

El registro de estas variables permitirá correlacionar el desempeño energético con el funcionamiento del sistema de adquisición y facilitar el diagnóstico del comportamiento operativo del nodo de monitoreo[cite: 2].

**6. Gestión remota del archivo de configuración**[cite: 2]
Implementar un sistema que permita verificar, descargar y actualizar automáticamente el archivo de configuración del sistema durante la conexión con el servidor[cite: 2]. Adicionalmente, se deberá permitir la recarga dinámica del archivo de configuración sin necesidad de reiniciar el microcontrolador STM32, permitiendo modificar parámetros operativos tales como:[cite: 2]
* frecuencia de adquisición[cite: 2]
* intervalos de transmisión[cite: 2]
* umbrales de alarma[cite: 2]

Esta funcionalidad permitirá mantener actualizados los parámetros operativos del sistema sin intervención física sobre los dispositivos instalados en terreno[cite: 2].

**7. Sistema de alarmas operativas**[cite: 2]
Integrar mecanismos de alerta automática hacia la plataforma de monitoreo o correo electrónico ante la detección de eventos tales como:[cite: 2]
* niveles críticos de batería[cite: 2]
* eventos de vibración fuera de rango[cite: 2]

Estos eventos podrían corresponder a actividad sísmica, manipulación del equipo o vibraciones anómalas en la estructura monitoreada[cite: 2].

**8. Diseño de PCB dedicado para adquisición**[cite: 2]
Desarrollar una placa electrónica dedicada basada en el microcontrolador STM32 que integre las funciones de adquisición, procesamiento y transmisión de datos[cite: 2]. La implementación de un diseño de PCB dedicado permitirá:[cite: 2]
* reducir dimensiones del sistema[cite: 2]
* disminuir consumo energético[cite: 2]
* mejorar la robustez eléctrica[cite: 2]
* simplificar el cableado interno del sistema[cite: 2]

**9. Expansión hacia arquitectura multisensor I²C**[cite: 2]
Incorporar soporte para arquitectura multisensor basada en bus I²C además del actual bus SPI[cite: 2]. Para ello se propone:[cite: 2]
* habilitar el acelerómetro para operación en modo I²C[cite: 2]
* integrar un multiplexor I²C TCA9548A o equivalente[cite: 2]
* utilizar extensores diferenciales de bus tipo SparkFun QwiicBus[cite: 2]

Esta arquitectura permitirá conectar múltiples sensores distribuidos a distancias de hasta aproximadamente 30 metros[cite: 2].

**10. Monitoreo del estado interno del sistema**[cite: 2]
Incorporar registro de variables operativas del dispositivo tales como:[cite: 2]
* uso de memoria[cite: 2]
* tiempo de operación (uptime)[cite: 2]
* errores de comunicación[cite: 2]
* número de reinicios del sistema[cite: 2]

Estos indicadores permitirán evaluar la estabilidad operativa del sistema y facilitar el diagnóstico remoto[cite: 2].

**11. Reinicio remoto del sistema**[cite: 2]
Implementar un mecanismo que permita reiniciar el sistema de forma remota desde la plataforma de monitoreo[cite: 2].

**12. Compensación térmica de mediciones**[cite: 2]
Integrar mecanismos de compensación térmica que permitan corregir posibles variaciones en la respuesta del acelerómetro debido a cambios de temperatura ambiental[cite: 2].

**13. Implementación de watchdog**[cite: 2]
Incorporar un sistema de supervisión mediante watchdog de hardware y/o software que permita detectar bloqueos del firmware y ejecutar reinicios automáticos controlados[cite: 2].

**14. Buffer local de datos ante pérdida de conectividad**[cite: 2]
Implementar almacenamiento temporal de datos en memoria local (flash o tarjeta SD) cuando se interrumpa la conectividad con el servidor[cite: 2].

**15. Sincronización horaria del sistema**[cite: 2]
Incorporar mecanismos de sincronización temporal robusta mediante:[cite: 2]
* servidores NTP[cite: 2]
* sincronización por red celular[cite: 2]
* receptores GNSS en aplicaciones de mayor precisión temporal[cite: 2]

**16. Caracterización del sistema de comunicaciones celulares**[cite: 2]
Realizar la caracterización del desempeño del módem celular utilizado en el sistema, evaluando parámetros de calidad de señal y comportamiento del enlace en distintas condiciones de operación[cite: 2]. A partir de estos resultados se propone diseñar una red resistiva de adaptación de impedancia para la antena, con el objetivo de optimizar la transferencia de potencia de radiofrecuencia y mejorar la calidad del enlace de comunicaciones[cite: 2].

**17. Optimización del consumo energético del sistema de adquisición**[cite: 2]
Implementar medidas de reducción de consumo energético a nivel de hardware y firmware, tales como:[cite: 2]
* desactivación de LEDs indicadores durante operación normal[cite: 2]
* uso de modos de bajo consumo del microcontrolador[cite: 2]
* optimización de ciclos de adquisición y transmisión[cite: 2]

**18. Migración del protocolo de transmisión de datos**[cite: 2]
Evaluar la migración del mecanismo actual de transmisión de datos basado en HTTP hacia protocolos más adecuados para operación continua tales como:[cite: 2]
* FTP[cite: 2]
* FTPS[cite: 2]

Esto permitirá implementar un sistema de transferencia de archivos robusto orientado a operación 24/7[cite: 2].

---

### 2. Pendientes del sistema energético[cite: 2]

**1. Validación del banco de baterías**[cite: 2]
Realizar ensayos experimentales para validar la capacidad efectiva del banco de baterías y confirmar la autonomía estimada del sistema bajo condiciones reales de operación[cite: 2].

**2. Sistema modular de baterías**[cite: 2]
Diseñar un soporte modular que permita integrar al menos cuatro baterías configurables en serie o paralelo, facilitando su reemplazo, mantenimiento y escalabilidad del sistema energético[cite: 2].

**3. PCB compacto para el sistema de alimentación**[cite: 2]
Desarrollar una placa electrónica dedicada que integre las funciones de regulación, protección, monitoreo energético y gestión de distribución de energía del sistema[cite: 2]. Esta placa permitirá mejorar la robustez del sistema, reducir el cableado y facilitar la replicabilidad del diseño en futuras versiones del dispositivo[cite: 2].

**4. Conectores rápidos para panel solar**[cite: 2]
Integrar conectores de conexión rápida en la interfaz entre el panel solar y el sistema energético, permitiendo una instalación y mantenimiento sencillo del módulo fotovoltaico en terreno[cite: 2]. El uso de conectores estandarizados facilitará la instalación, reducirá tiempos de intervención y permitirá la desconexión rápida del panel durante labores de mantenimiento o reemplazo del sistema[cite: 2].

**5. Interfaz de configuración local mediante USB-C**[cite: 2]
Implementar una interfaz de configuración local basada en puerto USB-C que permita acceder al sistema energético desde un computador mediante un puente USB-I2C[cite: 2]. Esta interfaz permitirá realizar tareas de configuración, diagnóstico y verificación del sistema durante procesos de instalación, mantenimiento o pruebas de laboratorio[cite: 2].

**6. Monitoreo del sistema fotovoltaico**[cite: 2]
Incorporar sensores que permitan medir y registrar parámetros eléctricos del panel solar tales como:[cite: 2]
* voltaje del panel solar[cite: 2]
* corriente proveniente del panel[cite: 2]
* potencia generada[cite: 2]

El registro de estas variables permitirá evaluar el desempeño del sistema fotovoltaico y correlacionar la generación de energía con las condiciones de operación del sistema[cite: 2].

**7. Monitoreo del estado de baterías**[cite: 2]
Incorporar medición y registro de variables asociadas al estado de las baterías tales como:[cite: 2]
* voltaje de batería[cite: 2]
* corriente de carga y descarga[cite: 2]
* estado de carga estimado (SOC)[cite: 2]
* temperatura de batería[cite: 2]

Estas métricas permitirán evaluar la salud del sistema energético, detectar degradación de baterías y optimizar las estrategias de operación del dispositivo[cite: 2].

**8. Detección de fallas del sistema fotovoltaico**[cite: 2]
Implementar mecanismos de detección automática de fallas del sistema fotovoltaico mediante el análisis de los parámetros eléctricos del panel solar[cite: 2]. La detección temprana de anomalías como caídas abruptas de voltaje o corriente permitirá identificar situaciones tales como desconexión del panel, fallas de cableado, sombreado excesivo o degradación del módulo fotovoltaico[cite: 2].

**9. Gestión energética adaptativa del sistema**[cite: 2]
Implementar estrategias de operación que permitan ajustar dinámicamente el consumo energético del sistema en función de la energía disponible, tales como:[cite: 2]
* reducción de frecuencia de adquisición de datos[cite: 2]
* disminución de la frecuencia de transmisión[cite: 2]
* activación de modos de bajo consumo[cite: 2]

Este enfoque permitirá mantener la operación continua del sistema incluso durante periodos prolongados de baja radiación solar[cite: 2].

**10. Arquitectura modular de salidas de energía**[cite: 2]
Diseñar un sistema de distribución energética que permita disponer de salidas de alimentación modulares (por ejemplo 12 V) destinadas a alimentar sensores adicionales, módulos de comunicación u otros periféricos del sistema[cite: 2].

**11. Integración de protecciones eléctricas del sistema**[cite: 2]
Incorporar circuitos de protección frente a condiciones eléctricas adversas tales como:[cite: 2]
* sobrecarga de batería[cite: 2]
* sobre descarga profunda[cite: 2]
* inversión de polaridad[cite: 2]
* sobretensión proveniente del panel solar[cite: 2]

Estas protecciones permitirán resguardar la integridad del sistema energético y prolongar la vida útil de los componentes[cite: 2].

**12. Optimización del sistema de bypass energético**[cite: 2]
Mejorar el mecanismo de bypass entre el panel solar, el sistema de almacenamiento y las cargas del sistema, permitiendo priorizar el uso directo de la energía generada por el panel cuando esté disponible y reduciendo el uso innecesario del banco de baterías[cite: 2].

**13. Validación energética en condiciones reales de operación**[cite: 2]
Realizar pruebas prolongadas del sistema energético en terreno considerando ciclos completos día-noche y variaciones climáticas, con el objetivo de evaluar:[cite: 2]
* desempeño del sistema fotovoltaico[cite: 2]
* comportamiento del banco de baterías[cite: 2]
* balance energético entre generación y consumo[cite: 2]

Estas pruebas permitirán ajustar el dimensionamiento energético y validar la autonomía del sistema bajo escenarios reales de operación[cite: 2].

---

### 3. Integración, validación y pruebas del sistema[cite: 2]

**1. Integración electrónica en encapsulado robusto**[cite: 2]
Integrar los subsistemas dentro de un encapsulado adecuado para operación en terreno, con protección frente a:[cite: 2]
* polvo[cite: 2]
* humedad[cite: 2]
* chorros de agua[cite: 2]

**2. Validación con sistema de referencia**[cite: 2]
Realizar pruebas comparativas entre el sistema desarrollado y un sistema de adquisición de datos de referencia para validar precisión, estabilidad y calidad de las mediciones de vibración[cite: 2].

**3. Generación de línea base de comportamiento estructural**[cite: 2]
Realizar campañas iniciales de adquisición de datos con el objetivo de establecer una línea base de comportamiento dinámico de la estructura monitoreada[cite: 2]. Esta línea base permitirá:[cite: 2]
* identificar las frecuencias naturales de vibración de la estructura[cite: 2]
* caracterizar niveles normales de vibración ambiental[cite: 2]
* detectar variaciones anómalas en etapas posteriores de monitoreo[cite: 2]

La disponibilidad de una línea base de referencia permitirá mejorar la interpretación de los datos adquiridos y facilitar la detección temprana de posibles cambios estructurales[cite: 2].

**4. Pruebas operacionales en terreno**[cite: 2]
Realizar campañas de operación prolongada del sistema en condiciones reales de instalación con el objetivo de evaluar:[cite: 2]
* estabilidad del sistema de adquisición[cite: 2]
* desempeño del sistema energético[cite: 2]
* confiabilidad de la transmisión de datos[cite: 2]
* comportamiento del sistema frente a condiciones ambientales reales[cite: 2]

Estas pruebas permitirán validar la operación continua del sistema y detectar mejoras necesarias antes de su despliegue definitivo[cite: 2].

---

### 4. Evolución tecnológica del sistema[cite: 2]

Las actividades descritas buscan evolucionar el prototipo desarrollado durante la práctica hacia una plataforma tecnológica robusta, escalable y orientada a aplicaciones reales de monitoreo estructural[cite: 2].

Las mejoras propuestas abarcan aspectos de hardware, firmware, comunicaciones y gestión energética, junto con la optimización de la integración electrónica mediante placas dedicadas y encapsulados adecuados para operación en condiciones ambientales exigentes[cite: 2].

La incorporación de arquitecturas multi sensor basadas en bus I2C, nodos de adquisición distribuidos e integración con plataformas remotas de procesamiento y visualización permitirá ampliar significativamente las capacidades del sistema, habilitando su uso en redes de monitoreo estructural de infraestructura crítica como puentes, edificios, instalaciones industriales y obras civiles[cite: 2].

En etapas posteriores, el desarrollo se orientará a cumplir criterios de diseño y fabricación alineados con estándares internacionales de confiabilidad electrónica, robustez operativa y calidad de medición[cite: 2]. De esta forma, se busca consolidar una solución tecnológica que pueda evolucionar desde un prototipo experimental hacia un producto comercial dentro del ámbito del Structural Health Monitoring (SHM) y de los sistemas autónomos de adquisición de vibraciones de alta fidelidad[cite: 2].

---

## Cronograma con Prioridades[cite: 2]

### Sistema de Adquisición de Datos - Hermes A1 (6 meses)[cite: 2]

| N°[cite: 2] | Actividad[cite: 2] | Prioridad[cite: 2] | Mes 1[cite: 2] | Mes 2[cite: 2] | Mes 3[cite: 2] | Mes 4[cite: 2] | Mes 5[cite: 2] | Mes 6[cite: 2] |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1[cite: 2] | Implementación de adquisición continua[cite: 2] | P1[cite: 2] | | | | | | |
| 2[cite: 2] | Integración de métricas de calidad de señal[cite: 2] | P2[cite: 2] | | | | | | |
| 3[cite: 2] | Optimización mediante DMA[cite: 2] | P1[cite: 2] | | | | | | |
| 4[cite: 2] | Calibración y caracterización del acelerómetro[cite: 2] | P1[cite: 2] | | | | | | |
| 5[cite: 2] | Telemetría energética integrada[cite: 2] | P2[cite: 2] | | | | | | |
| 6[cite: 2] | Gestión remota del archivo de configuración[cite: 2] | P2[cite: 2] | | | | | | |
| 7[cite: 2] | Sistema de alarmas operativas[cite: 2] | P2[cite: 2] | | | | | | |
| 8[cite: 2] | Diseño de PCB dedicado para adquisición[cite: 2] | P1[cite: 2] | | | | | | |
| 9[cite: 2] | Arquitectura multisensor I2C[cite: 2] | P3[cite: 2] | | | | | | |
| 10[cite: 2] | Monitoreo del estado interno del sistema[cite: 2] | P2[cite: 2] | | | | | | |
| 11[cite: 2] | Reinicio remoto del sistema[cite: 2] | P3[cite: 2] | | | | | | |
| 12[cite: 2] | Compensación térmica de mediciones[cite: 2] | P2[cite: 2] | | | | | | |
| 13[cite: 2] | Implementación de watchdog[cite: 2] | P1[cite: 2] | | | | | | |
| 14[cite: 2] | Buffer local ante pérdida de conectividad[cite: 2] | P1[cite: 2] | | | | | | |
| 15[cite: 2] | Sincronización horaria del sistema[cite: 2] | P1[cite: 2] | | | | | | |
| 16[cite: 2] | Caracterización del sistema celular[cite: 2] | P2[cite: 2] | | | | | | |
| 17[cite: 2] | Optimización del consumo energético[cite: 2] | P1[cite: 2] | | | | | | |
| 18[cite: 2] | Migración del protocolo de transmisión[cite: 2] | P3[cite: 2] | | | | | | |

#### Resumen por Prioridad[cite: 2]

**Prioridad 1 Crítico**[cite: 2]
Funciones mínimas para operación confiable del sistema[cite: 2].
* adquisición continua[cite: 2]
* DMA[cite: 2]
* calibración del acelerómetro[cite: 2]
* diseño de PCB dedicado[cite: 2]
* watchdog[cite: 2]
* buffer local de datos[cite: 2]
* sincronización horaria[cite: 2]
* optimización energética[cite: 2]

Estas tareas definen si Hermes A1 puede operar en terreno 24/7[cite: 2].

**Prioridad 2 Alto**[cite: 2]
Mejoran confiabilidad, monitoreo y control[cite: 2].
* métricas de calidad de señal[cite: 2]
* telemetría energética[cite: 2]
* gestión remota de configuración[cite: 2]
* sistema de alarmas[cite: 2]
* monitoreo interno[cite: 2]
* compensación térmica[cite: 2]
* caracterización del enlace celular[cite: 2]

Permiten operación remota y diagnóstico del sistema[cite: 2].

**Prioridad 3 Evolución**[cite: 2]
Funciones avanzadas o expansión del sistema[cite: 2].
* arquitectura multisensor I²C[cite: 2]
* reinicio remoto[cite: 2]
* migración del protocolo de transmisión[cite: 2]

Son importantes para escalar la plataforma, pero no bloquean el funcionamiento inicial[cite: 2].

---

### Cronograma (2 meses)[cite: 2]
### Sistema Energético - Hermes A1[cite: 2]

| N°[cite: 2] | Actividad[cite: 2] | Prioridad[cite: 2] | Mes 1[cite: 2] | Mes 2[cite: 2] |
| :--- | :--- | :--- | :--- | :--- |
| 1[cite: 2] | Validación del banco de baterías[cite: 2] | P1[cite: 2] | | |
| 2[cite: 2] | Monitoreo del estado de baterías[cite: 2] | P1[cite: 2] | | |
| 3[cite: 2] | Monitoreo del sistema fotovoltaico[cite: 2] | P1[cite: 2] | | |
| 4[cite: 2] | Detección de fallas del sistema fotovoltaico[cite: 2] | P2[cite: 2] | | |
| 5[cite: 2] | Sistema modular de baterias[cite: 2] | P2[cite: 2] | | |
| 6[cite: 2] | PCB compacto para el sistema de alimentación[cite: 2] | P1[cite: 2] | | |
| 7[cite: 2] | Conectores rápidos para panel solar[cite: 2] | P2[cite: 2] | | |
| 8[cite: 2] | Interfaz de configuración local USB-C[cite: 2] | P3[cite: 2] | | |
| 9[cite: 2] | Gestión energética adaptativa[cite: 2] | P2[cite: 2] | | |
| 10[cite: 2] | Arquitectura modular de salidas de energía[cite: 2] | P3[cite: 2] | | |
| 11[cite: 2] | Integración de protecciones eléctricas[cite: 2] | P1[cite: 2] | | |
| 12[cite: 2] | Optimización del bypass energético[cite: 2] | P2[cite: 2] | | |
| 13[cite: 2] | Validación energética en condiciones reales[cite: 2] | P1[cite: 2] | | |

#### Resumen por prioridad[cite: 2]

**Prioridad 1 Crítico**[cite: 2]
Necesarios para que el sistema funcione en terreno[cite: 2].
* validación banco de baterías[cite: 2]
* monitoreo de baterías[cite: 2]
* monitoreo fotovoltaico[cite: 2]
* protecciones eléctricas[cite: 2]
* PCB del sistema de alimentación[cite: 2]
* validación energética real[cite: 2]

**Prioridad 2 Alto**[cite: 2]
Mejoran estabilidad y operación autónoma[cite: 2].
* detección de fallas del panel[cite: 2]
* sistema modular de baterías[cite: 2]
* gestión energética adaptativa[cite: 2]
* optimización del bypass[cite: 2]

**Prioridad 3 Evolución**[cite: 2]
Mejoras de diseño y mantenimiento[cite: 2].
* interfaz USB-C[cite: 2]
* salidas energéticas modulares[cite: 2]