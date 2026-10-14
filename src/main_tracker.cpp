/**
 * @file    main_tracker.cpp
 * @brief   Sun Tracker ESP32 — Tecnicas Digitales 2
 * @details Porta el controlador PI del firmware AVR (ATmega168/328, popo.c) a
 *          ESP32 DOIT DevKit V1 con PlatformIO/Arduino. Pines y calibracion en
 *          include/config.h.
 *
 * Fuente original: docs/legacy/firmware_avr/popo/{PID_Azim.c, PID_Elev.c,
 *                  Leo_Fotos.c, Constantes.c, PWM.c, PORT.c}
 *
 * Mapeo AVR -> ESP32
 * ------------------
 *   ADC0..ADC3 (FOTO0..3) -> GPIO36, GPIO39, GPIO34, GPIO35 (ADC1)
 *   OC1A / PB1  -> GPIO25   (LEDC CH0 — PWM azimut)
 *   OC1B / PB2  -> GPIO26   (LEDC CH1 — PWM elevacion)
 *   PB4         -> GPIO27   (sentido azimut)
 *   PB5         -> GPIO14   (sentido elevacion)
 *
 * Arquitectura: loop() no bloqueante, planificado con millis() a Ts fijo.
 * Estados: TRACKING (lazo PI activo), NIGHT (oscuridad, motores parados),
 *          SENSOR_FAULT (LDR desconectado, motores parados).
 */

#include <Arduino.h>
#include "config.h"

// ============================================================================
// PWM — capa de compatibilidad Arduino-ESP32 2.x / 3.x
// ============================================================================
// El core 3.x elimino ledcSetup()/ledcAttachPin() y ledcWrite() pasa a
// recibir el pin. Asi el firmware compila con ambos.
static void pwmInit(uint8_t pin, uint8_t ch) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(pin, LEDC_FREQ_HZ, LEDC_RES_BITS, ch);
#else
    ledcSetup(ch, LEDC_FREQ_HZ, LEDC_RES_BITS);
    ledcAttachPin(pin, ch);
#endif
}

static void pwmWrite(uint8_t pin, uint8_t ch, uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    (void)ch;
    ledcWrite(pin, duty);
#else
    (void)pin;
    ledcWrite(ch, duty);
#endif
}

// ============================================================================
// EJE (azimut / elevacion)
// ============================================================================
struct Axis {
    float       kp;
    float       ki;
    uint8_t     pinPwm;
    uint8_t     pinDir;
    uint8_t     ledcCh;
    uint8_t     dirPositiveLevel;
    uint8_t     pinLimPos;
    uint8_t     pinLimNeg;

    // Estado
    float    integral;
    int8_t   dirSign;       // sentido cargado en el pin DIR: +1, -1, 0 (ninguno aun)
    uint32_t lastDriveMs;   // ultimo instante con PWM > 0
    int      pwm;           // ultima salida aplicada (telemetria)
};

static Axis axisAz = {KP_AZ, KI_AZ, PIN_PWM_AZ, PIN_DIR_AZ, LEDC_CH_AZ,
                      DIR_AZ_POSITIVE_LEVEL, PIN_LIM_AZ_POS, PIN_LIM_AZ_NEG,
                      0.0f, 0, 0, 0};
static Axis axisEl = {KP_EL, KI_EL, PIN_PWM_EL, PIN_DIR_EL, LEDC_CH_EL,
                      DIR_EL_POSITIVE_LEVEL, PIN_LIM_EL_POS, PIN_LIM_EL_NEG,
                      0.0f, 0, 0, 0};

/** Carrera activada en el sentido pedido (siempre false si no hay finales). */
static bool limitActive(const Axis& ax, int8_t sign) {
#if USE_LIMIT_SWITCHES
    return digitalRead(sign > 0 ? ax.pinLimPos : ax.pinLimNeg) == HIGH;
#else
    (void)ax;
    (void)sign;
    return false;
#endif
}

/** Motor detenido e integrador en cero. No toca DIR. */
static void stopAxis(Axis& ax) {
    pwmWrite(ax.pinPwm, ax.ledcCh, 0);
    ax.pwm      = 0;
    ax.integral = 0.0f;
}

/**
 * Un paso del PI (PID_Azim.c / PID_Elev.c) + etapa de salida.
 *
 *   |e| <= DEAD_BAND → parado, I = 0
 *   I = sat(I + e, ±RESET_AW)
 *   u = Kp·e + Ki·I ;  PWM = min(|trunc(u)|, PWM_MAX) ;  sentido = signo(u)
 *
 * No hay riesgo de overflow: |e| <= 1023, |I| <= 400 → |u| <= 5155 en float.
 */
static void updateAxis(Axis& ax, float error, uint32_t now) {
    if (fabsf(error) <= DEAD_BAND) {
        stopAxis(ax);
        return;
    }

    ax.integral = constrain(ax.integral + error, -RESET_AW, RESET_AW);
    const float u = ax.kp * error + ax.ki * ax.integral;

    // Truncado a entero igual que el (int) del AVR; u >= 0 → sentido positivo.
    const int    raw  = static_cast<int>(u);
    const int8_t sign = (raw >= 0) ? 1 : -1;
    int          duty = min(abs(raw), PWM_MAX);

    // Final de carrera: no empujar contra el tope ni acumular integral.
    if (limitActive(ax, sign)) {
        stopAxis(ax);
        return;
    }

    // Inversion de giro: esperar con PWM = 0 para no invertir un motor en
    // marcha (pico de corriente en el puente H).
    if (duty > 0 && sign != ax.dirSign) {
        if (ax.dirSign != 0 && now - ax.lastDriveMs < DIR_CHANGE_DEADTIME_MS) {
            duty = 0;
        } else {
            const uint8_t neg = (ax.dirPositiveLevel == HIGH) ? LOW : HIGH;
            digitalWrite(ax.pinDir, sign > 0 ? ax.dirPositiveLevel : neg);
            ax.dirSign = sign;
        }
    }

    pwmWrite(ax.pinPwm, ax.ledcCh, static_cast<uint32_t>(duty));
    ax.pwm = duty;
    if (duty > 0) ax.lastDriveMs = now;
}

// ============================================================================
// SENSORES
// ============================================================================
/** Promedia ADC_SAMPLES lecturas calibradas (mV) y reescala a 0..1023. */
static int readLdr(uint8_t pin) {
    uint32_t mv = 0;
    for (uint8_t i = 0; i < ADC_SAMPLES; ++i) mv += analogReadMilliVolts(pin);
    mv /= ADC_SAMPLES;
    const uint32_t counts = (mv * ADC_MAX_COUNT + ADC_FULL_MV / 2) / ADC_FULL_MV;
    return static_cast<int>(min<uint32_t>(counts, ADC_MAX_COUNT));
}

// ============================================================================
// MAQUINA DE ESTADOS DE SEGURIDAD
// ============================================================================
enum class State : uint8_t { TRACKING, NIGHT, SENSOR_FAULT };

static const char* stateName(State s) {
    switch (s) {
        case State::TRACKING:     return "TRACK";
        case State::NIGHT:        return "NIGHT";
        case State::SENSOR_FAULT: return "FAULT";
    }
    return "?";
}

/** Condicion que debe sostenerse `holdMs` seguidos para confirmarse. */
struct Confirm {
    bool     pending = false;
    uint32_t since   = 0;

    bool update(bool cond, uint32_t now, uint32_t holdMs) {
        if (!cond) {
            pending = false;
            return false;
        }
        if (!pending) {
            pending = true;
            since   = now;
        }
        return now - since >= holdMs;
    }
    void reset() { pending = false; }
};

static State   state = State::TRACKING;
static Confirm nightEnter, nightExit, faultEnter, faultClear;

static void setState(State next) {
    if (next == state) return;
    Serial.printf("[STATE] %s -> %s\n", stateName(state), stateName(next));
    state = next;
    nightEnter.reset();
    nightExit.reset();
    faultEnter.reset();
    faultClear.reset();
    if (state != State::TRACKING) {
        stopAxis(axisAz);
        stopAxis(axisEl);
    }
}

static void updateState(int avg, int minF, uint32_t now) {
    switch (state) {
        case State::TRACKING:
            if (nightEnter.update(avg < NIGHT_ENTER_LEVEL, now, NIGHT_CONFIRM_MS)) {
                setState(State::NIGHT);
            } else if (faultEnter.update(avg >= NIGHT_ENTER_LEVEL && minF < SENSOR_MIN_VALID,
                                         now, SENSOR_FAULT_CONFIRM_MS)) {
                setState(State::SENSOR_FAULT);
            }
            break;
        case State::NIGHT:
            if (nightExit.update(avg > NIGHT_EXIT_LEVEL, now, NIGHT_CONFIRM_MS))
                setState(State::TRACKING);
            break;
        case State::SENSOR_FAULT:
            if (faultClear.update(minF >= SENSOR_MIN_VALID, now, SENSOR_FAULT_CLEAR_MS))
                setState(State::TRACKING);
            break;
    }
}

/** TRACK: parpadeo 1 Hz | NIGHT: destello corto | FAULT: fijo. */
static void updateStatusLed(uint32_t now) {
    const uint32_t phase = now % HEARTBEAT_PERIOD_MS;
    bool on = false;
    switch (state) {
        case State::TRACKING:     on = phase < HEARTBEAT_PERIOD_MS / 2;  break;
        case State::NIGHT:        on = phase < HEARTBEAT_PERIOD_MS / 20; break;
        case State::SENSOR_FAULT: on = true;                             break;
    }
    digitalWrite(PIN_STATUS, on ? HIGH : LOW);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // Salidas de potencia primero: motores parados lo antes posible.
    pwmInit(PIN_PWM_AZ, LEDC_CH_AZ);
    pwmInit(PIN_PWM_EL, LEDC_CH_EL);
    pwmWrite(PIN_PWM_AZ, LEDC_CH_AZ, 0);
    pwmWrite(PIN_PWM_EL, LEDC_CH_EL, 0);

    pinMode(PIN_DIR_AZ, OUTPUT);  digitalWrite(PIN_DIR_AZ, LOW);
    pinMode(PIN_DIR_EL, OUTPUT);  digitalWrite(PIN_DIR_EL, LOW);
    pinMode(PIN_STATUS, OUTPUT);  digitalWrite(PIN_STATUS, LOW);

#if USE_LIMIT_SWITCHES
    pinMode(PIN_LIM_AZ_POS, INPUT_PULLUP);
    pinMode(PIN_LIM_AZ_NEG, INPUT_PULLUP);
    pinMode(PIN_LIM_EL_POS, INPUT_PULLUP);
    pinMode(PIN_LIM_EL_NEG, INPUT_PULLUP);
#endif

    // ADC: 12 bits, atenuacion 11 dB (rango util ~0.15..2.45 V lineal)
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_F0, ADC_11db);
    analogSetPinAttenuation(PIN_F1, ADC_11db);
    analogSetPinAttenuation(PIN_F2, ADC_11db);
    analogSetPinAttenuation(PIN_F3, ADC_11db);

    // Buffer TX para que la telemetria no bloquee el lazo (debe ir antes de begin)
    Serial.setTxBufferSize(1024);
    Serial.begin(115200);
    Serial.println();
    Serial.println("============================================");
    Serial.println(" TD2 - Sun Tracker ESP32  |  boot OK");
    Serial.printf(" Kp=%.2f Ki=%.2f Reset_aw=%.0f DB=%.0f Ts=%lums\n",
                  KP_AZ, KI_AZ, RESET_AW, DEAD_BAND,
                  static_cast<unsigned long>(CONTROL_PERIOD_MS));
    Serial.printf(" Finales de carrera: %s\n", USE_LIMIT_SWITCHES ? "SI" : "NO");
    Serial.println("============================================");

#if USE_LOOP_WDT
    enableLoopWDT();   // se alimenta automaticamente al terminar cada loop()
#endif
}

// ============================================================================
// LOOP — planificador no bloqueante a Ts = CONTROL_PERIOD_MS
// ============================================================================
void loop() {
    static uint32_t lastTick      = 0;
    static uint32_t lastTelemetry = 0;
    static uint32_t overruns      = 0;

    const uint32_t now = millis();
    updateStatusLed(now);

    if (now - lastTick < CONTROL_PERIOD_MS) return;
    lastTick += CONTROL_PERIOD_MS;               // sin deriva acumulada
    if (now - lastTick >= CONTROL_PERIOD_MS) {   // atraso > 1 periodo: resincronizar
        lastTick = now;
        ++overruns;
    }

    // 1. Sensores (Leo_Fotos.c) — escala 10 bits
    const int F0 = readLdr(PIN_F0);
    const int F1 = readLdr(PIN_F1);
    const int F2 = readLdr(PIN_F2);
    const int F3 = readLdr(PIN_F3);

    const int avg  = (F0 + F1 + F2 + F3) / 4;
    const int minF = min(min(F0, F1), min(F2, F3));

    // 2. Supervision (noche / sensor desconectado)
    updateState(avg, minF, now);

    // 3. Error (PID_Elev.c / PID_Azim.c). Division en float; el AVR truncaba.
    //    Elevacion: inferior (F2+F3) - superior (F0+F1)
    //    Azimut:    derecha  (F1+F3) - izquierda (F0+F2)
    const float errEl = static_cast<float>(F2 + F3 - F0 - F1) / 2.0f;
    const float errAz = static_cast<float>(F1 + F3 - F0 - F2) / 2.0f;

    // 4. Lazo PI + salida al puente H
    if (state == State::TRACKING) {
        updateAxis(axisEl, errEl, now);
        updateAxis(axisAz, errAz, now);
    }

    // 5. Telemetria
    if (now - lastTelemetry >= TELEMETRY_PERIOD_MS) {
        lastTelemetry = now;
        Serial.printf(
            "[%8lu] %s F=%4d %4d %4d %4d | eAz=%+6.1f eEl=%+6.1f"
            " | iAz=%+6.1f iEl=%+6.1f | pwmAz=%c%4d pwmEl=%c%4d | ovr=%lu\n",
            static_cast<unsigned long>(now), stateName(state), F0, F1, F2, F3,
            errAz, errEl, axisAz.integral, axisEl.integral,
            axisAz.dirSign < 0 ? '-' : '+', axisAz.pwm,
            axisEl.dirSign < 0 ? '-' : '+', axisEl.pwm,
            static_cast<unsigned long>(overruns));
    }
}
