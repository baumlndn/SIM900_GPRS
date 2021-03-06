/*
 * gprs.c
 *
 *  Created on: 11.02.2015
 *      Author: baumlndn
 */

#include "gprs.h"
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include "config.h"

uint8_t GPRS_Init( void )
{
	/* retry counter */
	uint8_t retryCnt = 5;

	/* Initialize SIM900 power pin */
#ifdef SIM900_POWER_USAGE

	SIM900_POWER_DDR 	|= (1<<SIM900_POWER_PIN);

#endif

	/* Initialize SIM900 reset pin */
#ifdef SIM900_RST_USAGE

	SIM900_RST_DDR 	|= (1<<SIM900_RST_PIN);

#endif

#ifdef DEBUG
	DDRC = 0xFF;
#endif

	/* Initialize USART */
	USART_Init(51u);

	/* activate global interrupt */
	sei();

	_delay_ms(1000);

	/* Power on SIM900 */
	GPRS_SwitchOn();

	/* local variable used for error deteciton */
	uint8_t tmpError = 0;

	/* Switch echo off */
	if (tmpError == 0)
	{
#ifdef DEBUG
		tmpError = GPRS_SendConfirm("ATE1");
#else
		tmpError = GPRS_SendConfirm("ATE0");
#endif
	}

	/* Set type to GPRS */
	if (tmpError == 0)
	{
		tmpError = GPRS_SendConfirm("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
	}

	/* Set GPRS parameter */
	if (tmpError == 0)
	{
		tmpError = GPRS_SendConfirm("AT+SAPBR=3,1,\"APN\",\"internet\"");
	}

	/* wait for some time */
	_delay_ms(2000);

	/* Open GPRS context */
	retryCnt = 5;

	while ( ( retryCnt > 0 ) && (tmpError == 0) )
	{
		if ( GPRS_SendConfirm("AT+SAPBR=1,1") == 0x00)
		{
			retryCnt = 0;
		}
		else
		{
			if (retryCnt == 1)
			{
				tmpError = 0x01;
			}
			else
			{
				retryCnt--;
			}
		}
	}

	/* Wait for connection to be established */
	_delay_ms(3000);

	/* query GPRS status */
	if (tmpError == 0)
	{
		tmpError = GPRS_SendConfirm("AT+SAPBR=2,1");
	}

	return tmpError;
}

void GPRS_End ( void )
{
	(void) GPRS_SendConfirm("AT+SAPBR=0,1");

	/* Power off SIM900 */
	GPRS_SwitchOff();
}

void GPRS_Reset ( void )
{

#ifdef SIM900_RST_USAGE

	_delay_ms(2000);
	SIM900_RST_PORT |=  (1<<SIM900_RST_PIN);
	_delay_ms(1000);
	SIM900_RST_PORT &=  ~(1<<SIM900_RST_PIN);

#endif

}

void GPRS_SwitchOn( void )
{
	uint8_t retryCnt = 5;

	if (GPRS_SendConfirm("AT") == 0)
	{

#ifdef SIM900_RST_USAGE

		GPRS_Reset();

#endif

	}
	else
	{

#ifdef SIM900_POWER_USAGE

		SIM900_POWER_PORT |=  (1<<SIM900_POWER_PIN);
		_delay_ms(500);
		SIM900_POWER_PORT &=  ~(1<<SIM900_POWER_PIN);

#endif

	}

	/* Initialize SIM900 */
	_delay_ms(6000);

	/* Check for init */
	while (retryCnt > 0)
	{
		/* Disable global interrupt */
		cli();

		/* If pattern was found, set retryCnt to zero */
		char tmpBuffer[255];

		(void) USART_ReadBuffer(&tmpBuffer[0],254);

		if( strstr( &tmpBuffer[0], "Call Ready") != NULL)
		{
			retryCnt = 0;
		}
		else
		{
			retryCnt--;
		}

		/* Reenable interrupt */
		sei();

		/* wait for some time */
		_delay_ms(1000);
	}
}

void GPRS_SwitchOff( void )
{
	if (GPRS_SendConfirm("AT") == 0)
	{

#ifdef SIM900_POWER_USAGE

		_delay_ms(2000);
		SIM900_POWER_PORT |=  (1<<SIM900_POWER_PIN);
		_delay_ms(1000);
		SIM900_POWER_PORT &=  ~(1<<SIM900_POWER_PIN);

#endif
	}
}


void GPRS_Send ( unsigned char * value, uint8_t length )
{
	uint8_t idx;

	for (idx=0;idx<length;idx++)
	{
		USART_Transmit( value[idx] );
	}

	/* Transmit CR and LF */
	USART_Transmit(0x0D);
	USART_Transmit(0x0A);
}

uint8_t GPRS_SendConfirm ( char * value )
{
	/* return value */
	uint8_t retVal = 1;

	/* retry varible */
	uint8_t retryCnt = 5;

	/* Send data via USART */
	GPRS_Send ((unsigned char *)value,strlen(value));

	/* Wait for some time */
	_delay_ms(DELAY_MS_DEFAULT);

	/* Wait and check for answer from SIM900 */
	while ( (retryCnt > 0) && (retVal == 1) )
	{
		/* Disable interrupt */
		cli();

		/* temporary copy of rx buffer */
		char tmpBuffer[255];
		uint8_t tmpCnt = USART_ReadBuffer( &tmpBuffer[0], 254);

		if (tmpCnt >= 6u)
		{
			if (
					(tmpBuffer[tmpCnt-4] == 'O') &&
					(tmpBuffer[tmpCnt-3] == 'K')
			   )
			{
				retVal = 0;
			}
		}

		/* Reenable interrupt */
		sei();

		/* decrement retryCnt */
		retryCnt--;

		/* wait for some time */
		_delay_ms(DELAY_MS_DEFAULT);
	}

#ifdef DEBUG
	/* Display status in PORTC LEDs */
	if (retVal == 1)
	{
		PORTC = 0x55;
	}
	else
	{
		PORTC = 0x00;
	}
#endif //PORT_DEBUG

	return retVal;
}
