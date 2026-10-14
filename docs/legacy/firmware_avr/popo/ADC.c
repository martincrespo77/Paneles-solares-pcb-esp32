/************************************************************/
void ADC_Init()
{
	/*
	ADMUX register
	REFS1 = REFS0 = 0: External AREF selected. Internal voltage turned OFF.
	ADLAR = 0: ADCH: 2MSB; ADCL: 8LSB.
	*/
	ADMUX = 0x00;
	 
	/*
    ADCSRA register:
	ADEN = 1: ADC enabled.
	ADFR = 0: Single conversion mode.
 	ADIE = 0: ADC interrupt disabled.
    ADIF = 1: cleared. 
	ADPS2 = ADPS1 = 1 y ADPS0 = 0 : Ck/64.
	*/
	ADCSRA = 0x96;
}
/************************************************************/
