#include "Arduino.h"

/**
* Button data
* Buttons have a value (closed = HIGH, open = LOW). 
* 
* LED State
* Onced pushed, a button needs an acknowledgement. LEDs are tied 
* tightly to the button a BlinkCycle is used to manage that
*
* led state (HIGH = on, LOW = off) is managed with ledState
*/
typedef struct { 
  // last read button pin state.
  int pinState;
  // start off with 0, if pressed goes to 1, when pit responds with 1, go to 2
  int carState;
  // start with 0, just reflect whatever the pit says
  int pitState;
} Button;

/**
* LCD data
* This allows the showMessage method to break up the 'message' array into lines
* or a single line can be manipulated and shown without parsing. The message 
* array is only parsed if 'newMessage' is '1'.
**/
const int lcdMessageLength = 80;
const int rows = 4;
const int columns = 20;
typedef struct { 
  char message[lcdMessageLength];
  
  //has to be 21, so we can end with \0
  //otherwise this memory all runs together 
  //for the LCD, causing duplicate rows
  char line1[columns+1];
  char line2[columns+1];
  char line3[columns+1];
  char line4[columns+1];
  
  long startupMessageTime;
  boolean started;
  
  //set when a new message has been added
  int newMessage;
  
  //has car responded to message from pit
  int responded;
  
  //state of the LCD Backlight
  int light;
  
} LCDDisplay;

void resetLCD(LCDDisplay *lcdDisplay)
{
  lcdDisplay->newMessage = 0;
  lcdDisplay->responded = 1;
  lcdDisplay->message[0] = '\0';
  lcdDisplay->line1[0] = '\0';
  lcdDisplay->line2[0] = '\0';
  lcdDisplay->line3[0] = '\0';
  lcdDisplay->line4[0] = '\0';
}

void setLine(LCDDisplay *lcdDisplay, char line[], int lineNumber)
{
  switch (lineNumber) {
    case 0:
      strcpy(lcdDisplay->line1, line);
      break;
    case 1:
      strcpy(lcdDisplay->line2, line);
      break;
    case 2:
      strcpy(lcdDisplay->line3, line);
      break;
    default: 
      strcpy(lcdDisplay->line4, line);
  }
}

void writeLine(LiquidCrystal *lcd, char *line, int lineNumber)
{
  lcd->setCursor(0, lineNumber);
  lcd->print(line);
}

void clearLine(LCDDisplay *lcdDisplay, int lineNumber)
{
  char line[columns+1];
  for (int i = 0; i < columns; i++) {
    line[i] = ' ';
  }
  line[columns]='\0';
  
  setLine(lcdDisplay, line, lineNumber);
}

/**********************************************************************************
* Break down an 80 char message for appropriate display on the LCD. Easier to 
* manage for data coming back and startup screen. Changing lines in the lcdDisplay
* struct and then calling this will update the display
**********************************************************************************/
void showMessage(LiquidCrystal *lcd, LCDDisplay *lcdDisplay)
{
  //lcd->clear();
    
  if (lcdDisplay->newMessage == 1) {
    //convert to line by line message for display below
    int msgPos = 0;
    for (int i = 0; i < rows; i++) {
      char line[columns+1];
      line[0]='\0';
      
      int lineLen = 0;
      for (int j = 0; j < columns; j++) {
        if ((msgPos <= lcdMessageLength) && (lcdDisplay->message[msgPos]!='\0')) {
          line[j]=lcdDisplay->message[msgPos];
          msgPos++;
          lineLen++;
        }
      }
      //message may have been shorter than whole line, so terminate
      line[lineLen] = '\0';
      setLine(lcdDisplay, line, i);
    }
    
    lcdDisplay->newMessage=0;
    lcd->clear();
  }
  
  //for each line in lcd display, show it.
  writeLine(lcd, lcdDisplay->line1, 0);
  writeLine(lcd, lcdDisplay->line2, 1);
  writeLine(lcd, lcdDisplay->line3, 2);
  writeLine(lcd, lcdDisplay->line4, 3);
}


/**
* Blinking LEDs and LCDs follow some of the same basic functionality, which are encapsulated here
* 
* Blinking has a duration on / off cycle (the blink) and an end state (like LED on or off).
*
* All durations are in ms
**/
typedef struct {
  // how many millis should the light be on
  unsigned long onDuration;
  // how many millis should the light be off
  unsigned long offDuration;

  // store the cycle (on or off) start time for comparison
  unsigned long lastTime;

  // what is the maximum number of cycles (on/off) to walk through
  // 0 for infinite
  int maxCycles;
    
  // on / off
  int endState;
  int currentState;
  
  // for state management
  int currentCycle;
} BlinkCycle;

/**********************************************************************************
* The blinkSpeed is the speed of an on/off cycle. On and Off durations are in ms,
* and could be different (as in LCD)
*
* ignoring the counter causes this to blink perpetually, instead of stopping
* when the button counter hits 0
*
* pass the current time for the cycle to compare
**********************************************************************************/
int blinkLED(BlinkCycle *blinkCycle, unsigned long time)
{
    if (blinkCycle->maxCycles == 0  || blinkCycle->maxCycles >= blinkCycle->currentCycle)
    {
      unsigned long elapsedTime = time - blinkCycle->lastTime;
      // This code block blinks the LED at the Speed
      //basically if we've hit the countdown for the cycle
      //reset and change the state from low to high or reverse
      if (blinkCycle->currentState == HIGH && elapsedTime >= blinkCycle->onDuration)
      {
        blinkCycle->currentState = LOW;
        blinkCycle->lastTime = time;
      }
      else if (blinkCycle->currentState == LOW && elapsedTime >= blinkCycle->offDuration)
      {
        blinkCycle->currentState = HIGH;
        blinkCycle->currentCycle++;
        blinkCycle->lastTime = time;
      }
    }
    else
    {
      // we're past the countdown, turn LED to its end state
      blinkCycle->currentState = blinkCycle->endState;
      blinkCycle->currentCycle = 0;
    }
//    Serial.print("state: on");
//    Serial.print(blinkCycle->onDuration);
//    Serial.print(", off");
//    Serial.print(blinkCycle->offDuration);
//    Serial.print(", max");
//    Serial.print(blinkCycle->maxOut);
//    Serial.print(", mc");
//    Serial.print(blinkCycle->maxCycles);
//    Serial.print(", CC:");
//    Serial.print(blinkCycle->currentCycle);
//    Serial.print(", CD:");
//    Serial.print(blinkCycle->currentDuration);
//    Serial.print(", ");
//    Serial.println(blinkCycle->currentState);
  return blinkCycle->currentState;     
}

int reverseLED(int state)
{
  int result = HIGH;
  if (state == HIGH) {
    result = LOW;
  }
  return result;
}


/**********************************************************************************
* GAUGE
* 
* Gauge data will allow mapping incoming voltage to the gauge ranges. 
* The mapped value is then used to impact the current value of the gauge,
* either through low pass filtering or simple addition.
*
* Remember that current fuel gauge is inverted. Max is lower voltage, so this 
* will return inverted values (75% is actually 25%). So 1-value not value.
**********************************************************************************/
typedef struct {
  //duration to check gauge in millis
  unsigned long duration;
  unsigned long lastCheckTime;
    
  //current gauge values
  float currentValue;
  float LPAlpha;

  //maximum gauge value
  int maxValue;

  //maximum input value
  int maxIn;
  //minimum input value  
  int minIn;
  
  //is max the 'empty' on gauge? then invert it
  boolean inverted;
} Gauge;

float mapToGauge(Gauge *gauge, int value)
{
  float maxGauge = gauge->maxValue;
  float maxIn = gauge->maxIn;
  float minIn = gauge->minIn;
  
  float inDelta = maxIn-minIn;
  float percent = value / inDelta;
  return maxGauge * percent;
}

int readGauge(Gauge *gauge, int analogPin, long currentTime)
{
  //time to take another reading?
  if (currentTime > (gauge->lastCheckTime + gauge->duration))
  {
     int value = 0;
     for (int i = 0; i < 1; i++)
     {
       int thisRead = analogRead(analogPin);
       value = value + thisRead;
//       Serial.print(" read ");
//       Serial.print(i);
//       Serial.print(":");
//       Serial.print(thisRead);
     }
     value = value / 1;
//     Serial.print("avg:");
//     Serial.println(value);
    
     float mappedValue = mapToGauge(gauge, value);
     
     //cleanup
     if (mappedValue > gauge->maxValue)
     {
       mappedValue = gauge->maxValue;
     } 
     else if (mappedValue < 0)
     {
       mappedValue = 0;
     }
     
     if (gauge->inverted)
     {
//       Serial.print("inverting:");
//       Serial.print(gauge->maxValue);
//       Serial.print(" mapped:");
//       Serial.print(mappedValue);
       mappedValue = gauge->maxValue - mappedValue;
//       Serial.print(" newmapped:");
//       Serial.println(mappedValue);
     }

     //apply low pass method
     gauge->currentValue = gauge->currentValue - (gauge->LPAlpha * (gauge->currentValue - mappedValue));

//     Serial.print("read value:");
//     Serial.print(value);
//     Serial.print(" mapped value:");
//     Serial.print(mappedValue);
//     Serial.print(" max/min in: (");
//     Serial.print(gauge->maxIn);
//     Serial.print("/");
//     Serial.print(gauge->minIn);
//     Serial.print(") maxGaugeValue:");
//     Serial.print(gauge->maxValue);
//     Serial.print(" gauge:");
//     Serial.println(gauge->currentValue);
     
     //cleanup
     if(gauge->currentValue > gauge->maxValue)
     {
       gauge->currentValue = gauge->maxValue;
     }
     if(gauge->currentValue < 0)
     {
       gauge->currentValue = 0;
     }
     gauge->lastCheckTime = currentTime;
  }
  return lround(gauge->currentValue);
}
