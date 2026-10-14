

/************************************************************/
unsigned char ADC_Captura(void)
{
	//ADSC = 1: ADC start conversion.
	ADCSRA |= 0x40;
	
	// waiting when an ADC completes.
	while( !(ADCSRA & (1<<ADIF)) )
	;
}
/************************************************************/
