      // PID
	  // ---  
      /* Determino control para movimiento azimuth */
      Error_Az = (adc_dato_1 + adc_dato_3 - adc_dato_0 - adc_dato_2) / 2;					 
					 
	  // Parte integrativa
	  Integral_Az = Error_Az + Integral_Az;

	  // Reset anti wind-up
	  if(Integral_Az >= Reset_aw)
 	     Integral_Az = Reset_aw;

	  if(Integral_Az <= -Reset_aw)
	     Integral_Az = -Reset_aw;
			
	  // Ley de contAzl
	  PID_Az = (Error_Az * Kp_Az) + (Integral_Az * Ki_Az);
	  PWM_Az = (int)(PID_Az);
			
	  // Determino sentido
	  if(PWM_Az >= 0)
	     // avanza paun lao
	   	 // Ley_pid es +
	     PORTB &= 0xef;		
  	  else
	  	{
		 // avanza pal otAz
	     PORTB |= 0x10;				 			 
	     // Ley_pid es -
		 PWM_Az = -1 * PWM_Az;
		}

	  // Limito el máximo y mínimo ley pid
	  if(PWM_Az >= 1023)
	  	 PWM_Az = 1023;
			
	  if(PWM_Az <= 0)
	     PWM_Az = 0;
			
	  // Waiting when an OCF1A completes.
	  while( !(TIFR1 & (1<<OCF1A)) )
	            ;
	  /* Saco el PWM para el motor Azll */
	  OCR1A = PWM_Az;
   
      /* OCF1A = OCF1B = 1 los limpio */
      TIFR1 |= 0x18;
