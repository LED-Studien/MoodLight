/************************************************************************/
/*                                                                      */
/*                      Reading rotary encoder                   	*/
/*                      one, two and four step encoders supported	*/
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                                                                      */
/************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
 
				// target: ATmega16
//------------------------------------------------------------------------
 
#define	XTAL		8e6			// 8MHz
 
#define PHASE_A		(PINC & 1<<PC1)
#define PHASE_B		(PINC & 1<<PC2)
 
 
volatile int8_t enc_delta;			// -128 ... 127
static int8_t last;
 
 
void encode_init( void )
{
  int8_t new;
 
  new = 0;
  if( PHASE_A )
    new = 3;
  if( PHASE_B )
    new ^= 1;					// convert gray to binary
  last = new;					// power on state
  enc_delta = 0;
  TCCR2A = (1<<WGM21); 
  TCCR2B = (1<<CS22); 		// CTC, XTAL / 64
  OCR2A = (uint8_t)(XTAL / 64.0 * 1e-3 - 0.5);	// 1ms
  TIMSK2 |= 1<<OCIE2A;

}
 
 
ISR( TIMER2_COMPA_vect )				// 1ms for manual movement
{
  int8_t new, diff;
 
  new = 0;
  if( PHASE_A )
    new = 3;
  if( PHASE_B )
    new ^= 1;					// convert gray to binary
  diff = last - new;				// difference last - new
  if( diff & 1 ){				// bit 0 = value (1)
    last = new;					// store new as next last
    enc_delta += (diff & 2) - 1;		// bit 1 = direction (+/-)
  }
}
 
 
int8_t encode_read1( void )			// read single step encoders
{
  int8_t val;
 
  cli();
  val = enc_delta;
  enc_delta = 0;
  sei();
  return val;					// counts since last call
}
 
 
int8_t encode_read2( void )			// read two step encoders
{
  int8_t val;
 
  cli();
  val = enc_delta;
  enc_delta = val & 1;
  sei();
  return val >> 1;
}
 
 
int8_t encode_read4( void )			// read four step encoders
{
  int8_t val;
 
  cli();
  val = enc_delta;
  enc_delta = val & 3;
  sei();
  return val >> 2;
}
 
