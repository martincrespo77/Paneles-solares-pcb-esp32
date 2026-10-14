 
 // Canales del ADC
 adc_dato_0 = 0;
 adc_dato_1 = 0;
 adc_dato_2 = 0;
 adc_dato_3 = 0;
 
 Int_elev = 0;
 Int_azim = 0;

 error_elev = 0;
 error_azim = 0;



  
//===================================================
// ASIGNACIÓN DE VALORES A LAS CONSTANTES PARA EL PID
// --------------------------------------------------
   Integral_El = 0.0;
   Integral_Az = 0.0;

   // Ctes. PID
   Kp_El = 5.0;
   Ki_El = 0.1;
   Kd_El = 0.0;

   Kp_Az = 5.0;
   Ki_Az = 0.1;
   Kd_Az = 0.0;


    // Cte. reset anti-wind up
   //Reset_aw = 120.0;
   Reset_aw = 400.0;

  