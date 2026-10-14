# Seguidor Solar (Sun Tracker) de 2 Ejes con ESP32

**Cátedra:** Técnicas Digitales 2 — FIE (Facultad de Ingeniería del Ejército)  
**Plataforma:** ESP32 DOIT DevKit V1 (30 pines) · PlatformIO / Arduino C++  
**Estado:** Firmware implementado, compila sin warnings. Pendiente validación en banco.

---

## 1. Descripción

Migración del sistema de control de un **seguidor solar de dos ejes** (Azimut y Elevación) desde la plataforma original AVR (**ATmega168/328**, puente H L298HN y CD4053 — informe técnico *UNDEFI 263*) a un **ESP32**.

Se conservan las ecuaciones físicas del lazo, la calibración PI probada en el sistema original y la topología de sensado. Se agregan supervisión de seguridad, temporización determinística y una arquitectura de firmware mantenible.

---

## 2. Arquitectura

```
          [ 4 LDR en cruz: F0 F1 / F2 F3 ]   (divisor LDR + 4k7)
                         |
                         v
          [ ADC1 ESP32 — 8 muestras, calibrado, escala 10 bits ]
                         |
                         v
          [ Supervisión: TRACKING / NIGHT / SENSOR_FAULT ]
                         |
                         v
          [ Error Azimut / Elevación ]
                         |
                         v
          [ PI por eje + anti-windup + zona muerta ]
                         |
                         v
          [ PWM LEDC 1 kHz / 10 bits + sentido (tiempo muerto) ]
                         |
                         v
          [ Puente H doble (L298N) ]
                         |
                         v
          [ Motor DC Azimut ]   [ Motor DC Elevación ]
```

### Ley de control

| Paso | Ecuación / valor |
|---|---|
| Error elevación | `e_El = (F2 + F3 − F0 − F1) / 2` (inferior − superior) |
| Error azimut | `e_Az = (F1 + F3 − F0 − F2) / 2` (derecha − izquierda) |
| Zona muerta | `abs(e) ≤ 5` → motor parado, integrador en 0 |
| Integrador | `I[k] = sat(I[k−1] + e[k], ±400)` |
| Acción | `u[k] = Kp·e[k] + Ki·I[k]`, con `Kp = 5.0`, `Ki = 0.1` (por muestra) |
| Salida | `PWM = min(abs(u), 1023)`, sentido = signo de `u` |
| Período | `Ts = 20 ms` (50 Hz) → `Ki` continuo equivalente = 5 s⁻¹ |

### Fail-safes

| Condición | Detección | Acción |
|---|---|---|
| Noche / oscuridad | Promedio LDR < 40 cuentas durante 10 s (sale con > 80) | Motores parados, LED destello corto |
| LDR desconectado | Un LDR < 10 cuentas con luz ambiente, 200 ms | Motores parados, LED fijo |
| Inversión de giro | Cambio de signo de `u` | PWM = 0 durante 40 ms antes de invertir |
| Final de carrera | Opcional (`USE_LIMIT_SWITCHES`) | Bloquea el sentido activado |
| Cuelgue del firmware | Watchdog del `loop()` | Reset → arranque con motores parados |

Todos los umbrales están en [`include/config.h`](include/config.h).

---

## 3. Mapeo de pines (ESP32 DevKit V1)

| Señal | Pin | Función | Destino |
|---|:---:|---|---|
| FOTO0 | `GPIO36` (VP) | ADC1_CH0 | LDR superior izquierdo |
| FOTO1 | `GPIO39` (VN) | ADC1_CH3 | LDR superior derecho |
| FOTO2 | `GPIO34` | ADC1_CH6 | LDR inferior izquierdo |
| FOTO3 | `GPIO35` | ADC1_CH7 | LDR inferior derecho |
| PWM_AZ | `GPIO25` | LEDC CH0 | ENA — motor Azimut |
| PWM_EL | `GPIO26` | LEDC CH1 | ENB — motor Elevación |
| DIR_AZ | `GPIO27` | Digital | Sentido Azimut |
| DIR_EL | `GPIO14` | Digital | Sentido Elevación |
| STATUS | `GPIO2` | Digital | LED onboard |

Solo se usa **ADC1** (el ADC2 queda inutilizable con WiFi activo) y ningún strapping pin crítico. Se recomienda pull-down de 10 kΩ en ENA/ENB para garantizar motores parados durante el boot.

> **L298N:** cada motor necesita IN1/IN2 complementarios. Con un único pin de sentido por motor hace falta un inversor (en el diseño original lo resolvía el CD4053).

---

## 4. Estructura del repositorio

```
├── platformio.ini              Configuración de compilación (platform fijada)
├── include/config.h            Pines, ganancias, tiempos y umbrales
├── src/main_tracker.cpp        Firmware del tracker
├── Kicad/Control de panel/     Esquemático y PCB (KiCad), zócalo DevKit 30 pines
│   └── lib/                    Símbolo y footprint ESP32_DEVKIT_V1_30PIN
└── docs/
    ├── CONTEXTO_TD2_SUN_TRACKER.md          Referencia única: contexto y trazabilidad
    ├── DOCUMENTO_TECNICO_TD2_SUN_TRACKER.md Especificación técnica y modelo
    ├── PLANMODE_ARMADO_TD2.md               Puesta en marcha en banco por etapas
    ├── img/                                 Imágenes y diagramas
    └── legacy/                              Sistema AVR original (solo referencia)
        ├── UNDEFI 263 - Informe Final.pdf
        ├── circuito/Sun_seeker.PDF
        └── firmware_avr/                    Fuentes C del ATmega (popo.c, PID_*.c…)
```

---

## 5. Compilación y carga

Requiere [PlatformIO](https://platformio.org/) (extensión de VS Code o CLI). Ajustar `upload_port` / `monitor_port` en `platformio.ini` al puerto COM de la placa.

```powershell
pio run -e tracker                  # compilar
pio run -e tracker -t upload        # cargar
pio device monitor -e tracker       # monitor serie a 115200 baud
```

Si la placa no entra sola en modo descarga, usar `-e tracker_manual` y mantener presionado **BOOT** al iniciar la carga.

---

## 6. Puesta en marcha

1. **Sin motores:** cargar el firmware y abrir el monitor. Verificar el banner de boot.
2. **Sensores:** sombrear cada LDR por separado y comprobar que baja solo su canal `F`.
3. **Signo del error:** iluminar desde abajo → `eEl` positivo; desde la derecha → `eAz` positivo.
4. **Fail-safes:** desconectar un LDR → `[STATE] TRACK -> FAULT`; tapar todos 10 s → `NIGHT`.
5. **Motores en vacío:** confirmar que cada eje gira hacia la luz. Si gira al revés, invertir `DIR_*_POSITIVE_LEVEL` en `config.h`.
6. **Sintonía:** ajustar `DEAD_BAND`, `KP_*`, `KI_*` según la mecánica real.

Formato de telemetría (cada 500 ms):

```
[   12500] TRACK F= 512  530  498  515 | eAz= +17.5 eEl=  -14.5 | iAz= +35.0 iEl= -29.0 | pwmAz=+  91 pwmEl=-  75 | ovr=0
```

Guía detallada: [`docs/PLANMODE_ARMADO_TD2.md`](docs/PLANMODE_ARMADO_TD2.md).
