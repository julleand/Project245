#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
	DDRB |= (1 << PB0);

	uint8_t brightness = 1;
	int8_t fade_amount = 1;
	
	while (1)
	{
		
		for (uint8_t i = 0; i < 255; i++)
		{
			if (i < brightness)
			{
				PORTB &= ~(1 << PB0);
			}
			else
			{
				PORTB |= (1 << PB0);
			}
			
			_delay_us(30);
		}
		
		// Adjust brightness
		brightness += fade_amount;
		
		// Reverse the direction at the limits (1 and 254)
		if (brightness == 1 || brightness == 254)
		{
			fade_amount = -fade_amount;
		}
		
		_delay_ms(10);
	}

	return 0;
}