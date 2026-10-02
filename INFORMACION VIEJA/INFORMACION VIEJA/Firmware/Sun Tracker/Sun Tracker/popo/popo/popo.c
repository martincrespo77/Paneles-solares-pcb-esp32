/************************************************************************************************/
/* CONTROL DE ELEVACIÓN Y AZIMUTH SUN TRACKER                                                   */
/* ------------------------------------------                                                   */
/* 28/01/2021                                                                                   */
/* Se actualiza el compilador a Atmel Studio 6.2 y el micro es ATMEL 328, compatible a 168.     */
/* Se sensan los movimientos de elevación y azimuth mediante LDRs.                              */
/* Foto 0: AIN0                                                                                 */
/* Foto 1: AIN1                                                                                 */
/* Foto 2: AIN2                                                                                 */
/* Foto 3: AIN3                                                                                 */
/* Movimiento Elevación: Prom foto0-1 - Prom Foto2-3                                            */
/* Movimiento Azimuth: Prom foto0-2 - Prom Foto1-3                                              */
/* El control de los motores es mediante PI, no es necesario PD porque el seguimiento es lento  */
/* Este programa está adaptado para el Sun Tracker con panel solar de SOLARTEC.                 */
/************************************************************************************************/

#include <avr/io.h>
#include <avr/signal.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <math.h>
#include <avr/wdt.h>
#include "Def.h"
#include "PORT.c"
#include "ADC.c"
#include "USART.c"
#include "PWM.c"
#include "Leo_ADC.c"

/*************************************************************************************************/

int main(void)
{
	#include "Constantes.c"
	PORT_Init();
	ADC_Init();
	USART_Init();
	Fast_Pwm_Init();
		
	while(1)
	{
 	 // Subo bandera
	 PORTB |= 0x01;
	 // lee los 4 fototransistores
	 #include "Leo_fotos.c"
	 // Bajo bandera
	 PORTB &= 0xfe;
	 //-----------------------------------------------------
	 #include "PID_Elev.c"
	 #include "PID_Azim.c"
	}
}

//----------------------------------------------------------------------------------------


