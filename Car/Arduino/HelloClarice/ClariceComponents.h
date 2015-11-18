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
  
  int startupCount;

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

void writeLine(LCDDisplay *lcdDisplay, char line[], int lineNumber)
{
  line[columns] = '\0';
  switch (lineNumber) {
    case 0:
      memcpy(lcdDisplay->line1,line,columns);
      break;
    case 1:
      memcpy(lcdDisplay->line2,line,columns);
      break;
    case 2:
      memcpy(lcdDisplay->line3,line,columns);
      break;
    default: 
      memcpy(lcdDisplay->line4,line,columns);
  }
}

void clearLine(LCDDisplay *lcdDisplay, int lineNumber)
{
  char line[columns+1];
  for (int i = 0; i < columns; i++) {
    line[i] = ' ';
  }
  line[columns]='\0';
  writeLine(lcdDisplay, line, lineNumber);
}

/**********************************************************************************
* Break down an 80 char message for appropriate display on the LCD. Easier to 
* manage for data coming back and startup screen. Changing lines in the lcdDisplay
* struct and then calling this will update the display
**********************************************************************************/
void showMessage(LiquidCrystal *lcd, LCDDisplay *lcdDisplay)
{
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
      writeLine(lcdDisplay, line, i);
    }
  }

  if (lcdDisplay->newMessage == 1) {
    lcdDisplay->newMessage=0;
    lcd->clear();
  }
  
  //for each line in lcd display, show it.
  lcd->setCursor(0,0);
  lcd->print(lcdDisplay->line1);
  lcd->setCursor(0,1);
  lcd->print(lcdDisplay->line2);
  lcd->setCursor(0,2);
  lcd->print(lcdDisplay->line3);
  lcd->setCursor(0,3);
  lcd->print(lcdDisplay->line4);
}


/**
* Blinking LEDs and LCDs follow some of the same basic functionality, which are encapsulated here
* 
* Blinking has a duration on / off cycle (the blink) and an end state (like LED on or off).
*
* All durations are actually number of loop counts. So if the loop delays for 50ms, a duration of 2 
* means 2 loops (or 100ms)
**/
typedef struct {
  //how many loops should the light be on
  int onDuration; 
  //how many loops should the light be off
  int offDuration;

  //if there are max cycles, use them
  boolean maxOut;
  //what is the maximum number of cycles (on/off) to walk through
  int maxCycles;
    
  //on / off
  int endState;
  int currentState;
  
  //for state management
  int currentCycle;
  int currentDuration;
} BlinkCycle;

/**********************************************************************************
* The blinkSpeed is the speed of an on/off cycle. The clock goes for 50ms, 
* so a speed of 10 means 50*10ms on, 50*10ms off 
*
* ignoring the counter causes this to blink perpetually, instead of stopping
* when the button counter hits 0
**********************************************************************************/
int blinkLED(BlinkCycle *blinkCycle)
{
    if (!blinkCycle->maxOut || (blinkCycle->maxCycles >= blinkCycle->currentCycle)) {
      blinkCycle->currentDuration++;
        
      // This code block blinks the LED at the Speed
      //basically if we've hit the countdown for the cycle
      //reset and change the state from low to high or reverse
      if (blinkCycle->currentState == HIGH && (blinkCycle->currentDuration >= blinkCycle->onDuration)) {
        blinkCycle->currentState = LOW;
        blinkCycle->currentDuration = 0;
      }
      else if (blinkCycle->currentState == LOW && (blinkCycle->currentDuration >= blinkCycle->offDuration)) {
        blinkCycle->currentState = HIGH;
          
        //increment cycle count if we care about max cycles
        if (blinkCycle->maxOut) {
          blinkCycle->currentCycle++;
        }
        blinkCycle->currentDuration = 0;
      }
    }
    else {
     //we're past the countdown, turn LED off
     blinkCycle->currentState = blinkCycle->endState;
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
  return state == HIGH ? LOW : HIGH;
}
