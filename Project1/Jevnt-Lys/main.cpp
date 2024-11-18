#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	// Set PB0 (pin 0 on port B) as output
	DDRB |= (1 << PB0);

	// Write a low value to PB0 (turn it off)
	PORTB = 0x01;

	while(1)
	{

	}

	return 0;
}