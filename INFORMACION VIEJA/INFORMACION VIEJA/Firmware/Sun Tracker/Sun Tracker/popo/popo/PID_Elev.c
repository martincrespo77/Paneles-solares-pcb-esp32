      // PID
	  // ---       
      //-----------------------------------------------------------
      /* Determino control para movimiento elevación */

      // Valor absoluto
      Error_El = (adc_dato_2 + adc_dato_3 - adc_dato_0 - adc_dato_1) / 2;
		             
	  // Parte integrativa
	  Integral_El = Error_El + Integral_El;

	  // Reset anti wind-up
	  if(Integral_El >= Reset_aw)
 	     Integral_El = Reset_aw;

	  if(Integral_El <= -Reset_aw)
	     Integral_El = -Reset_aw;
			
	  // Ley de control
	  PID_El = ( Error_El * Kp_El) + (Integral_El * Ki_El);
	  PWM_El = (int)(PID_El);
			
	  // Determino sentido
	  if(PWM_El >= 0)
	     // avanza paun lao
	   	 // Ley_Eld es +
	     PORTB |= 0x20;			
  	  else
	  	{
		 // avanza pal otro
	     PORTB &= 0xdf;				 
	     // Ley_Eld es -
		 PWM_El = -1 * PWM_El;
		}

	  // Limito el máximo y mínimo ley Eld
	  if(PWM_El >= 1023)
	  	 PWM_El = 1023;
			
	  if(PWM_El <= 0)
	     PWM_El = 0;
			
	  // Waiting when an OCF1B completes.
	  while( !(TIFR1 & (1<<OCF1B)) )
	            ;
	  /* Saco el PWM para el motor Eltch */
	  OCR1B = PWM_El;
   
      /* OCF1A = OCF1B = 1 los limElo */
      TIFR1 |= 0x18;
		   				
