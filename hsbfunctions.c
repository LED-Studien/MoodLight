/**************************************************************
*Tools für HSB und RGB
*
**************************************************************/



void HSB2RGB (int16_t hue, int16_t sat, int16_t bri, int16_t *red, int16_t *green, int16_t *blue)
{
uint16_t red_val, green_val, blue_val;

		if (hue > 764) hue = 0;

		if (hue < 255) {
		   red_val = (10880 - sat * (hue - 170)) >> 7;
		   green_val = (10880 - sat * (85 - hue)) >> 7;
		   blue_val = (10880 - sat * 85) >> 7;
		}
		else if (hue < 510) {
		   red_val = (10880 - sat * 85) >> 7;
		   green_val = (10880 - sat * (hue - 425)) >> 7;
		   blue_val = (10880 - sat * (340 - hue)) >> 7;
		}
		else {
		   red_val = (10880 - sat * (595 - hue)) >> 7;
		   green_val = (10880 - sat * 85) >> 7;
		   blue_val = (10880 - sat * (hue - 680)) >> 7;
		}
	
		//im HSB-Programm geht Helligkeit bis 255
		*red = ((bri + 1) * red_val) >> 8;
		*green = ((bri + 1) * green_val) >> 8;
		*blue = ((bri + 1) * blue_val) >> 8;
		
		return;		
}
