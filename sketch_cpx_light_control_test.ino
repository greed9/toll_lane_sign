//
// Logic for light timing and on/off
//

// CPX hardware stuff
#include <Adafruit_CircuitPlayground.h>

// States for ON/OFF activity switching based on light level
#define OFF 0
#define INIT 1
#define DARK 2
#define LIGHT 3
#define DONE 4

char* state_names[5] = { "OFF", "INIT", "DARK", "LIGHT", "DONE"} ;

// Pins on sign
#define LEFT_YELLOW_ARROW   10
#define RIGHT_YELLOW_ARROW  12
#define RED_X               9
#define GREEN_ARROW         6

// PIN to trigger interrupt for motion detect
#define PIR_PIN 0 

// Global vas
static int state = INIT;
static int running = 0 ;
volatile int pir_trips = 0 ;

// Color struct for neopixels
typedef struct color_struct 
{
  uint8_t red ;
  uint8_t green ;
  uint8_t blue ;

} Colors ;

//
// LightTimer
//

static unsigned long start_time ;

class LightTimer {
    
public:

  unsigned long run_time ;
  unsigned  long delta ;

  LightTimer( unsigned long run_time )
  {
      run_time = run_time ;
      start_time = millis ( ) ;
      delta = 0 ;
  }

  void start( unsigned long prun_time )
  {
    Serial.println( "In timer start") ;
      run_time = prun_time ;
      start_time = millis ( ) ;
      Serial.print( "    start_time=") ;
      Serial.println( start_time) ;
      delta = 0 ;
      running = 1 ;
  }

  int tick( ) 
  {
      unsigned long current ;
      Serial.println( "In timer tick");
      Serial.print( "     running=");
      Serial.println( running ) ;
      Serial.print( "     start_time =" ) ;
      Serial.println( start_time) ;
      

      if(running) 
      {
          current = millis ( ) ;
          delta = current - start_time ;
          Serial.print( "    delta=") ;
          Serial.println( delta ) ;
          Serial.print( "    runtime=" ) ;
          Serial.println( run_time) ;
          if( delta > run_time )
          {
            Serial.println( "    Timer expired") ;
              return 1 ;
          }
          else
          {
              return 0 ;
          }

      }
      else
      {
        return 0 ;
      }
  }
      
  void stop( )
  {
      running = 0 ;
  }

  int is_running( ) 
  {
      return running ;
  }
    
} ;

// Encapsulate using PWM on a single pin to adjust brightness of LEDs
class Dimmer
{
  byte _pin = {} ;
  byte _currVal = 0 ;

  public: 
  
  Dimmer( byte pin ) : _pin{ pin }
  {
    analogWrite( _pin, _currVal ) ;
  }

  void setVal( byte val )
  {
    analogWrite( _pin, val ) ;
    _currVal = val ;
  }

  byte getVal ( )
  {
    return _currVal ;
  }

  void rampUp( byte inc, byte maxVal, int dly )
  {
    Serial.print( "pin=" ) ;
    Serial.print( _pin ) ;
    for( int i = _currVal ; i < maxVal ; i += inc ) 
    {
      analogWrite( _pin, i ) ;
      Serial.println(i) ;
      delay( dly ) ;
    }
    _currVal = maxVal ;
  }

  void rampDown( byte dec, byte minVal, int dly )
  {
    for( int i = _currVal ; i > minVal ; i -= dec )
    {
      analogWrite( _pin, i ) ;
      Serial.println(i) ;
      delay( dly ) ;
    }
    _currVal = minVal ;
  }

  void pulse( byte val, int duration )
  {
    analogWrite( _pin, val ) ;
    delay( duration ) ;
    analogWrite( _pin, _currVal ) ;
  }

  void flicker( byte n_times, byte max_time, byte max_value)
  {
    for( byte i = 0 ; i < n_times ; i++ )
    {
      int dly = random( max_time ) ;
      setVal( 0 ) ;
      delay( dly ) ;
      setVal( max_value ) ;
      delay( dly ) ;
    }
    setVal( 0 ) ;
  }
} ;

Dimmer red_dimmer( RED_X) ;
Dimmer left_dimmer( LEFT_YELLOW_ARROW ) ;
Dimmer right_dimmer( RIGHT_YELLOW_ARROW) ;
Dimmer green_dimmer( GREEN_ARROW ) ;

Dimmer* dimmers[4] = { &red_dimmer, &left_dimmer, &right_dimmer, &green_dimmer } ;

// states:  init, light, dark, done
int smooth( int n_values)
{
    int light = CircuitPlayground.lightSensor();
    for( int i = 0 ; i < n_values ; i++ )
    {
        light += CircuitPlayground.lightSensor();
        delay( 100 ) ;
    }
    light = light / n_values ;
    return light ;
}


// return 1 if we should be active, else 0
int run( int on_light_val, int off_light_val, unsigned long on_time, LightTimer* light_timer)
{
    int result = 0 ;
    int smoothed_val = smooth(10) ;
    Serial.print( "light=") ;
    Serial.println( smoothed_val) ;
    if (state == INIT)
    {
        if (smoothed_val > off_light_val)
        {
            //light now, turn off and wait for dark
            state = LIGHT ;
            light_timer->stop() ;
            result = 0 ;
        }
        else if( smoothed_val < on_light_val)
        {
            //dark now, turn on and start timing
            state = DARK ;
            light_timer->start(on_time) ;
            result = 1 ;
            //return 1 ;
        }
        else
        {
          //return 0 ;
          result = 0 ;
        }
    }
    else if ( state == LIGHT)
    {
        if (smoothed_val < on_light_val)
        {
            // dark now, turn on and start timeron_off=0

            state = DARK ;
            light_timer->start(on_time) ;
            result = 1 ;
            //return 1 ;
        }
    }
    else if (state == DARK)
    {
        int expired = light_timer->tick() ;
        if (expired == 1) 
        {
            state = DONE ;
            light_timer->stop() ;
            final_display ( ) ;
            result = 0 ;
            //return 0 ;
        }
        else
        {
            //light_timer.tick() ;
            result = 1 ;
        }
    }
    else if (state == DONE)
    {
        if (smoothed_val > off_light_val)
        {
            state = LIGHT ;
            result = 1 ;
        }
    }
    Serial.print( "state=" ) ;
    Serial.println(state_names[state]) ;
    return result ;
}

Colors marquee_color1 = { 0, 0, 255} ;
Colors marquee_color2 = { 128, 128, 128 };
Colors marquee_color3 = { 128, 0, 0 } ;

void color_marquee( Colors c1, Colors c2 )
{
  static byte odd_even = 0 ;

  for( uint8_t i = 0 ; i < 10; i += 2 )
  {
    if( odd_even )
    {
      CircuitPlayground.setPixelColor(i, c1.red, c1.green, c1.blue );
      CircuitPlayground.setPixelColor(i + 1, c2.red, c2.green, c2.blue );
    }
    else
    {
      CircuitPlayground.setPixelColor(i, c2.red, c2.green, c2.blue  );
      CircuitPlayground.setPixelColor(i + 1, c1.red, c1.green, c1.blue );
    }
  }
  odd_even = !odd_even ;
}

// PIR detector interrupt service routine
void pirIsr( )
{
  pir_trips ++ ;
}

void pulse_each( byte val, int duration, int in_between)
{
  
  for( int i = 0 ; i < 4 ; i++ )
  { 
    dimmers[i]->pulse( val, duration) ;
    delay( in_between ) ;
  }
}

// Shutdown display -- all on, then sucessive fade out
void final_display( )
{
  red_dimmer.setVal( 255 ) ;
  left_dimmer.setVal( 255 ) ;
  right_dimmer.setVal( 255 ) ;
  green_dimmer.setVal( 255 ) ;

  delay( 5000 ) ;

  red_dimmer.rampDown( 10, 0, 250 ) ;
  red_dimmer.setVal( 0 ) ;

  delay(1000) ;

  left_dimmer.rampDown( 10, 0, 250 ) ;
  left_dimmer.setVal( 0 ) ;

  delay( 1000 ) ;

  right_dimmer.rampDown( 10, 0, 250 ) ;
  right_dimmer.setVal( 0 ) ;

  delay( 1000 ) ;
  green_dimmer.rampDown( 10, 0, 250 ) ;
  green_dimmer.setVal( 0 ) ;

}

// Startup display -- runs everything n_times
void initial_display( int n_times )
{
  Serial.println( "In final_display") ;
  for( int i = 0 ; i < n_times ; i++ )
  {
    red_dimmer.setVal( 255 ) ;
    left_dimmer.setVal( 255 ) ;
    right_dimmer.setVal( 255 ) ;
    green_dimmer.setVal( 255 ) ;

    delay(2000) ;

    red_dimmer.setVal( 0 ) ;
    left_dimmer.setVal( 0 ) ;
    right_dimmer.setVal( 0 ) ;
    green_dimmer.setVal( 0 ) ;

    delay(2000) ;

    green_dimmer.flicker( 10, 100, 32) ;
    red_dimmer.flicker( 10, 200, 32 ) ;
    right_dimmer.flicker( 10, 200, 128 ) ;
    left_dimmer.flicker( 20, 50, 32 ) ;

    delay(2000) ;

    pulse_each( 128, 1000, 200) ;
    delay( 2000 ) ;
    Serial.println( "Done final display") ;
  }
}

// Flicker randomly chosen light
void randomFlicker( )
{
  int dimmer_num = ( int ) random( 4);
  dimmers[dimmer_num]->flicker( 10, 100, 32) ;
}

// Wig-Wag randomly chosen pair of lights
void wigWag( int n_times, int duration)
{
  int dimmer1 = (int) random( 4 ) ;
  int dimmer2 = dimmer1 ;
  for( int i = 0 ; i < 10 && dimmer1 == dimmer2 ; i++ )
  {
    dimmer2 = ( int ) random(4) ;
  }

  for( int i = 0 ; i < n_times ; i++ )
  {
    dimmers[dimmer1]->setVal( 255 ) ;
    delay( duration ) ;
    dimmers[dimmer2]->setVal( duration ) ;
    dimmers[dimmer1]->setVal( 0 ) ;
    delay(duration);
    dimmers[dimmer2]->setVal( 0 ) ;
  }
    
}

// 6 hours active after dark.
LightTimer light_timer(1000 * 60 * 360) ;

void setup() {
  // put your setup code here, to run once:
   // Init NeoPixels
  CircuitPlayground.begin();
  Serial.begin(9600) ;
  
  // interrupt is used to catch PIR trips
  attachInterrupt( digitalPinToInterrupt( PIR_PIN ), pirIsr, RISING ) ;

  Serial.println( "Starting") ; 
  delay(1000) ;

  // Initial test display
  initial_display( 3 ) ;
 
}
static unsigned long run_time = 5L * 1000L * 60L * 60L;

void loop() {
  // put your main code here, to run repeatedly:
  int on_off = run(40, 400, run_time, &light_timer) ;
  Serial.print( "on_off=") ;
  Serial.println( on_off ) ;
  if( on_off == 1)
  {
      color_marquee( marquee_color3, marquee_color2 );
      if( pir_trips )
      {
        Serial.print( "PIR trips =") ;
        Serial.println( pir_trips ) ;
        pir_trips = 0 ;

        int effect_num = ( int ) random(2) ;
        if( effect_num == 0 )
        {
          randomFlicker( ) ;
        }
        else
        {
          wigWag( 5, 250) ;
        }
    
    }
  }
  else
  {
    color_marquee( marquee_color1, marquee_color2 );
  }

 

  delay( 1000 ) ;
  
}
