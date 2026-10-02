# Contexto tecnico del proyecto Sun Tracker y migracion a ESP32

Fecha de analisis: 2026-05-22

Este documento resume lo entendido del proyecto actual, del firmware viejo, del circuito general del informe y del objetivo de migracion. No es todavia un plan de implementacion. Sirve para validar si estamos hablando del mismo problema antes de pedir un plan detallado.

## 1. Resumen corto

El repositorio contiene dos lineas de trabajo relacionadas:

1. **Sun Tracker viejo**: sistema de seguimiento solar de dos ejes, azimut y elevacion, basado en un microcontrolador AVR ATmega168/328, cuatro sensores LDR, control PI/PID discreto y un puente H L298 para mover dos motores DC con reductor.
2. **Trabajo nuevo con ESP32**: primer material para usar ESP32 con MicroPython, medicion/telemetria de variables solares y transmision optica por laser. Ese codigo nuevo todavia no controla motores.

El objetivo actual es conservar la logica funcional del Sun Tracker viejo, migrarla a una **ESP32 programada en MicroPython/Python**, y mover los motores mediante un **puente H**. El enfoque correcto es portar primero el control de seguimiento y despues integrarlo con la telemetria optica, no mezclar todo desde el primer paso.

## 2. Archivos relevantes encontrados

### Material viejo

- `INFORMACION VIEJA-20260522T124344Z-3-001/INFORMACION VIEJA/UNDEFI 263 - Informe Final.pdf`
  - Informe tecnico del Sun Tracker.
  - Explica objetivo, mecanica, sensores, motores, puente H, PID discreto y circuito.

- `INFORMACION VIEJA-20260522T124344Z-3-001/INFORMACION VIEJA/Circuito Eléctrico/Sun_seeker.PDF`
  - Esquematico electronico general.
  - Incluye ATmega168/328, L298HN, CD4053, LM7805, sensores LDR y conectores de motores.

- `INFORMACION VIEJA-20260522T124344Z-3-001/INFORMACION VIEJA/Firmware/Sun Tracker/Sun Tracker/popo/popo/`
  - Firmware en C para AVR.
  - Archivos principales: `popo.c`, `ADC.c`, `Leo_Fotos.c`, `PWM.c`, `PID_Elev.c`, `PID_Azim.c`, `PORT.c`, `Constantes.c`, `USART.c`, `Def.h`.

### Material nuevo

- `Info general.md`
  - Define un sistema de telemetria solar por enlace optico FSO usando dos ESP32.
  - Usa sensor de energia, laser, fototransistor/comparador y tramas JSON con CRC.

- `Código de Ejecución en MicroPython (Nodo Transmisor).py`
  - Script MicroPython inicial para ESP32.
  - Lee valores simulados de energia, arma JSON, calcula CRC32 y transmite por UART hacia laser.
  - No controla LDR ni motores.

- `Diagrama de flujo.txt`
  - Flujo del nodo transmisor de telemetria: inicializar perifericos, leer sensor, acumular potencia, generar historico, armar JSON, calcular CRC y transmitir.

- `Proyecto FINAL Optoelectronica/`
  - Carpeta encontrada vacia.

- `Proyecto FINAL Tecnicas digitales 2/`
  - Carpeta encontrada vacia.

## 3. Que hace el proyecto viejo

El proyecto viejo es un **seguidor solar de doble eje**. Su objetivo es orientar un panel fotovoltaico hacia el Sol para mejorar la captacion de energia.

Segun el informe, el sistema tiene:

- Panel solar fotovoltaico Solartec KS45TA de 45 Wp.
- Movimiento en dos ejes:
  - **Azimut**.
  - **Elevacion**.
- Dos motores de corriente continua con reductor, modelo IGNIS MR08D-012004-4.0, con motor Mabuchi RS-385PH-2085.
- Sensado de luz mediante cuatro LDR ubicados en un discriminador de sombra en forma de cruz.
- Control digital ejecutado en un microcontrolador AVR.
- Accionamiento de motores mediante puente H L298.

La idea de control es tradicional y correcta para este tipo de prototipo: comparar luz recibida en cuatro cuadrantes, calcular error por eje, y mover cada motor hasta minimizar el error.

## 4. Sensores y calculo de error

El sensor optico viejo usa cuatro LDR:

- `FOTO0`
- `FOTO1`
- `FOTO2`
- `FOTO3`

Estan separados por una barrera horizontal y una barrera vertical. Esa cruz genera sombras cuando el panel no esta orientado correctamente.

El firmware viejo lee los cuatro sensores por ADC:

- `ADC0 -> FOTO0`
- `ADC1 -> FOTO1`
- `ADC2 -> FOTO2`
- `ADC3 -> FOTO3`

La logica de error implementada en C es:

```c
Error_El = (adc_dato_2 + adc_dato_3 - adc_dato_0 - adc_dato_1) / 2;
Error_Az = (adc_dato_1 + adc_dato_3 - adc_dato_0 - adc_dato_2) / 2;
```

Interpretacion:

- Elevacion compara el promedio inferior/superior, segun como esten montados los LDR.
- Azimut compara el promedio derecho/izquierdo.
- Si el error es positivo o negativo, cambia el sentido del motor.
- Si el error tiende a cero, el sistema esta alineado.

Esto hay que conservar en la migracion. Si se invierte fisicamente un motor o se cambia el orden de LDR, no se cambia la teoria: se ajusta el signo o el mapeo.

## 5. Circuito viejo segun informe y esquematico

El circuito viejo se organiza asi:

```text
4 LDR -> ADC0..ADC3 del ATmega -> calculo de error -> PI/PID discreto
      -> PWM + bits de sentido -> CD4053 / L298 -> motores DC de azimut y elevacion
```

Componentes principales:

- **U1: ATmega168/328**
  - El informe habla de ATmega328.
  - El proyecto Atmel Studio figura configurado como ATmega168.
  - El comentario del firmware dice que el codigo es compatible con ATmega328 y ATmega168.

- **U2: L298HN**
  - Puente H doble.
  - Maneja dos motores DC.
  - Un puente para azimut.
  - Otro puente para elevacion.

- **U3: CD4053BCN**
  - Multiplexor/demultiplexor analogico usado para direccionar senales de control hacia el L298.

- **U4: LM7805**
  - Regulador lineal para alimentar la electronica de control a 5 V.

- **D1 a D8: FR107**
  - Diodos rapidos de rueda libre para proteger frente a energia inductiva de los motores.

- **Y1: cristal de 16 MHz**
  - El esquematico muestra 16 MHz.

- **R2, R3, R4, R5: 4k7**
  - Resistencias asociadas a los sensores LDR.

- **P1**
  - Motor de acimut.

- **P2**
  - Motor de elevacion.

- **P3**
  - Conector de sensores: `FOTO0`, `FOTO1`, `FOTO2`, `FOTO3` y alimentacion.

- **P4**
  - Entrada de alimentacion rotulada como `9Vcc` en el esquematico.
  - El informe menciona bateria pequena de `12Vcc`. Este punto hay que verificar fisicamente.

## 6. Pines y senales del firmware viejo

El firmware usa principalmente el puerto B del AVR:

| Funcion | AVR | Uso |
|---|---:|---|
| `PB0` | salida | bandera/debug para medir tiempos |
| `PB1 / OC1A` | salida PWM | PWM de azimut (`PWMA`) |
| `PB2 / OC1B` | salida PWM | PWM de elevacion (`PWME`) |
| `PB4` | salida digital | sentido de azimut (`SENTA`) |
| `PB5` | salida digital | sentido de elevacion (`SENTE`) |

El Timer1 se configura en Fast PWM de 10 bits. Por eso el PWM viejo trabaja en rango:

```text
0..1023
```

El codigo limita cada salida PWM a ese rango antes de escribir:

```c
OCR1A = PWM_Az; // azimut
OCR1B = PWM_El; // elevacion
```

## 7. Firmware viejo en C

El punto de entrada es `popo.c`.

Flujo principal:

```text
Inicializar constantes
Inicializar puertos
Inicializar ADC
Inicializar USART
Inicializar PWM

while(1):
    levantar bandera PB0
    leer FOTO0..FOTO3
    bajar bandera PB0
    ejecutar control de elevacion
    ejecutar control de azimut
```

Modulos:

- `PORT.c`
  - Configura pines del puerto B como salida.
  - El puerto D queda como entrada.

- `ADC.c`
  - Configura ADC con referencia externa AREF.
  - Modo single conversion.
  - Resultado de 10 bits.

- `Leo_Fotos.c`
  - Lee secuencialmente ADC0, ADC1, ADC2 y ADC3.
  - Arma valores de 10 bits en `adc_dato_0` a `adc_dato_3`.

- `PWM.c`
  - Configura Timer1 en Fast PWM de 10 bits.
  - Usa salidas OC1A y OC1B.

- `PID_Elev.c`
  - Calcula error de elevacion.
  - Integra el error.
  - Aplica anti-windup.
  - Calcula PWM.
  - Define sentido por `PB5`.
  - Escribe `OCR1B`.

- `PID_Azim.c`
  - Calcula error de azimut.
  - Integra el error.
  - Aplica anti-windup.
  - Calcula PWM.
  - Define sentido por `PB4`.
  - Escribe `OCR1A`.

- `Constantes.c`
  - Inicializa ganancias y variables.

Constantes usadas por el firmware C:

```c
Kp_El = 5.0;
Ki_El = 0.1;
Kd_El = 0.0;

Kp_Az = 5.0;
Ki_Az = 0.1;
Kd_Az = 0.0;

Reset_aw = 400.0;
```

Punto importante: aunque los archivos se llaman `PID`, el codigo activo implementa en la practica un **PI**, porque no usa el termino derivativo. `Kd` existe como variable, pero no participa en la ley de control.

## 8. Diferencias y dudas tecnicas detectadas

Hay varias diferencias que no conviene ignorar:

1. **ATmega168 vs ATmega328**
   - El informe habla de ATmega328.
   - El esquematico dice ATmega168/328.
   - El `.cproj` esta configurado para ATmega168.
   - El comentario de `popo.c` habla de ATmel 328 compatible con 168.
   - Para la migracion no afecta demasiado, pero para documentar el original hay que decirlo como esta: familia ATmega168/328.

2. **9 Vcc vs 12 Vcc**
   - El esquematico rotula una entrada como `9Vcc`.
   - El informe habla de bateria de `12Vcc`.
   - El L298 y los motores se modelan con 12 V.
   - Antes de cablear la ESP32 hay que medir o confirmar la alimentacion real.

3. **Cristal y USART**
   - El esquematico muestra cristal de 16 MHz.
   - `USART.c` comenta "57600 baud with 20 MHz osc" y usa `UBRR0L = 0x15`.
   - Si el reloj real es 16 MHz, ese valor no da 57600 exactos en modo normal.
   - La USART parece secundaria o de depuracion, no es central para el control.

4. **PID teorico vs PI implementado**
   - El informe desarrolla PID discreto.
   - El codigo final usa Kd = 0 y no calcula derivada.
   - Para migrar, lo conservador es portar primero el PI real que funcionaba.

5. **L298**
   - Es tradicional, robusto y aparece en el circuito.
   - Tambien es viejo e ineficiente frente a drivers modernos.
   - Si el objetivo academico es respetar el informe, se puede usar L298.
   - Si el objetivo es eficiencia electrica, convendria evaluar otro puente H, pero eso ya es decision de diseno.

## 9. Codigo nuevo en MicroPython

El archivo `Código de Ejecución en MicroPython (Nodo Transmisor).py` es un primer nodo de telemetria.

Hace esto:

1. Configura I2C para un sensor de energia.
2. Configura UART2 para transmitir por laser.
3. Simula lectura de voltaje, corriente y potencia.
4. Acumula potencia.
5. Mantiene un historico de 24 bloques horarios.
6. Arma un JSON.
7. Calcula CRC32.
8. Transmite una trama por UART:

```text
[JSON|CRC32]\n
```

Ese codigo sirve para el proyecto de optoelectronica/telemetria. No reemplaza todavia al firmware del Sun Tracker.

## 10. Objetivo de migracion a ESP32

La migracion principal deberia apuntar a esto:

```text
4 LDR -> ADC de ESP32 -> control PI/PID en MicroPython
      -> PWM + direccion -> puente H -> motores de azimut/elevacion
```

La ESP32 reemplaza al ATmega como unidad de control. El puente H reemplaza o conserva la etapa de potencia. Los motores no se conectan jamas directo a la ESP32.

Aspectos minimos de la migracion:

- Leer cuatro entradas analogicas de la ESP32.
- Tener en cuenta que el ADC de ESP32 no se comporta igual que el ADC AVR:
  - AVR: 10 bits, referencia controlada.
  - ESP32: normalmente 12 bits, no lineal, requiere atenuacion y calibracion practica.
- Portar las ecuaciones de error.
- Portar la ley PI actual:
  - proporcional
  - integral
  - anti-windup
  - saturacion de PWM
  - seleccion de sentido
- Generar PWM con `machine.PWM`.
- Controlar pines digitales de direccion.
- Definir una zona muerta para evitar que el sistema tiemble cuando el error sea pequeno.
- Agregar limites de seguridad:
  - fin de carrera, si existen
  - corte por corriente, si el driver lo permite
  - timeout de movimiento
  - parada por sensores invalidos
  - parada manual o modo seguro

## 11. Propuesta de separacion entre ambos proyectos

Para no ensuciar el diseno, conviene separar los proyectos asi:

### Proyecto A: Tecnicas Digitales 2

Tema central:

```text
Migracion del Sun Tracker viejo a ESP32 con MicroPython y puente H.
```

Responsabilidades:

- Lectura de LDR.
- Control de orientacion.
- Movimiento de dos motores.
- Driver de puente H.
- Seguridad electrica y mecanica.
- Validacion con pruebas de banco.

Este proyecto debe nacer del firmware AVR viejo, porque ahi esta la logica de control que ya funcionaba.

### Proyecto B: Optoelectronica

Tema central:

```text
Telemetria optica de variables solares entre dos ESP32 usando laser.
```

Responsabilidades:

- Medicion de voltaje, corriente y potencia.
- Construccion de tramas.
- CRC.
- Transmision por laser.
- Recepcion por fototransistor/comparador.
- Validacion del enlace optico.

Este proyecto debe nacer del archivo MicroPython actual y de `Info general.md`.

### Relacion entre ambos

Ambos proyectos se conectan porque usan el mismo sistema fisico solar:

- El Sun Tracker orienta el panel.
- La telemetria mide o transmite informacion del panel.
- Ambos pueden usar ESP32.
- Ambos pueden compartir alimentacion y gabinete, pero no conviene que compartan responsabilidades al comienzo.

Lo correcto es que primero funcionen por separado:

1. Control de motores estable.
2. Telemetria optica estable.
3. Integracion final con interfaces claras.

## 12. Como iniciar el trabajo

### Inicio recomendado para el Proyecto A

1. Levantar una tabla exacta de pines ESP32.
2. Decidir puente H:
   - L298 si se respeta el circuito historico.
   - Otro driver si se prioriza eficiencia.
3. Medir tension real de motores y corriente maxima.
4. Cablear una sola etapa de motor en banco, sin el panel.
5. Probar PWM y direccion manual.
6. Leer los cuatro LDR en MicroPython.
7. Verificar que el orden de sensores coincida con `FOTO0..FOTO3`.
8. Portar el calculo de error.
9. Portar PI de un eje.
10. Repetir para el segundo eje.
11. Agregar zona muerta, limites y parada segura.
12. Montar en la mecanica real.

### Inicio recomendado para el Proyecto B

1. Definir si se mide lado DC del panel o lado AC del inversor.
2. Elegir sensor:
   - INA226 para DC.
   - PZEM-004T para AC.
3. Reemplazar los valores simulados del script MicroPython por lectura real.
4. Probar UART local por cable antes del laser.
5. Probar laser con distancia corta.
6. Validar recepcion y CRC.
7. Agregar reintentos o descarte de tramas corruptas.
8. Registrar historico de datos.

## 13. Riesgos tecnicos

- La ESP32 trabaja a 3.3 V logicos. Si se reutiliza L298/CD4053 alimentado a 5 V, hay que verificar compatibilidad de niveles.
- El ADC de ESP32 necesita calibracion. No hay que asumir que leer `0..4095` equivale linealmente a luz util.
- Los motores generan ruido electrico. Hay que separar masa de potencia y logica con criterio, usar capacitores y cableado corto.
- El L298 disipa potencia. Puede calentar si la corriente del motor se acerca al limite.
- Sin finales de carrera se puede forzar la mecanica.
- MicroPython no es tiempo real duro. Para este sistema puede alcanzar porque el seguimiento solar es lento, pero los callbacks de timer no deben hacer calculos pesados ni asignar memoria.
- Si se integra laser y motores en la misma ESP32, el ruido de motores puede degradar la comunicacion optica.

## 14. Decision tecnica inicial

Mi lectura conservadora es esta:

1. No hay que empezar escribiendo un sistema nuevo desde cero.
2. Hay que portar primero el comportamiento real del AVR:
   - cuatro ADC
   - dos errores
   - PI por eje
   - PWM absoluto
   - bit de direccion
3. El puente H debe tratarse como etapa de potencia separada.
4. La telemetria FSO debe quedar como segundo modulo, no mezclada con el control de movimiento.
5. Las carpetas `Proyecto FINAL Optoelectronica` y `Proyecto FINAL Tecnicas digitales 2` deberian usarse para separar formalmente ambos entregables.

## 15. Pendientes para confirmar antes del plan detallado

Antes de pedir un plan completo a Claude o a cualquier agente de planificacion, hay que confirmar:

1. Que puente H se usara realmente: L298/L298N, BTS7960, TB6612FNG u otro.
2. Tension real de motores: 9 V, 12 V u otra.
3. Corriente de arranque o bloqueo de cada motor.
4. Modelo exacto de ESP32.
5. Si se mantendran los cuatro LDR originales.
6. Si existen finales de carrera fisicos.
7. Si el proyecto final exige MicroPython si o si, o si se acepta C/C++ con ESP-IDF/Arduino.
8. Si la telemetria optica va en la misma ESP32 que controla motores o en otra ESP32 separada.
