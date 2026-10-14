//-----------------------------------------------------------
/* ADC0 */
ADMUX &= 0xf0;

ADC_Captura();

// mando 8lsb
adc_dato_lo = ADCL;
//  USART_Transmit(adc_dato_lo);

//  // espero que transmita
//  retardo();
//  retardo();

// mando 2msb
adc_dato_hi = ADCH & 0x03;
//  USART_Transmit(adc_dato_hi);

// armo la palabra de 10 bits
adc_dato_0 = (adc_dato_hi << 8) + adc_dato_lo;

//-----------------------------------------------------------
/* ADC1 */
ADMUX &= 0xf0;
ADMUX |= 0x01;
//ADMUX |= 0x02;

ADC_Captura();

// mando 8lsb
adc_dato_lo = ADCL;
//  USART_Transmit(adc_dato_lo);

//  // espero que transmita
//  retardo();
//  retardo();

// mando 2msb
adc_dato_hi = ADCH & 0x03;
//  USART_Transmit(adc_dato_hi);

// armo la palabra de 10 bits
adc_dato_1 = (adc_dato_hi << 8) + adc_dato_lo;

//-----------------------------------------------------------
/* ADC2 */
ADMUX &= 0xf0;
ADMUX |= 0x02;

ADC_Captura();

// mando 8lsb
adc_dato_lo = ADCL;
//  USART_Transmit(adc_dato_lo);

//  // espero que transmita
//  retardo();
//  retardo();

// mando 2msb
adc_dato_hi = ADCH & 0x03;
//  USART_Transmit(adc_dato_hi);

// armo la palabra de 10 bits
adc_dato_2 = (adc_dato_hi << 8) + adc_dato_lo;

//-----------------------------------------------------------
/* ADC3 */
ADMUX &= 0xf0;
ADMUX |= 0x03;

ADC_Captura();

// mando 8lsb
adc_dato_lo = ADCL;
//  USART_Transmit(adc_dato_lo);

//  // espero que transmita
//  retardo();
//  retardo();

// mando 2msb
adc_dato_hi = ADCH & 0x03;
//  USART_Transmit(adc_dato_hi);

// armo la palabra de 10 bits
adc_dato_3 = (adc_dato_hi << 8) + adc_dato_lo;

//-----------------------------------------------------------