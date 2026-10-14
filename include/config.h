/**
 * @file    config.h
 * @brief   Sun Tracker ESP32 — pines, perifericos y parametros de calibracion.
 * @details Unico lugar donde se ajusta el hardware y la sintonia del lazo.
 *          src/main_tracker.cpp no contiene numeros magicos.
 *
 * Trazabilidad con el firmware AVR (INFORMACION VIEJA → docs/legacy/firmware_avr):
 *   Constantes.c  → KP_*, KI_*, RESET_AW
 *   PWM.c         → PWM 10 bits (OCR1A/OCR1B, 0..1023)
 *   PORT.c        → PB4 = sentido azimut, PB5 = sentido elevacion
 */
#pragma once

#include <Arduino.h>

// ============================================================================
// PINES — ESP32 DOIT DevKit V1 (30 pines)
// ============================================================================
// Sensores LDR (divisor LDR a 3V3 + 4k7 a GND → mas luz = mas tension).
// Todos en ADC1: el ADC2 queda inutilizable cuando el WiFi esta activo.
// GPIO34..39 son solo-entrada y sin pull-up/pull-down internos (correcto para ADC).
constexpr uint8_t PIN_F0 = 36;   // ADC1_CH0 (VP) — LDR0 superior izquierdo
constexpr uint8_t PIN_F1 = 39;   // ADC1_CH3 (VN) — LDR1 superior derecho
constexpr uint8_t PIN_F2 = 34;   // ADC1_CH6      — LDR2 inferior izquierdo
constexpr uint8_t PIN_F3 = 35;   // ADC1_CH7      — LDR3 inferior derecho

// Salidas al puente H. Ninguno es strapping pin.
// GPIO14 emite pulsos durante el boot (JTAG/SDIO): inocuo porque ENA/ENB
// (GPIO25/26) arrancan en alta impedancia → poner pull-down de 10k en ENA/ENB.
constexpr uint8_t PIN_PWM_AZ = 25;   // ENA — PWM azimut
constexpr uint8_t PIN_PWM_EL = 26;   // ENB — PWM elevacion
constexpr uint8_t PIN_DIR_AZ = 27;   // sentido azimut   (equiv. PB4)
constexpr uint8_t PIN_DIR_EL = 14;   // sentido elevacion (equiv. PB5)

// LED integrado. GPIO2 es strapping pin (debe estar en LOW/flotante al
// entrar en modo descarga); como salida de LED no interfiere.
constexpr uint8_t PIN_STATUS = 2;

// ============================================================================
// FINALES DE CARRERA (opcional — deshabilitado hasta confirmar hardware)
// ============================================================================
// Contacto NC a GND recomendado (cable cortado = carrera activada = fail-safe).
// Lectura con INPUT_PULLUP: LOW = libre, HIGH = activado/cable abierto.
// Verificar en el PCB que estos GPIO esten libres antes de habilitar.
#define USE_LIMIT_SWITCHES 0

constexpr uint8_t PIN_LIM_AZ_POS = 32;   // azimut, sentido de error positivo
constexpr uint8_t PIN_LIM_AZ_NEG = 33;   // azimut, sentido de error negativo
constexpr uint8_t PIN_LIM_EL_POS = 18;   // elevacion, sentido de error positivo
constexpr uint8_t PIN_LIM_EL_NEG = 19;   // elevacion, sentido de error negativo

// ============================================================================
// SENTIDO DE GIRO — replica literal del AVR
// ============================================================================
//   PID_Elev.c: u >= 0 → PORTB |= 0x20 (PB5 = 1) → DIR_EL HIGH
//   PID_Azim.c: u >= 0 → PORTB &= 0xEF (PB4 = 0) → DIR_AZ LOW
// Si un motor gira al reves en banco, invertir aqui (no en el cableado).
constexpr uint8_t DIR_EL_POSITIVE_LEVEL = HIGH;
constexpr uint8_t DIR_AZ_POSITIVE_LEVEL = LOW;

// ============================================================================
// LEDC (PWM por hardware)
// ============================================================================
constexpr uint8_t  LEDC_CH_AZ    = 0;      // canales 0 y 1 → mismo timer (0)
constexpr uint8_t  LEDC_CH_EL    = 1;
constexpr uint32_t LEDC_FREQ_HZ  = 1000;   // 1 kHz — adecuado para L298/L298N
constexpr uint8_t  LEDC_RES_BITS = 10;     // 0..1023, igual que OCR1x del AVR
constexpr int      PWM_MAX       = (1 << LEDC_RES_BITS) - 1;

// ============================================================================
// ADC
// ============================================================================
// Se lee con analogReadMilliVolts() (calibrado con Vref de eFuse) y se
// reescala a 10 bits para conservar las constantes PI del AVR.
constexpr uint8_t  ADC_SAMPLES   = 8;      // promedio por canal y por ciclo
constexpr uint32_t ADC_FULL_MV   = 3300;   // tension equivalente a 1023 cuentas
constexpr int      ADC_MAX_COUNT = 1023;

// ============================================================================
// LAZO PI — Constantes.c (AVR), forma discreta por muestra
// ============================================================================
//   I[k] = sat( I[k-1] + e[k] , ±RESET_AW )
//   u[k] = KP·e[k] + KI·I[k]
// KI es ganancia POR MUESTRA (el AVR no multiplicaba por dt). Equivalente
// continuo: Ki_c = KI / Ts = 0.1 / 0.02 s = 5 s^-1. Si se cambia CONTROL_PERIOD_MS
// hay que reescalar KI para conservar la dinamica.
constexpr float KP_AZ    = 5.0f;
constexpr float KI_AZ    = 0.1f;
constexpr float KP_EL    = 5.0f;
constexpr float KI_EL    = 0.1f;
constexpr float RESET_AW = 400.0f;   // aporte integral maximo = KI·RESET_AW = 40 cuentas

// Zona muerta (agregado respecto al AVR): |e| <= DEAD_BAND → motor parado e
// integrador en cero. Evita "cazar" por ruido de ADC con el panel orientado.
constexpr float DEAD_BAND = 5.0f;    // cuentas ADC (escala 10 bits)

// ============================================================================
// TIEMPOS
// ============================================================================
constexpr uint32_t CONTROL_PERIOD_MS   = 20;    // Ts = 20 ms → 50 Hz
constexpr uint32_t TELEMETRY_PERIOD_MS = 500;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 1000;
constexpr uint32_t DIR_CHANGE_DEADTIME_MS = 40; // PWM=0 antes de invertir giro

// ============================================================================
// FAIL-SAFES
// ============================================================================
// Noche / oscuridad: promedio de los 4 LDR por debajo del umbral. Histeresis
// y confirmacion temporal para no conmutar con nubes o sombras pasajeras.
constexpr int      NIGHT_ENTER_LEVEL = 40;     // cuentas (≈130 mV)
constexpr int      NIGHT_EXIT_LEVEL  = 80;     // cuentas (≈260 mV)
constexpr uint32_t NIGHT_CONFIRM_MS  = 10000;

// Sensor desconectado/en corto a GND: un LDR casi en 0 mientras hay luz
// ambiente (promedio > NIGHT_ENTER_LEVEL). Con 4k7 de carga, un LDR tapado
// sigue dando decenas de cuentas por luz difusa.
constexpr int      SENSOR_MIN_VALID      = 10;   // cuentas (≈32 mV)
constexpr uint32_t SENSOR_FAULT_CONFIRM_MS = 200;
constexpr uint32_t SENSOR_FAULT_CLEAR_MS   = 2000;

// Watchdog del loop de Arduino (resetea el ESP32 si el loop se cuelga;
// al reiniciar, los motores arrancan detenidos).
#define USE_LOOP_WDT 1
