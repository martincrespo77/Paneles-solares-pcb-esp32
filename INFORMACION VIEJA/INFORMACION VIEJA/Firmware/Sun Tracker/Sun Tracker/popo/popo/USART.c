/************************************************************/
void retardo()
{
	for(ii =0; ii<200; ii++)
	;
}
/************************************************************/


/************************************************************/
void USART_Init()
{
 // DEFINITIVO!!!!!!!!!
 /* 57600 baud with 20 MHz osc*/
 UBRR0H = 0x00;
 UBRR0L = 0x15;

 UCSR0A = 0x0;
	
 /* receiver ON/transmitter ON */
 UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	
 /* data 8/stop 1/parity NONE */
 UCSR0C = (1<<UCSZ01)| (1<<UCSZ00);
}
/************************************************************/

/************************************************************/
void USART_Transmit( unsigned char data)
{
 /* Wait for empty transmit buffer */
 while( !(UCSR0A & (1<<UDRE0)) )
   	  ;
	
 /* Put data into buffer, sends thr data */
 UDR0 = data;
	
 retardo();
}
/************************************************************/


/************************************************************/
unsigned char USART_Receive(void)
{
 /* Wait for data to be received */
 while( !(UCSR0A & (1<<RXC0)) )
    	;
	
 /* Get and return received data from buffer */
 return UDR0;
}
/************************************************************/

