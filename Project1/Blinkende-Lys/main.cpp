#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	// Set PB0 as output
	DDRB |= (1 << PB0);
	
	while(1)
	{
		// Set PB0 to LOW to sink current (turn the LED ON)
		PORTB &= ~(1 << PB0);  // Clear PB0, pulling it to GND

		_delay_ms(500);  // Wait 500 ms

		// Set PB0 to HIGH to stop sinking current (turn the LED OFF)
		PORTB |= (1 << PB0);  // Set PB0 to HIGH

		_delay_ms(500);  // Wait 500 ms
	}
	
	return 0;
}