# TD2 — Sun Tracker ESP32: Contexto del proyecto y plan de migracion

**Materia:** Tecnicas Digitales 2  
**Estado:** Firmware PI implementado; pendiente validacion en banco  
**Fecha de escritura:** 2026-06-09 — **Ultima revision:** 2026-10-14

---

## 0. Proposito de este documento

Documento de referencia **unico** del proyecto: contexto, trazabilidad con el
sistema AVR original, decisiones de migracion y estado actual.
Si algo esta mal descrito aqui, hay que corregir este documento, no arrancar a codear.

> Reemplaza a `CONTEXTO_MIGRACION_SUN_TRACKER_ESP32.md` (analisis inicial de mayo
> 2026, cuando se evaluaba MicroPython y se mezclaba con Optoelectronica). Ese
> archivo se conserva solo como historico en
> [`legacy/CONTEXTO_MIGRACION_INICIAL.md`](legacy/CONTEXTO_MIGRACION_INICIAL.md).

---

## 1. Que hay que hacer (en dos palabras)

Tomar el **Sun Tracker de dos ejes que ya existe en C para AVR** y portarlo a una
**ESP32 programada en C++ con PlatformIO/Arduino**.

La logica de control PI, los sensores LDR, el puente H y los motores se conservan.
El microcontrolador cambia de ATmega168/328 a ESP32.

---

## 2. Que ya existe (material viejo)

Todo el material fuente esta en `docs/legacy/`.

### Informe tecnico

`docs/legacy/UNDEFI 263 - Informe Final.pdf`

Describe el sistema completo: objetivo, mecanica, sensores, motores, puente H,
control PI discreto y circuito electronico.

### Esquematico electronico

`docs/legacy/circuito/Sun_seeker.PDF`

Circuito con ATmega168/328, L298HN, CD4053BCN, LM7805, LDR (4x) y conectores de motores.

### Firmware AVR (fuente de referencia principal)

`docs/legacy/firmware_avr/ (fuentes) y docs/legacy/firmware_avr_original.rar`

Proyecto Atmel Studio. Archivos clave:

| Archivo | Rol |
|---|---|
| `popo.c` | Punto de entrada, loop principal |
| `ADC.c` | Configuracion ADC de 10 bits, referencia externa |
| `Leo_Fotos.c` | Lectura secuencial de ADC0..ADC3 |
| `PWM.c` | Timer1 Fast PWM 10 bits, OC1A y OC1B |
| `PID_Elev.c` | Control PI de elevacion |
| `PID_Azim.c` | Control PI de azimut |
| `PORT.c` | Configuracion de pines |
| `Constantes.c` | Inicializacion de ganancias y variables |
| `USART.c` | UART de debug (secundaria) |
| `Def.h` | Definiciones comunes |

---

## 3. Como funciona el sistema viejo

### Sensado de luz (4 LDR)

Cuatro LDR colocados en un discriminador de sombra en forma de cruz:

```
    [FOTO0]  [FOTO1]
    ----|--------|----   <- barrera horizontal
    [FOTO2]  [FOTO3]
```

Conectados a ADC0..ADC3 del AVR (10 bits, referencia externa AREF, single conversion).

### Calculo de error

```c
// Error de elevacion (eje vertical)
Error_El = (adc_dato_2 + adc_dato_3 - adc_dato_0 - adc_dato_1) / 2;

// Error de azimut (eje horizontal)
Error_Az = (adc_dato_1 + adc_dato_3 - adc_dato_0 - adc_dato_2) / 2;
```

Cuando el panel esta bien orientado, los cuatro LDR reciben la misma luz y el
error tiende a cero.

### Ley de control (PI, no PID)

Aunque los archivos se llaman `PID_*.c`, el codigo activo implementa un **PI**.
`Kd` existe como variable pero no participa.

Constantes del firmware viejo:

```c
Kp_El = 5.0;   Ki_El = 0.1;   Kd_El = 0.0;   // elevacion
Kp_Az = 5.0;   Ki_Az = 0.1;   Kd_Az = 0.0;   // azimut

Reset_aw = 400.0;  // limite anti-windup (valor absoluto del integrador)
```

Salida del PI para cada eje:

```
integrador += error * dt          // acumulacion
if |integrador| > Reset_aw:       // anti-windup por clamping
    integrador = signo * Reset_aw
pwm = Kp * error + Ki * integrador
pwm = clamp(pwm, 0, 1023)         // saturacion (10 bits)
sentido = (pwm_neto > 0) ? 1 : 0  // bit de direccion
OCR1A o OCR1B = |pwm|             // escritura Timer1
```

### Pines del AVR (referencia)

| AVR pin | Funcion | Nombre en firmware |
|---|---|---|
| `PB0` | bandera debug (osciloscopio) | — |
| `PB1 / OC1A` | PWM azimut | `PWMA` |
| `PB2 / OC1B` | PWM elevacion | `PWME` |
| `PB4` | sentido azimut | `SENTA` |
| `PB5` | sentido elevacion | `SENTE` |
| `ADC0` | FOTO0 | `adc_dato_0` |
| `ADC1` | FOTO1 | `adc_dato_1` |
| `ADC2` | FOTO2 | `adc_dato_2` |
| `ADC3` | FOTO3 | `adc_dato_3` |

---

## 4. Circuito electronico (hardware viejo, segun esquematico)

```
4 LDR -> ADC0..ADC3 del ATmega
      -> calculo de error
      -> PI por eje
      -> PWM + bit de sentido
      -> CD4053BCN (multiplexor analogico de control)
      -> L298HN (puente H doble)
      -> 2 motores DC con reductor (azimut y elevacion)
```

Componentes principales:

| Ref | Parte | Funcion |
|---|---|---|
| U1 | ATmega168/328 | Controlador digital |
| U2 | L298HN | Puente H doble (dos motores) |
| U3 | CD4053BCN | Multiplexor/demux analogico de senales de control |
| U4 | LM7805 | Regulador 5 V para la logica |
| D1-D8 | FR107 | Diodos de rueda libre (proteccion inductiva) |
| Y1 | Cristal 16 MHz | Reloj del AVR |
| R2-R5 | 4k7 | Resistencias de carga para los LDR |
| P1 | Motor azimut | IGNIS MR08D-012004-4.0 / Mabuchi RS-385PH-2085 |
| P2 | Motor elevacion | Idem |
| P3 | Conector sensores | FOTO0..FOTO3 + alimentacion |
| P4 | Alimentacion | 9 Vcc segun esquema (informe dice 12 Vcc: VERIFICAR) |

---

## 5. Migracion a ESP32 — lo que hay que construir

### Diagrama objetivo

```
4 LDR (4k7 pull-down)
   |
ADC1 del ESP32 (12 bits, canales 34/35/36/39)
   |
calculo de error (misma formula del AVR)
   |
PI por eje (mismas constantes de partida, ajustar en banco)
   |
LEDC PWM 12 bits + GPIO de sentido
   |
Puente H (L298N o equivalente)
   |
Motor azimut + Motor elevacion
```

### Pines propuestos para ESP32 DevKit

| ESP32 GPIO | Funcion | Notas |
|---|---|---|
| GPIO36 (VP) | FOTO0 (LDR0) | ADC1 ch0, solo entrada |
| GPIO39 (VN) | FOTO1 (LDR1) | ADC1 ch3, solo entrada |
| GPIO34 | FOTO2 (LDR2) | ADC1 ch6, solo entrada |
| GPIO35 | FOTO3 (LDR3) | ADC1 ch7, solo entrada |
| GPIO25 | PWM azimut | LEDC, salida analogica posible |
| GPIO26 | PWM elevacion | LEDC |
| GPIO27 | Sentido azimut | GPIO digital |
| GPIO14 | Sentido elevacion | GPIO digital |
| GPIO2 | LED estado (integrado) | debug/heartbeat |
| GPIO0 | — | NO usar (boot) |

> **Nota ADC ESP32:** El ADC del ESP32 no es lineal. Requiere atenuacion
> `ADC_11db` (0..3.3 V) y calibracion practica. El rango util real es
> aprox. 150..3900 cuentas de las 4096. Tener en cuenta al escalar el error.

> **Nota niveles logicos:** El ESP32 trabaja a 3.3 V logicos. Si se reutiliza
> el L298 o el CD4053 alimentados a 5 V, verificar compatibilidad de entradas.
> El L298 acepta niveles de 3.3 V en sus entradas de control.

### Estructura del firmware (`src/main_tracker.cpp` + `include/config.h`)

- `include/config.h`: pines, LEDC, ganancias PI, tiempos y umbrales de seguridad.
  Es el unico archivo que se toca para recalibrar.
- `src/main_tracker.cpp`:
  1. **Lectura ADC** — `analogReadMilliVolts()` (calibrado por eFuse), 8 muestras,
     reescalado a 10 bits para conservar las constantes del AVR.
  2. **Supervision** — maquina de estados `TRACKING / NIGHT / SENSOR_FAULT` con
     histeresis y confirmacion temporal.
  3. **Calculo de error** — formulas identicas a `PID_Elev.c` / `PID_Azim.c`.
  4. **PI por eje** — `I = sat(I + e, ±400)`, `u = Kp·e + Ki·I`, zona muerta ±5.
  5. **Salida** — PWM LEDC 1 kHz / 10 bits + pin de sentido, con tiempo muerto
     de 40 ms antes de invertir el giro. Finales de carrera opcionales.
  6. **Planificador** — `loop()` no bloqueante con `millis()`, Ts = 20 ms sin
     deriva, contador de overruns y watchdog del loop.

---

## 6. Diferencias tecnicas detectadas entre AVR y ESP32

| Aspecto | ATmega168/328 | ESP32 | Accion |
|---|---|---|---|
| ADC bits | 10 bits (0..1023) | 12 bits (0..4095) | Escalar error y anti-windup |
| ADC linealidad | Buena con AREF | No lineal, necesita calibracion | Agregar tabla o compensacion |
| PWM bits | 10 bits (0..1023) | 1..16 bits via LEDC | Usar 10 bits para conservar Kp/Ki |
| Logica | 5 V | 3.3 V | Verificar compatibilidad con L298 |
| Tiempo de loop | Timer por interrupcion (Timer0) | FreeRTOS / loop() de Arduino | Fijar `dt` real con `millis()` |
| USART debug | 57600 baud (dudoso a 16 MHz) | Serial normal a 115200 | Usar Serial ESP32 normalmente |

---

## 7. Pendientes antes de implementar

Confirmar fisicamente antes de escribir una sola linea de codigo:

- [ ] Que puente H se usara: L298N de modulo, BTS7960, TB6612FNG u otro.
- [ ] Tension real de alimentacion de motores: 9 V o 12 V (el esquema dice 9 V, el informe dice 12 V).
- [ ] Corriente de arranque o bloqueo de cada motor (para no quemar el driver).
- [ ] Si existen finales de carrera fisicos o no.
- [ ] Si se conservan los cuatro LDR originales y su disposicion.
- [ ] Modelo exacto de ESP32 disponible (DevKit V1, WROOM-32 30p u otro).
- [ ] Si el proyecto exige MicroPython o se acepta C++ con Arduino/PlatformIO.

---

## 8. Riesgos tecnicos conocidos

- **Ruido electrico de motores:** separar masa de potencia y logica. Capacitores en
  paralelo a los motores (100 nF ceramico + 100 uF electrolitico).
- **ADC ESP32 + motores en mismo PCB:** el ruido de conmutacion del puente H puede
  degradar las lecturas ADC. Filtros RC en las entradas de los LDR.
- **MicroPython no es tiempo real duro:** para seguimiento solar (movimientos lentos)
  puede alcanzar, pero no sirve si se necesitan interrupciones criticas.
- **Sin finales de carrera:** si el motor fuerza el limite mecanico, puede romper la
  estructura. Agregar corte por corriente o sensores de fin de recorrido.
- **L298 disipacion:** el L298 cae 2 V internos. Con 12 V de alimentacion, los motores
  reciben ~10 V reales. Considerar ventilacion si la corriente supera 1 A.

---

## 9. Relacion con el proyecto de Optoelectronica (FINALIZADO)

El proyecto de Optoelectronica es independiente y esta cerrado.

Usa los mismos ESP32 pero para un sistema de comunicacion optica laser FSO.
La telemetria que transmite ese sistema (`TelemetryData`) incluye campos de
este tracker (`F0..F3`, `errAz`, `errEl`, `motAz`, `motEl`) como datos **simulados**.

Cuando el tracker TD2 este operativo, esa simulacion se puede reemplazar por
lecturas reales, pero es una decision de integracion futura.

Los dos proyectos **no comparten codigo fuente en la misma compilacion**.
Cada uno tiene su propio entorno en `platformio.ini`.

---

## 10. Estado actual del repo para TD2

| Elemento | Estado |
|---|---|
| `docs/legacy/` (informe, esquematico y firmware AVR) | Versionado, fuentes descomprimidas |
| `src/main_tracker.cpp` + `include/config.h` | Implementado, compila sin warnings (`-Wall -Wextra -Werror`) |
| `docs/CONTEXTO_TD2_SUN_TRACKER.md` | Este archivo (referencia unica) |
| `platformio.ini` | `espressif32@6.13.0` (Arduino core 2.0.17) fijado; envs `tracker` y `tracker_manual` |
| Control PI portado | **Hecho** — pendiente sintonia en banco |
| Fail-safes (noche, sensor, inversion, WDT) | **Hecho** — umbrales a calibrar con el hardware real |
| Finales de carrera | Soporte en firmware, deshabilitado (`USE_LIMIT_SWITCHES 0`) |
| PCB KiCad (`Kicad/Control de panel/`) | Adaptado a ESP32 DevKit V1 30 pines |
| Esquema de pines ESP32 | Definido en `config.h`, pendiente verificacion fisica |
