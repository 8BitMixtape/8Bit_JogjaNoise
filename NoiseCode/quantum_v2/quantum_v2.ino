#include <Arduino.h>
#line 1 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
#line 1 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
/*
    main.cpp
    Quantum DJ
    
    [ MIT license ]

    Copyright (c) 2015 - 2017, Alexander Zolotov <nightradio@gmail.com>
    www.warmplace.ru
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to 
    deal in the Software without restriction, including without limitation the 
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in 
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include <WS2812.h>

#define LEDCount 8
#define outputPin 3

WS2812 LED(LEDCount);
cRGB value;

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include "neo_lib.h"

#ifdef PROGMEM
#undef PROGMEM
#define PROGMEM __attribute__((section(".progmem.data")))
#endif


#define CLOCK_PRESCALER 3
#define F_CPU ( 8000000UL >> CLOCK_PRESCALER )

//Half of sinus:
static const unsigned char g_hsin_tab[ 256 ] PROGMEM =
{
  0, 3, 6, 9, 12, 15, 18, 21, 25, 28, 31, 34, 37, 40, 43, 46,
  49, 52, 56, 59, 62, 65, 68, 71, 74, 77, 80, 83, 86, 89, 92, 95,
  97, 100, 103, 106, 109, 112, 115, 117, 120, 123, 126, 128, 131, 134, 136, 139,
  142, 144, 147, 149, 152, 154, 157, 159, 162, 164, 167, 169, 171, 174, 176, 178,
  180, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211,
  212, 214, 216, 217, 219, 221, 222, 224, 225, 227, 228, 229, 231, 232, 233, 235,
  236, 237, 238, 239, 240, 242, 243, 243, 244, 245, 246, 247, 248, 249, 249, 250,
  251, 251, 252, 252, 253, 253, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 252, 251,
  251, 250, 249, 249, 248, 247, 246, 245, 245, 244, 243, 242, 241, 240, 238, 237,
  236, 235, 234, 232, 231, 230, 228, 227, 225, 224, 222, 221, 219, 218, 216, 214,
  213, 211, 209, 207, 205, 203, 201, 200, 198, 196, 194, 191, 189, 187, 185, 183,
  181, 179, 176, 174, 172, 169, 167, 165, 162, 160, 157, 155, 152, 150, 147, 145,
  142, 139, 137, 134, 131, 129, 126, 123, 120, 118, 115, 112, 109, 106, 104, 101,
  98, 95, 92, 89, 86, 83, 80, 77, 74, 71, 68, 65, 62, 59, 56, 53,
  50, 47, 44, 41, 37, 34, 31, 28, 25, 22, 19, 16, 12, 9, 6, 3
};

#line 74 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
static int sin8( int phase );
#line 125 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
static unsigned char get_options( void );
#line 139 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
static int adc_read();
#line 151 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
void adc_init();
#line 161 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
void adc_start();
#line 165 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
int main();
#line 74 "/Users/xcorex/Documents/Arduino/quantumNEODJ/quantumNEODJ.ino"
inline static int sin8( int phase )
{
  int rv;
  phase &= 511;
  if ( phase < 256 )
    rv = (int)pgm_read_byte( &g_hsin_tab[ phase & 255 ] );
  else
    rv = -(int)pgm_read_byte( &g_hsin_tab[ phase & 255 ] );
  return rv;
}


enum
{
  MODE_REPEAT = 0,
  MODE_REPEAT2,
  MODE_REPEAT3,
  MODE_REPEAT_MOD, //fast modulations
  MODE_REPEAT_MOD2, //stream modification: add
  MODE_REPEAT_MOD3, //stream modification: xor
  MODE_REPEAT_MOD4, //stream modification: mul
  MODE_WAVE,
  MODE_SINE,
  MODE_SINE2,
  MODE_MAX,
};

volatile uint16_t pot1; // 0...255
volatile uint16_t pot2; // 0...255
volatile uint16_t pot3; // 0...255

volatile uint8_t adc1 = _BV(ADLAR) | _BV(MUX0); //PB2-ADC1 pot1
volatile uint8_t adc2 = _BV(ADLAR) | _BV(MUX1); //PB4-ADC2 pot2
volatile uint8_t adc3 = _BV(ADLAR) | _BV(MUX0) | _BV(MUX1); //PB3-ADC3 pot3

#define ENTER_CRIT()    {byte volatile saved_sreg = SREG; cli()
#define LEAVE_CRIT()    SREG = saved_sreg;}


#define SLOWDOWN( len ) \
  { volatile char d = 0; for( volatile int dd = 0; dd < ( len ); dd++ ) d++; }

#define OUT( bit ) \
  { PORTB = ( PORTB & ~( 1 << output_bit_offset ) ) | ( bit ) << output_bit_offset; }

const int frame_len = 1024;
const int output_bit_offset = 1;

#define OPTION1_BIT ( 1 << 1 )
#define OPTION2_BIT ( 1 << 2 )

static unsigned char get_options( void )
{
  unsigned char rv = 0;
  uint8_t b = wasButtonPressed(pot3);

  if ( b == BUTTON_LEFT                )   rv |= 1;
  if ( b == BUTTON_RIGHT               )    rv |= 2;

  return rv;
}

#define adc_read_begin() ADCSRA |= ( 1 << ADSC )
#define adc_read_finished() ( ( ADCSRA & ( 1 << ADSC ) ) == 0 )
#define adc_get() ADC
static int adc_read() //Get 0...1023 or -512...+511 value from ADC channel
{
  return pot1 + pot2 + pot3;
  // start single convertion
  adc_read_begin();

  // wait for conversion to complete
  while ( !adc_read_finished() ) {}

  return adc_get();
}

void adc_init()
{
  ADCSRA |= _BV(ADIE); //adc interrupt enable
  ADCSRA |= _BV(ADEN); //adc enable
  ADCSRA |= _BV(ADATE); //auto trigger
  ADCSRA |= _BV(ADPS0) | _BV(ADPS1) | _BV(ADPS2); //prescale 128
  ADMUX  = adc3;
  ADCSRB = 0;
}

void adc_start()
{
  ADCSRA |= _BV(ADSC); //start adc conversion
}
int main()
{
  unsigned int stream = 0;
  unsigned int stream2 = 0;
  unsigned int emotion_counter = 0;

    clock_prescale_set(clock_div_16); //1MHZ LOFI
  adc_init();
  sei(); //enable global interrupt
  adc_start(); //start adc conversion

  //#ifdef CLOCK_PRESCALER
  //  CLKPR = 1 << CLKPCE;
  //  CLKPR = CLOCK_PRESCALER;
  //#endif

  DDRB = 1 << output_bit_offset; //Set Port B output bits
  PORTB = 0x00; //Turn all output pins on Port B off
  //  PORTB |= OPTION1_BIT | OPTION2_BIT; //Enable Pull-up resistors for the input pins
  int btn_timer = 0;
  int opt_btn = 0;
int ledRefresh = 0;
uint8_t fq = 1;
pinMode(0, OUTPUT);



  while ( 1 )
  {
    digitalWrite(0, HIGH);
    btn_timer++;

//    if (btn_timer > 500)
    {
      uint8_t b = getButton(pot3);
      if ( b == BUTTON_LEFT)   opt_btn |= 1;
//      if ( b == BUTTON_RIGHT)  opt_btn |= 2;
      if ( b == BUTTON_NONE)   opt_btn = 0;

      uint8_t c = wasButtonPressed(pot3);

      if (c == BUTTON_RIGHT){
        fq++;
        fq = fq % 5;
        CLKPR = 1 << CLKPCE;
        CLKPR = fq;
      }
      btn_timer = 0;
    }



    unsigned char opt = opt_btn;
    stream2 <<= 1;
    stream2 |= adc_read() & 1;
    unsigned int emotion_amp = stream & 127;
    unsigned int slowdown = ( ( stream & 127 ) * emotion_amp ) >> 7;
    unsigned int frame_len2 = ( frame_len + ( sin8( emotion_counter << 3 ) + 255 ) ) / ( ( slowdown >> 1 ) + 1 );
    if ( opt & 1 ) frame_len2 <<= 2;
    unsigned int mode = ( ( stream >> 8 ) + stream2 + emotion_counter ) % MODE_MAX;
    if ( opt & 2 ) mode = MODE_REPEAT3;
    emotion_counter++;
    switch ( mode )
    {
      case MODE_REPEAT:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_REPEAT2:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= ( adc_read() >> 1 ) & 1;
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_REPEAT3:
        {
          char bb = emotion_amp & 7;
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= ( adc_read() >> 1 ) & 1;
            OUT( stream & 1 );
            SLOWDOWN( slowdown + ( ( sin8( i << bb ) + 255 ) >> 6 ) );
          }
        }
        break;
      case MODE_REPEAT_MOD:
        {
          for ( int i = 0; i < frame_len; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            OUT( stream & 1 );
            SLOWDOWN( stream & 15 );
          }
        }
        break;
      case MODE_REPEAT_MOD2:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            stream += i;
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_REPEAT_MOD3:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            stream ^= ( stream >> 8 );
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_REPEAT_MOD4:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            stream += ( stream >> 8 );
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_WAVE:
        {
          for ( int i = 0; i < frame_len2 >> 2; i++ )
          {
            OUT( ( stream << ( i & 15 ) ) & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_SINE:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            stream += sin8( i );
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
      case MODE_SINE2:
        {
          for ( int i = 0; i < frame_len2; i++ )
          {
            stream <<= 1;
            stream |= adc_read() & 1;
            stream ^= sin8( i );
            OUT( stream & 1 );
            SLOWDOWN( slowdown );
          }
        }
        break;
    }
  }

//  
//    ledRefresh++;
//
//    if(ledRefresh > 10000)
//    {
//        for (int i = 0; i < LEDCount; i++)
//        {
//          value.b = 0; value.g = pot1; value.r = pot2; // RGB Value -> Blue
//          LED.set_crgb_at(i, value);
//        }
//  
//      ledRefresh = 0;
//        LED.sync();
//
//    }
}

ISR(ADC_vect)
{
  //http://joehalpin.wordpress.com/2011/06/19/multi-channel-adc-with-an-attiny85/

  static uint8_t firstTime = 1;
  static uint8_t val;


  val = ADCH;

  if (firstTime == 1) {
    firstTime = 0;
  }
  else if (ADMUX  == adc1) {
    pot3 = val;
    ADMUX = adc2;
  }
  else if ( ADMUX == adc2) {
    pot2  = val;
    ADMUX = adc3;
  }
  else if ( ADMUX == adc3) {
    pot1  = val;
    ADMUX = adc1;
  }

}
