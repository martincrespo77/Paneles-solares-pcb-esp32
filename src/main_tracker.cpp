/**
 * @file    main_tracker.cpp
 * @brief   Sun Tracker ESP32 — Tecnicas Digitales 2
 * @details Migra el controlador PI del firmware AVR (ATmega168/328, popo.c) a
 *          ESP32 DevKit V1 con PlatformIO/Arduino.
 *
 * Fuente original: INFORMACION VIEJA/.../popo/popo/{PID_Azim.c, PID_Elev.c,
 *                  Leo_Fotos.c, Constantes.c, PWM.c, PORT.c}
 *
 * Mapeo AVR -> ESP32
 * ------------------
 *   ADC0 (FOTO0) -> GPIO36   ADC0 (FOTO1) -> GPIO39
 *   ADC2 (FOTO2) -> GPIO34   ADC3 (FOTO3) -> GPIO35
 *   OC1A / PB1  -> GPIO25   (LEDC CH0 — PWM azimut)
 *   OC1B / PB2  -> GPIO26   (LEDC CH1 — PWM elevacion)
 *   PB4         -> GPIO27   (sentido azimut)
 *   PB5         -> GPIO14   (sentido elevacion)
 *
 * Datos de hardware faltantes (ver docs/PLANMODE_ARMADO_TD2.md):
 *   - Puente H final no definido  → ajustar pines ENA/ENB/IN segun driver
 *   - Tension real de motores     → se asume 9-12V, verificar en banco
 *   - Finales de carrera          → descomentar FAULT si se conectan
 *
 * @date 2026-06-09
 */

#include <Arduino.h>

// ============================================================================
// PINES — ESP32 DOIT DevKit V1
// ============================================================================

// Sensores (ADC1, entrada analog. solo lectura — NO usar WiFi simultaneamente)
constexpr int PIN_F0 = 36;   // ADC1_CH0 — LDR Foto0
constexpr int PIN_F1 = 39;   // ADC1_CH3 — LDR Foto1
constexpr int PIN_F2 = 34;   // ADC1_CH6 — LDR Foto2
constexpr int PIN_F3 = 35;   // ADC1_CH7 — LDR Foto3

// Salidas de control al puente H
constexpr int PIN_PWM_AZ = 25;   // ENA  — PWM azimut   (LEDC CH0)
constexpr int PIN_PWM_EL = 26;   // ENB  — PWM elevacion (LEDC CH1)
constexpr int PIN_DIR_AZ = 27;   // IN_A — sentido azimut
constexpr int PIN_DIR_EL = 14;   // IN_B — sentido elevacion

// Debug
constexpr int PIN_STATUS = 2;    // LED integrado — heartbeat

// ============================================================================
// LEDC (PWM hardware)
// ============================================================================
constexpr uint8_t  LEDC_CH_AZ    = 0;
constexpr uint8_t  LEDC_CH_EL    = 1;
constexpr uint32_t LEDC_FREQ_HZ  = 1000;   // 1 kHz — compatible con L298/L298N
constexpr uint8_t  LEDC_RES_BITS = 10;     // 10 bits → 0..1023 (igual que AVR)
constexpr int      PWM_MAX       = (1 << LEDC_RES_BITS) - 1;  // 1023

// ============================================================================
// PARAMETROS DE CONTROL — tomados literalmente de Constantes.c (AVR)
// ============================================================================
constexpr double KP_AZ    = 5.0;
constexpr double KI_AZ    = 0.1;
constexpr double KP_EL    = 5.0;
constexpr double KI_EL    = 0.1;
constexpr double RESET_AW = 400.0;   // limite anti-windup

// Zona muerta (nuevo respecto al AVR — evita oscilacion por ruido ADC)
constexpr int DEAD_BAND = 5;   // cuentas ADC en escala 10 bits

// Periodo de control
constexpr unsigned long DT_MS = 20;   // 20 ms ≈ 50 Hz

// ============================================================================
// ESTADO DEL CONTROLADOR
// ============================================================================
static double integral_Az = 0.0;
static double integral_El = 0.0;

// ============================================================================
// Lee canal ADC, promedia N muestras y convierte 12 bits → 10 bits
// (AVR usaba ADC de 10 bits; mantener escala para reutilizar constantes PI)
// ============================================================================
static int leerADC10(int pin, int n = 8) {
    long acum = 0;
    for (int i = 0; i < n; ++i) acum += analogRead(pin);
    return static_cast<int>((acum / n) >> 2);   // /4 para escalar 12→10 bits
}

// ============================================================================
// Detiene ambos motores de forma segura
// ============================================================================
static void pararMotores() {
    ledcWrite(LEDC_CH_AZ, 0);
    ledcWrite(LEDC_CH_EL, 0);
    integral_Az = 0.0;
    integral_El = 0.0;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("============================================");
    Serial.println(" TD2 — Sun Tracker ESP32  |  boot OK");
    Serial.println(" Kp=5.0  Ki=0.1  Reset_aw=400  dt=20ms");
    Serial.println("============================================");

    // Sentido y LED
    pinMode(PIN_DIR_AZ, OUTPUT);   digitalWrite(PIN_DIR_AZ, LOW);
    pinMode(PIN_DIR_EL, OUTPUT);   digitalWrite(PIN_DIR_EL, LOW);
    pinMode(PIN_STATUS, OUTPUT);   digitalWrite(PIN_STATUS, LOW);

    // ADC: resolucion 12 bits, atenuacion 11 dB (0..3.3 V)
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_F0, ADC_11db);
    analogSetPinAttenuation(PIN_F1, ADC_11db);
    analogSetPinAttenuation(PIN_F2, ADC_11db);
    analogSetPinAttenuation(PIN_F3, ADC_11db);

    // LEDC PWM — canal azimut
    ledcSetup(LEDC_CH_AZ, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(PIN_PWM_AZ, LEDC_CH_AZ);
    ledcWrite(LEDC_CH_AZ, 0);

    // LEDC PWM — canal elevacion
    ledcSetup(LEDC_CH_EL, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(PIN_PWM_EL, LEDC_CH_EL);
    ledcWrite(LEDC_CH_EL, 0);

    Serial.println("[TD2] Entrando al loop de control...");
}

// ============================================================================
// LOOP DE CONTROL — dt fijo de DT_MS ms
// ============================================================================
void loop() {
    static unsigned long lastTick = 0;
    static uint32_t seq = 0;

    const unsigned long now = millis();
    if (now - lastTick < DT_MS) return;
    lastTick = now;
    ++seq;

    // -------------------------------------------------------------------------
    // 1. Lectura de sensores (Leo_Fotos.c portado)
    //    F0=ADC0, F1=ADC1, F2=ADC2, F3=ADC3 en escala 10 bits
    // -------------------------------------------------------------------------
    const int F0 = leerADC10(PIN_F0);
    const int F1 = leerADC10(PIN_F1);
    const int F2 = leerADC10(PIN_F2);
    const int F3 = leerADC10(PIN_F3);

    // Validacion basica de sensores (rango ADC: > 10 cuentas = sensor conectado)
    if (F0 < 10 && F1 < 10 && F2 < 10 && F3 < 10) {
        if (seq % 50 == 0)
            Serial.println("[FAULT] Sensores invalidos — motores detenidos");
        pararMotores();
        digitalWrite(PIN_STATUS, HIGH);   // LED fijo = fault
        return;
    }

    // -------------------------------------------------------------------------
    // 2. Calculo de error (formulas identicas al AVR — PID_Elev.c / PID_Azim.c)
    // -------------------------------------------------------------------------
    //   Elevacion: inferior (F2+F3) - superior (F0+F1)
    //   Azimut:    derecha  (F1+F3) - izquierda (F0+F2)
    const double Error_El = static_cast<double>(F2 + F3 - F0 - F1) / 2.0;
    const double Error_Az = static_cast<double>(F1 + F3 - F0 - F2) / 2.0;

    // -------------------------------------------------------------------------
    // 3. PI Elevacion (PID_Elev.c portado)
    //    Sentido AVR: PWM_El >= 0 → PB5=1 (set 0x20) → DIR_EL HIGH
    //                 PWM_El <  0 → PB5=0 (clr 0xDF) → DIR_EL LOW
    // -------------------------------------------------------------------------
    int  pwm_El = 0;
    bool dir_El = false;

    if (abs(Error_El) > DEAD_BAND) {
        integral_El += Error_El;
        if (integral_El >  RESET_AW) integral_El =  RESET_AW;
        if (integral_El < -RESET_AW) integral_El = -RESET_AW;

        double u_El = (Error_El * KP_EL) + (integral_El * KI_EL);
        int    raw  = static_cast<int>(u_El);

        if (raw >= 0) {
            dir_El = true;    // PORTB |= 0x20  (PB5 = 1)
        } else {
            dir_El = false;   // PORTB &= 0xDF  (PB5 = 0)
            raw = -raw;
        }
        pwm_El = constrain(raw, 0, PWM_MAX);
    } else {
        integral_El = 0.0;   // reset integrador en zona muerta
    }

    // -------------------------------------------------------------------------
    // 4. PI Azimut (PID_Azim.c portado)
    //    Sentido AVR: PWM_Az >= 0 → PB4=0 (clr 0xEF) → DIR_AZ LOW
    //                 PWM_Az <  0 → PB4=1 (set 0x10) → DIR_AZ HIGH
    // -------------------------------------------------------------------------
    int  pwm_Az = 0;
    bool dir_Az = false;

    if (abs(Error_Az) > DEAD_BAND) {
        integral_Az += Error_Az;
        if (integral_Az >  RESET_AW) integral_Az =  RESET_AW;
        if (integral_Az < -RESET_AW) integral_Az = -RESET_AW;

        double u_Az = (Error_Az * KP_AZ) + (integral_Az * KI_AZ);
        int    raw  = static_cast<int>(u_Az);

        if (raw >= 0) {
            dir_Az = false;   // PORTB &= 0xEF  (PB4 = 0)
        } else {
            dir_Az = true;    // PORTB |= 0x10  (PB4 = 1)
            raw = -raw;
        }
        pwm_Az = constrain(raw, 0, PWM_MAX);
    } else {
        integral_Az = 0.0;
    }

    // -------------------------------------------------------------------------
    // 5. Aplicar salidas al puente H
    // -------------------------------------------------------------------------
    digitalWrite(PIN_DIR_EL, dir_El ? HIGH : LOW);
    digitalWrite(PIN_DIR_AZ, dir_Az ? HIGH : LOW);
    ledcWrite(LEDC_CH_EL, pwm_El);
    ledcWrite(LEDC_CH_AZ, pwm_Az);

    // -------------------------------------------------------------------------
    // 6. Telemetria serial (cada 500 ms ≈ 25 ciclos a dt=20ms)
    // -------------------------------------------------------------------------
    if (seq % 25 == 0) {
        Serial.printf(
            "[%5lu] F0=%4d F1=%4d F2=%4d F3=%4d"
            " | eAz=%+6.1f eEl=%+6.1f"
            " | iAz=%+7.1f iEl=%+7.1f"
            " | pwmAz=%4d(%d) pwmEl=%4d(%d)\n",
            seq, F0, F1, F2, F3,
            Error_Az, Error_El,
            integral_Az, integral_El,
            pwm_Az, static_cast<int>(dir_Az),
            pwm_El, static_cast<int>(dir_El)
        );
    }

    // Heartbeat LED
    digitalWrite(PIN_STATUS, (seq % 50 < 25) ? HIGH : LOW);
}
