/************************************************************/
void Fast_Pwm_Init()
{        
    /* TCCR1A
	   CM1A1 = 1; CM1A0 = 0.
	   Set OC1A at bottom.
	   Clear OC1A on compare match.
       CM1B1 = 1; CM1B0 = 0.
	   Set OC1B at bottom.
	   Clear OC1B on compare match.
	   FOC1A = FOC1B = 0 in PWM Mode.
	   WGM11 = WGM10 = 1
	   Fast PWM 10 bits.
     */
	 TCCR1A = 0xa3;

     /* TCCR1B
	    ICN1 = 0 no input capture noise canceler.
		ICES1 = 0 no input capture edge select.
		WGM13 = 0; WGM12= 0
		Fast PWM 10 bits.
		CS12 = 1; CS11 = CS10 = 0 
		prescaling clk/1024.
     */
	 TCCR1B = 0x0a; //1/8
    
     /* para fast pwm f_fast_pwm = 16MHz/(N_prescaler*N_pwm_10bits)
        f_fast_pwm = 16MHz/(8 * 1024) = 1953.125.141Hz  
	  */ 

	 /* Hago PB2 = OC1B y PB1 = OC1A para que sean de salida*/
	 /* Hago también que PB4 y PB5 sean de salida           */
	 /* Uso PB0 como flag para medir tiempos                */
     DDRB = 0x37; 

	 /* OCF1A = OCF1B = 1 los limpio */
	 TIFR1 |= 0x18;
}
/************************************************************/
