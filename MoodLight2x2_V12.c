///////////////////////////////////////////////////////////////////////
//
// MoodLight Software (c) 2009 N. Turianskyj
// nichtkommerzielle Nutzung freigegeben
//
// Software-PWM by PoWl - 25.10.08
// webmaster@the-powl.de
//
// Encoder-Routinen von Peter Dannegger
//
// ATmega8 @ 8Mhz
//
///////////////////////////////////////////////////////////////////////

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "taster.c"
#include "HSBFunctions.c"
#include "encoder1.c"




// -------------------------- 8-Bit PWM --------------------------- //
// An - 254 Graustufen - Aus

#define CHANNELS 12

uint8_t pwm_mask_PORTB_A[CHANNELS + 1];
uint8_t pwm_mask_PORTD_A[CHANNELS + 1];
uint8_t pwm_timing_A[CHANNELS + 1];

uint8_t pwm_mask_PORTB_B[CHANNELS + 1];
uint8_t pwm_mask_PORTD_B[CHANNELS + 1];
uint8_t pwm_timing_B[CHANNELS + 1];

uint8_t *ptr_PORTB_main = &pwm_mask_PORTB_A;
uint8_t *ptr_PORTD_main = &pwm_mask_PORTD_A;
uint8_t *ptr_timing_main = &pwm_timing_A;

uint8_t *ptr_PORTB_isr = &pwm_mask_PORTB_B;
uint8_t *ptr_PORTD_isr = &pwm_mask_PORTD_B;
uint8_t *ptr_timing_isr = &pwm_timing_B;

uint8_t pwm_value[CHANNELS];
uint8_t pwm_queue[CHANNELS];

volatile uint8_t pwm_cycle;
volatile bool pwm_change;

uint8_t pwm_pin[CHANNELS] = { PD4, PD3, PD2, PD7, PD6, PD5, PB5, PB4, PB3, PB2, PB1, PB0};

uint8_t **pwm_port[CHANNELS] = {
  &ptr_PORTD_main,
  &ptr_PORTD_main,
  &ptr_PORTD_main,
  &ptr_PORTD_main,
  &ptr_PORTD_main,
  &ptr_PORTD_main,
  &ptr_PORTB_main,
  &ptr_PORTB_main,
  &ptr_PORTB_main,
  &ptr_PORTB_main,
  &ptr_PORTB_main,
  &ptr_PORTB_main
};

volatile uint8_t iProg=2;					  	// Programmwahl
volatile uint8_t bChangeProg=0;					// Programm wurde gewechselt

// Tabelle für logarithmische Helligkeitsverteilung
uint16_t bri_table_8[64]  PROGMEM = {0,0,1,1,1,2,2,2,2,2,2,3,3,3,3,4,4,4,5,5,6,
									6,7,8,8,9,10,11,12,13,14,15,17,18,20,22,24,
									26,28,31,34,37,40,44,48,52,57,62,68,74,81,89,
									97,106,116,126,138,150,164,179,196,214,234,255};


void pwm_update(void)
{
  // Sortieren mit InsertSort

  for(uint8_t i=1; i<CHANNELS; i++)
  {
    uint8_t j = i;

    while(j && pwm_value[pwm_queue[j - 1]] > pwm_value[pwm_queue[j]])
    {
      uint8_t temp = pwm_queue[j - 1];
      pwm_queue[j - 1] = pwm_queue[j];
      pwm_queue[j] = temp;

      j--;
    }
  }

  // Maske generieren

  for(uint8_t i=0; i<(CHANNELS + 1); i++)
  {
    ptr_PORTB_main[i] = 0;
    ptr_PORTD_main[i] = 0;
    ptr_timing_main[i] = 0;
  }

  uint8_t j = 0;

  for(uint8_t i=0; i<CHANNELS; i++)
  {
    // Sonderfall "ganz AUS" ausschließen:
    //  - Pins garnicht erst anschalten
    //  - Pins die vorher "ganz AN" waren müssen trotzdem noch deaktiviert werden, deshalb nur folgendes in der if
    if(pwm_value[pwm_queue[i]] != 0)
    {
      (*pwm_port[pwm_queue[i]])[0] |= (1 << pwm_pin[pwm_queue[i]]);
    }

    // Sonderfall "ganz AN" ausschließen:
    //  - Nicht in Timing-Tabelle übernehmen da sonst die Pointer niemals getauscht werden
    //  - Pins die "ganz AN" sind müssen nicht abgeschaltet werden
    if(pwm_value[pwm_queue[i]] != 255)
    {
      (*pwm_port[pwm_queue[i]])[j + 1] |= (1 << pwm_pin[pwm_queue[i]]);

      ptr_timing_main[j] = pwm_value[pwm_queue[i]];
    }

    if(pwm_value[pwm_queue[i]] != pwm_value[pwm_queue[i + 1]])
    {
      j++;
    }
  }

  /*
  // Der ISR etwas Arbeit abnehmen
  for(uint8_t i=1; i<(CHANNELS + 1); i++)
  {
    ptr_PORTB_main[i] = ~ptr_PORTB_main[i];
    ptr_PORTD_main[i] = ~ptr_PORTD_main[i];
  }
  */

  // Synchronisation

  pwm_change = 0;
  while(!pwm_change);

  cli();

  uint8_t *temp_ptr;

  temp_ptr = ptr_PORTB_main;
  ptr_PORTB_main = ptr_PORTB_isr;
  ptr_PORTB_isr = temp_ptr;

  temp_ptr = ptr_PORTD_main;
  ptr_PORTD_main = ptr_PORTD_isr;
  ptr_PORTD_isr = temp_ptr;

  temp_ptr = ptr_timing_main;
  ptr_timing_main = ptr_timing_isr;
  ptr_timing_isr = temp_ptr;

  sei();
}


//Hilfsfunktion
void delay_ms(uint16_t ms)
{
	for(uint16_t i=0;i<ms;i++) _delay_ms(1);
}




int main(void)
{
	//Konfiguration Taster Input an PC0 und Encoder PC1, PC2
	DDRC &= ~( 1 << PC0 );
	DDRC &= ~( 1 << PC1 );
	DDRC &= ~( 1 << PC2 );
	//enable internal pullup PC0
	PORTC |= (1<<PC0) | (1<<PC1) | (1<<PC2);    

	//Soft-PWM-Ports
	DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5);
	DDRD = (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);

	// PWM initialisieren
	for(uint8_t i=0; i<CHANNELS; i++)
	{
		pwm_queue[i] = i;
	}

	// Timer für PWM
	OCR1A = 255;
	TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B);
	TCCR1B = (1 << WGM12) | 4;

	encode_init();		//Encoder initialisieren

	sei();



	//diverse Variablen für Farben etc.
	uint8_t RGB=0;							//für Regenbogenprog
	uint8_t flip=0;							//Hilfsvariablen
	int16_t i=0; 
	uint16_t Step_stop=50;					//Fading-Delay
	uint16_t Step_delay=10;					//Pause
	uint16_t MoodDelay=10000;				//Verweilzeit Farbe
	uint16_t baseColor=1;					//für HSB-Farbrad
	int8_t iColor=1;						//Prog 4 feste Farbe
	uint16_t bri_val=63;					//Wert für Helligkeitsindex
	//Farben etc.
	int16_t hue, hue_old, sat, bri, red, blue, green;
	int16_t hue1, hue_old1, red1, blue1, green1;
	int16_t hue2, hue_old2, red2, blue2, green2;
	int16_t hue3, hue_old3, red3, blue3, green3;

	//Initialisierung einige Variablen
	sat=127;bri=155;
	hue_old=0;hue=700;hue_old1=0;hue1=700;hue_old2=0;hue2=700;hue_old3=0;hue3=700;
	red=0;green=0;blue=0;


	while (1)
	{
//	  	srand ((unsigned) time(NULL));


		//Programmauswahl
		switch (iProg)
	  	{
			case 0: { 	//alle LEDs gemeinsam hoch-/runterfahren...

				Step_delay += (encode_read4()*2);
				if (Step_delay>200) Step_delay=200; else if (Step_delay<5) Step_delay=5;
				if (flip==0) i++; else i--;
		      	if (i > 255) 
			  	{
		         	flip=1;
					i=255;
				    delay_ms(200);       //Pause am Ende
				}
				else if (i < 0)
				{
					flip=0;
					i=0;
				    delay_ms(200);       //Pause am Ende
				}
				pwm_value[0]=255-i;
				pwm_value[1]=255-i;
				pwm_value[2]=255-i;
				pwm_value[3]=i;
				pwm_value[4]=i;
				pwm_value[5]=i;
				pwm_value[6]=255-i;
				pwm_value[7]=255-i;
				pwm_value[8]=255-i;
				pwm_value[9]=i;
				pwm_value[10]=i;
				pwm_value[11]=i;
				pwm_update();

			    delay_ms(Step_delay);       //Geschwindigkeit Fading
				break;
			}

			case 1:	
			{	//Regenbogen-Fader
				if (bChangeProg==1)
				{
					red=green=blue=0;
					bChangeProg=0;
				}
				Step_delay += (encode_read4()*2);
				if (Step_delay>200) Step_delay=200; else if (Step_delay<5) Step_delay=5;
			    delay_ms(Step_delay);       //Geschwindigkeit Fading
				if (RGB == 0)
				{
					red++;
					blue--;
					delay_ms(Step_delay);
				}
				if (RGB == 1)
				{
					red--;
					green++;
					delay_ms(Step_delay);
				}
				if (RGB == 2)
				{
					blue++;
					green--;
					delay_ms(Step_delay);
				}
				if (red == 255)
				{
					RGB=1;
					blue=0;
					delay_ms(Step_stop);
				}
				if (green == 255)
				{
					RGB=2;
					red=0;
					delay_ms(Step_stop);
				}
				if (blue == 255)
				{
					RGB=0;
					green=0;
					delay_ms(Step_stop);
				}
				//Farben je 2 komplementär ausgeben
				pwm_value[0]=red;
				pwm_value[1]=green;
				pwm_value[2]=blue;
				pwm_value[3]=255-red;
				pwm_value[4]=255-green;
				pwm_value[5]=255-blue;
				pwm_value[6]=red;
				pwm_value[7]=green;
				pwm_value[8]=blue;
				pwm_value[9]=255-red;
				pwm_value[10]=255-green;
				pwm_value[11]=255-blue;
				pwm_update();

				break;
			}

		case 2:	{	//MoodLight-Fading über Zufallsfarben

				//neue Farben bestimmen
				hue=rand() %764;
				hue1=rand() %764;
				hue2=rand() %764;
				hue3=rand() %764;

				//Fading von alter zu neuer Farbe
				while (((hue_old!=hue) || (hue_old1 != hue1) || (hue_old2 != hue2) || (hue_old3 != hue3)) && bChangeProg!=1)
				{
					//Laufrichtung bestimmen und bei jedem Durchlauf einen Schritt
					//je RGB-LED unterschiedlich lange Laufzeiten
					if (hue_old < hue) hue_old++; else if (hue_old > hue) hue_old--;
					if (hue_old1 < hue1) hue_old1++; else if (hue_old1 > hue1) hue_old1--;
					if (hue_old2 < hue2) hue_old2++; else if (hue_old2 > hue2) hue_old2--;
					if (hue_old3 < hue3) hue_old3++; else if (hue_old3 > hue3) hue_old3--;
					delay_ms(10);

					//RGB-Umwandlung
					HSB2RGB(hue_old, sat, bri, &red, &green, &blue);
					HSB2RGB(hue_old1, sat, bri, &red1, &green1, &blue1);
					HSB2RGB(hue_old2, sat, bri, &red2, &green2, &blue2);
					HSB2RGB(hue_old3, sat, bri, &red3, &green3, &blue3);
					//Ausgabe der 4 RGB-Kanäle
					pwm_value[0]=red;
					pwm_value[1]=green;
					pwm_value[2]=blue;
					pwm_value[3]=red1;
					pwm_value[4]=green1;
					pwm_value[5]=blue1;
					pwm_value[6]=red2;
					pwm_value[7]=green2;
					pwm_value[8]=blue2;
					pwm_value[9]=red3;
					pwm_value[10]=green3;
					pwm_value[11]=blue3;
					pwm_update();
				}
				//Verweilzeit Farbe
				MoodDelay += (encode_read2()*100);
				if (MoodDelay>30000) MoodDelay=30000; else if (MoodDelay<500) MoodDelay=500;
				if(bChangeProg==0)
					delay_ms(MoodDelay);
				else
					bChangeProg=0;
				//Werte umkopieren
				hue_old=hue;
				hue_old1=hue1;
				hue_old2=hue2;
				hue_old3=hue3;

				break;
			}

		case 3:	{	// Farbrad über die 4 RGB-Kanäle
				if (bChangeProg==1)
				{
					baseColor=0;
					bChangeProg=0;
				}
				//Farbe berechnen
				hue=baseColor % 764;
				hue1=(baseColor+191) % 764;
				hue2=(baseColor+382) % 764;
				hue3=(baseColor+573) % 764;
				//RGB-Umwandlung
				HSB2RGB(hue, sat, bri, &red, &green, &blue);
				HSB2RGB(hue1, sat, bri, &red1, &green1, &blue1);
				HSB2RGB(hue2, sat, bri, &red2, &green2, &blue2);
				HSB2RGB(hue3, sat, bri, &red3, &green3, &blue3);
				pwm_value[0]=red;
				pwm_value[1]=green;
				pwm_value[2]=blue;
				pwm_value[3]=red1;
				pwm_value[4]=green1;
				pwm_value[5]=blue1;
				pwm_value[6]=red2;
				pwm_value[7]=green2;
				pwm_value[8]=blue2;
				pwm_value[9]=red3;
				pwm_value[10]=green3;
				pwm_value[11]=blue3;
				pwm_update();
				//Verweilzeit Farbe
				Step_stop += encode_read4();
				if (Step_stop > 200) Step_stop=200;
				if (Step_stop < 1) Step_stop=1;
				delay_ms(Step_stop);

				baseColor+=1;if (baseColor>764) baseColor=1;
				break;
			}

		case 4:	{	// verschiedene feste Farben

				switch (iColor)
				{
					case 1:
					{
						red=255;red1=255;red2=255;red3=255;
						green=0;green1=0;green2=0;green3=0;
						blue=0;blue1=0;blue2=0;blue3=0;
						break;
					}
					case 2:
					{
						red=0;red1=0;red2=0;red3=0;
						green=255;green1=255;green2=255;green3=255;
						blue=0;blue1=0;blue2=0;blue3=0;
						break;
					}
					case 3:
					{
						red=0;red1=0;red2=0;red3=0;
						green=0;green1=0;green2=0;green3=0;
						blue=255;blue1=255;blue2=255;blue3=255;
						break;
					}
					case 4:
					{
						red=255;red1=255;red2=255;red3=255;
						green=244;green1=244;green2=244;green3=244;
						blue=0;blue1=0;blue2=0;blue3=0;
						break;
					}
					case 5:
					{
						red=255;red1=255;red2=255;red3=255;
						green=244;green1=0;green2=244;green3=0;
						blue=0;blue1=0;blue2=0;blue3=0;
						break;
					}
					case 6:
					{
						red=0;red1=0;red2=0;red3=0;
						green=255;green1=255;green2=255;green3=255;
						blue=200;blue1=200;blue2=200;blue3=200;
						break;
					}
					case 7:
					{
						red=0;red1=0;red2=0;red3=0;
						green=255;green1=0;green2=255;green3=0;
						blue=0;blue1=255;blue2=0;blue3=255;
						break;
					}
					case 8:
					{
						red=255;red1=255;red2=255;red3=255;
						green=0;green1=0;green2=0;green3=0;
						blue=0;blue1=255;blue2=0;blue3=255;
						break;
					}
					case 9:
					{
						red=0;red1=0;red2=0;red3=0;
						green=255;green1=255;green2=255;green3=255;
						blue=0;blue1=128;blue2=0;blue3=128;
						break;
					}
					case 10:
					{
						red=128;red1=128;red2=128;red3=128;
						green=0;green1=255;green2=0;green3=255;
						blue=255;blue1=0;blue2=255;blue3=0;
						break;
					}
/*					case 11:
					{
						red=;red1=;red2=;red3=;
						green=;green1=;green2=;green3=;
						blue=;blue1=;blue2=;blue3=;
						break;
					}
					case 12:
					{
						red=;red1=;red2=;red3=;
						green=;green1=;green2=;green3=;
						blue=;blue1=;blue2=;blue3=;
						break;
					}
*/				}
				iColor += encode_read4();
				if (iColor>10) iColor=10; else if (iColor<1) iColor=1;
/*				pwm_value[0]=((bri + 1) * (255-red)) >> 8;//255-red;
				pwm_value[1]=((bri + 1) * (255-green)) >> 8;//255-green;
				pwm_value[2]=((bri + 1) * (255-blue)) >> 8;//255-blue;
				pwm_value[3]=((bri + 1) * (255-red1)) >> 8;//255-red1;
				pwm_value[4]=((bri + 1) * (255-green1)) >> 8;//255-green1;
				pwm_value[5]=((bri + 1) * (255-blue1)) >> 8;//255-blue1;
				pwm_value[6]=((bri + 1) * (255-red2)) >> 8;//255-red2;
				pwm_value[7]=((bri + 1) * (255-green2)) >> 8;//255-green2;
				pwm_value[8]=((bri + 1) * (255-blue2)) >> 8;//255-blue2;
				pwm_value[9]=((bri + 1) * (255-red3)) >> 8;//255-red3;
				pwm_value[10]=((bri + 1) * (255-green3)) >> 8;//255-green3;
				pwm_value[11]=((bri + 1) * (255-blue3)) >> 8;//255-blue3;
*/				pwm_value[0]=red;
				pwm_value[1]=green;
				pwm_value[2]=blue;
				pwm_value[3]=red1;
				pwm_value[4]=green1;
				pwm_value[5]=blue1;
				pwm_value[6]=red2;
				pwm_value[7]=green2;
				pwm_value[8]=blue2;
				pwm_value[9]=red3;
				pwm_value[10]=green3;
				pwm_value[11]=blue3;
				pwm_update();
				
				break;
			}

		case 5:	{	// nur zur Einstellung Helligkeit

				hue_old=0;
				hue_old1=255;
				hue_old2=509;
				hue_old3=382;
				//RGB-Umwandlung
				HSB2RGB(hue_old, sat, bri, &red, &green, &blue);
				HSB2RGB(hue_old1, sat, bri, &red1, &green1, &blue1);
				HSB2RGB(hue_old2, sat, bri, &red2, &green2, &blue2);
				HSB2RGB(hue_old3, sat, bri, &red3, &green3, &blue3);
				//Ausgabe der 4 RGB-Kanäle
				pwm_value[0]=red;
				pwm_value[1]=green;
				pwm_value[2]=blue;
				pwm_value[3]=red1;
				pwm_value[4]=green1;
				pwm_value[5]=blue1;
				pwm_value[6]=red2;
				pwm_value[7]=green2;
				pwm_value[8]=blue2;
				pwm_value[9]=red3;
				pwm_value[10]=green3;
				pwm_value[11]=blue3;
				pwm_update();

				//Wert bri einstellen
				bri_val += (encode_read2());
				if (bri_val > 63) bri_val=63;
				if (bri_val < 1) bri_val=1;
				//Helligkeit über Lookup-Tabelle berechnen
				bri=pgm_read_word(bri_table_8+bri_val);  //0..255

				break;
			}

		}


	} //while

	return 0;

}



ISR(TIMER1_COMPA_vect)
{
  pwm_cycle = 0;

  PORTB |= ptr_PORTB_isr[0];
  PORTD |= ptr_PORTD_isr[0];

  OCR1B = ptr_timing_isr[0];
  	//Tastenabfrage
	if (debounce(&PINC, PC0))     // Falls Taster an PIN PC0 gedrueckt..  
	{
		iProg=iProg+1;
		if (iProg > 5) iProg=0;
		bChangeProg=1;
	}


}



ISR(TIMER1_COMPB_vect)
{
  pwm_cycle++;

  PORTB &= ~ptr_PORTB_isr[pwm_cycle];
  PORTD &= ~ptr_PORTD_isr[pwm_cycle];

  if(ptr_timing_isr[pwm_cycle] != 0)
  {
    OCR1B = ptr_timing_isr[pwm_cycle];
  }
  else
  {
    pwm_change = 1;
  }
}
