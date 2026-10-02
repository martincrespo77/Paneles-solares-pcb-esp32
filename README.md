# Seguidor Solar (Sun Tracker) de 2 Ejes con ESP32

**Cátedra:** Técnicas Digitales 2 — FIE (Facultad de Ingeniería del Ejército)  
**Estado:** Firmware base implementado y verificado en PlatformIO  

---

## 1. Descripción del Proyecto

Este proyecto consiste en la **migración y modernización del sistema de control del Seguidor Solar (Sun Tracker) de dos ejes** (Azimut y Elevación). 

El sistema original estaba implementado en código de bajo nivel en C para un microcontrolador AVR (**ATmega168/328**) utilizando el puente H L298HN y un demultiplexor CD4053 (referencia: informe técnico `UNDEFI 263` en `INFORMACION VIEJA/`).

En esta etapa se porta el núcleo de control hacia un microcontrolador moderno **ESP32 DOIT DevKit V1 (30 pines)** bajo el framework PlatformIO / Arduino C++, conservando las ecuaciones físicas del lazo de control, la calibración PI probada y la robustez del accionamiento.

---

## 2. Arquitectura de Control

```
                 [ 4 LDRs en cruz: F0, F1, F2, F3 ]
                                |
                                v
               [ ADC1 del ESP32 (4 canales a 10 bits) ]
                                |
                                v
                [ Cálculo de error (Azimut / Elevación) ]
                                |
                                v
             [ Control PI por eje con Anti-Windup y Deadband ]
                                |
                                v
            [ Generación PWM (LEDC 1 kHz) + Señales de Sentido ]
                                |
                                v
                [ Etapa de Potencia: Puente H Doble ]
                                |
                                v
              [ Motores DC Azimut (P1) y Elevación (P2) ]
```

### Algoritmo y Ley de Control

1. **Lectura ADC:** Lee los 4 sensores LDR sobre el ADC1 (evitando conflictos de Wi-Fi), aplicando promediado de 8 muestras y reescalado de 12 a 10 bits (`0..1023`) para mantener compatibilidad con las constantes legadas.
2. **Cálculo de Error:**
   - Elevación: `Error_El = (F2 + F3 - F0 - F1) / 2`
   - Azimut: `Error_Az = (F1 + F3 - F0 - F2) / 2`
3. **Controlador PI Discreto:**
   - Ganancias: $K_p = 5.0$, $K_i = 0.1$, $K_d = 0.0$
   - Límite Anti-Windup: Clamping del integrador a $\pm 400.0$
   - Zona Muerta (`DEAD_BAND = 5`): Evita oscilaciones por jitter o ruido eléctrico cuando el panel está orientado.
4. **Modulación PWM:** Canales LEDC a 1 kHz con 10 bits de resolución (`PWM_MAX = 1023`).

---

## 3. Mapeo de Pines (ESP32 DevKit V1)

| Señal | Pin ESP32 | Función | Destino en Hardware |
|---|:---:|---|---|
| **FOTO0** | `GPIO36` (VP) | Entrada ADC1 Ch0 | LDR 0 (Superior izquierdo) |
| **FOTO1** | `GPIO39` (VN) | Entrada ADC1 Ch3 | LDR 1 (Superior derecho) |
| **FOTO2** | `GPIO34` | Entrada ADC1 Ch6 | LDR 2 (Inferior izquierdo) |
| **FOTO3** | `GPIO35` | Entrada ADC1 Ch7 | LDR 3 (Inferior derecho) |
| **PWM_AZ** | `GPIO25` | Salida PWM (LEDC CH0) | ENA / Entrada PWM motor Azimut |
| **PWM_EL** | `GPIO26` | Salida PWM (LEDC CH1) | ENB / Entrada PWM motor Elevación |
| **DIR_AZ** | `GPIO27` | Salida Digital | IN_A / Sentido de giro Azimut |
| **DIR_EL** | `GPIO14` | Salida Digital | IN_B / Sentido de giro Elevación |
| **STATUS** | `GPIO2` | Salida Digital | LED onboard (Heartbeat / Fault) |

---

## 4. Estructura del Repositorio

*   [`src/main_tracker.cpp`](src/main_tracker.cpp): Firmware principal en C++ para la ESP32.
*   [`platformio.ini`](platformio.ini): Configuración de compilación para ESP32 DOIT DevKit V1.
*   [`Kicad/Control de panel/`](Kicad/Control%20de%20panel/): Proyecto de esquemático y PCB en KiCad adaptado para zócalo ESP32 DevKit V1 30 pines.
*   [`docs/`](docs/):
    *   [`CONTEXTO_TD2_SUN_TRACKER.md`](docs/CONTEXTO_TD2_SUN_TRACKER.md): Contexto técnico detallado y trazabilidad del proyecto.
    *   [`DOCUMENTO_TECNICO_TD2_SUN_TRACKER.md`](docs/DOCUMENTO_TECNICO_TD2_SUN_TRACKER.md): Especificación técnica formal y modelo matemático.
    *   [`PLANMODE_ARMADO_TD2.md`](docs/PLANMODE_ARMADO_TD2.md): Guía de validación y puesta en marcha en banco por etapas.
*   [`INFORMACION VIEJA/`](INFORMACION%20VIEJA/): Archivos históricos de referencia (informe final UNDEFI 263, esquemático analógico y firmware AVR en C).

---

## 5. Compilación y Carga

Con [PlatformIO](https://platformio.org/) instalado:

```powershell
# Compilar el firmware del Sun Tracker
pio run -e tracker

# Cargar a la placa ESP32 (ajustar puerto COM en platformio.ini si difiere)
pio run -e tracker -t upload

# Abrir el monitor serie (115200 baud)
pio device monitor -e tracker
```
