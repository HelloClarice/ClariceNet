#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <Messenger.h>

#include "ClariceComponents.h"


/**
General communication starts being read from the serial connection which results in:
 - If there are no acknowledgements received or previous button pushes sent
     - then display sensor data
 - If there are previous button pushes sent, but no acknowledgements received
     - blink button LED
     - display sensor data
 - If there are previous button pushes sent, and acknowledgments are received
     - turn off corresponding button LED
     - show LCD text acknowledging
 - If a message is received
     - blink LCD
     - show message until acknowledged (yes or no)
     - send acknowledgement
- Always send sensor data
- Always send button state

From the info above, the format for data should look like:
- SENT:
 - sensor data
 - button data (flagged, wreck, pit, gas, temp, engine, brakes, tires, beer)
 - message yes
 - message no

- RECEIVED:
 - button ack
 - message for driver
  
**/

/**
* common values like LED blink lengths
**/
const int LED_SHORT = 2;
const int LED_MEDIUM = 6;
const int LED_LONG = 9;
const int LCD_ACK = 20;

/**
* serial Communication via XBee
**/
SoftwareSerial mySerial(10,11); // RX, TX
Messenger messenger = Messenger('|'); 
void messageReady();

/**
* LCD Data
* bloody good: http://www.hacktronics.com/Tutorials/arduino-character-lcd-tutorial.html
**/
LiquidCrystal lcd(12, 7, 6, 5, 4, 3, 2);
int lcdBackLight = 13;    // pin 13 will control the backlight
const int LCD_CYCLE_SPEED = LED_SHORT;
LCDDisplay lcdDisplay = {"", " Hello, Clarice....", "   Tell me about","    the silence ","    of the  cams", 100, 0, 1, HIGH};
BlinkCycle lcdBlink = {LCD_ACK, 1, true, 50, HIGH, HIGH, 0, 0};

/**
* comms button data
**/
const int COMM_BUTTON_COUNT = 10;
int commButtonPins[COMM_BUTTON_COUNT] = {22, 24, 26, 28, 30, 32, 34, 36, 38, 40};
int commLEDPins[COMM_BUTTON_COUNT] = {23, 25, 27, 29, 31, 33, 35, 37, 39, 41};
Button commButtons[COMM_BUTTON_COUNT];
BlinkCycle ledAck = {LED_MEDIUM, LED_MEDIUM, false, 0, HIGH, HIGH, 0, 0};
BlinkCycle yesnoBlink = {LED_SHORT, LED_SHORT, false, 0, LOW, HIGH, 0, 0};
BlinkCycle yesnoAck = {LED_LONG, 1, true, 2, LOW, HIGH, 0, 0};

/**
* raw data from sensors
**/
const int COMM_SENSOR_COUNT = 4;
int sensors[COMM_SENSOR_COUNT] = {123, 456, 678, 900}; //MOCK DATA

/**
Application state. Describe status of acknowledgements, what display should show, which buttons should be lit, etc
**/
// number of buttons,   number separators, length of message
//const int inputLength = COMM_BUTTON_COUNT + COMM_BUTTON_COUNT + lcdMessageLength; 
int noDataReceived = 0;
int lastSentDataTime = 0;

/**********************************************************************************
* Initialize the LCD and communication
**********************************************************************************/
void setup() 
{
    for (int i=0; i<COMM_BUTTON_COUNT; i++) {
      Button b = {LOW, 0, 0};
      commButtons[i] = b;
      pinMode(commLEDPins[i], OUTPUT);
    }
    
    // columns, rows.  use 16,2 for a 16x2 LCD, etc.
    lcd.begin(columns, rows);
    showMessage(&lcd, &lcdDisplay);
    
    pinMode(lcdBackLight, OUTPUT);
    digitalWrite(lcdBackLight, lcdDisplay.light);    
    
    Serial.begin(9600);
    setupXbee();
}

void setupXbee()
{
  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
  
  // Force display of bootloader menu
  mySerial.println("\r");
  // Read bootloader menu before we can respond
  while (mySerial.available()) {
    Serial.write(mySerial.read());
  }
  
  // Force Bypass mode
  mySerial.println("B");
  
  // Now we can communicate with remote host
//  Serial.println("Hello, Hannibal?");
//  mySerial.println("Hello, Hannibal?");
  
  // Attach the callback function to the Messenger
  messenger.attach(messageReady);
}

long hash(long sum) 
{
   return ((-1 * sum) % 256) & 0xFF;
}

long calculateChecksum(String input)
{
  long checksum = 0;
  int i = 0;
  char c;
  while ((c = input[i++]) != '\0') {
    checksum += (int)c;
  }
  return hash(checksum);
}

long calculateChecksum(const char* input)
{
  long checksum = 0;
  char* p = (char*)input;
  while (*p != '\0') {
    checksum += (int)*p;
    ++p;
  }
  return hash(checksum);
}

/**********************************************************************************
* Read incoming serial data and marshal into the input array
**********************************************************************************/

void messageReady() 
{
//  Serial.print("Message received. Lag: ");
//  Serial.println(noDataReceived);
  noDataReceived = 0;
  
  // extract checksum
  long checksum = messenger.readLong();
  
  // verify message checksum
  char line[lcdMessageLength];
  messenger.copyString(line, lcdMessageLength);
  long calc_checksum = calculateChecksum(line); 
  if (checksum != calc_checksum) {
    Serial.print("invalid checksum for new line ");
    Serial.print(checksum);
    Serial.print("/");
    Serial.print(calc_checksum);
    Serial.print("/");
    Serial.println(line);
    return;
  }
  
  // read button states as seen by pit in a temp value
  int states[COMM_BUTTON_COUNT];
  for (int i=0; i<COMM_BUTTON_COUNT; i++) {
      if (!messenger.available()) {
        Serial.println("invalid line");
        return;
      }
      states[i] = messenger.readInt();
  }
  
  // update button pit states from received message
  for (int i=0; i<COMM_BUTTON_COUNT; i++) {
    commButtons[i].pitState = states[i];
  }
  
  // parse question if any was added to the message
  char question[lcdMessageLength];
  boolean messageReceived = false;
  if (messenger.available()) { 
    messenger.readString(question, lcdMessageLength);
    messageReceived = true;
  }
    
  if (!messageReceived || question[0] == 0) {
//    Serial.println("No new question.");
    return;
  }
  
  // parse question
  boolean same = true;  
  int i = 0;
  while (i<lcdMessageLength) {
    // update UI state if question is new
    if (lcdDisplay.message[i] != question[i]) {
      same = false;
      lcdDisplay.responded = 0;
      lcdDisplay.newMessage = 1;
      commButtons[8].carState = 0;
      commButtons[9].carState = 0;
    }
    
    lcdDisplay.message[i] = question[i];
    if (question[i] == '\0') 
      break;
    
    i++;
  } 
  
  // null terminate
  lcdDisplay.message[lcdMessageLength-1] = '\0';
  
  if (!same) {
    Serial.print("New question received: ");
    Serial.println(question);
    Serial.print("Message: ");
    Serial.println(line);
  }
}

/**********************************************************************************
* Send sensor data and button data to the Serial
**********************************************************************************/
void sendData() 
{
  String output = "";
  for (int i=0; i<COMM_SENSOR_COUNT; i++) {
    if (i>0) {
      output += "|";
    }
    output += sensors[i];
  }
  for (int i=0; i<COMM_BUTTON_COUNT; i++) {
    if (i>=0 && output.length() > 0) {
      output += "|";
    }
    output += commButtons[i].carState;
  }
  
  // prepend checksum
  long checksum = calculateChecksum(output);
  output = String(checksum) + "|" + output;
  
  Serial.println("Sending: " + output);
  mySerial.println(output);
}

/**********************************************************************************
* This dictates overall LCD behavior, such as blinking the backlight and determining
* if lines are changed.
**********************************************************************************/
void writeLCD()
{
  // if still in startup mode (showing nice text), 
  // just skip touching the LCD
  if (lcdDisplay.startupCount > 0) {
    lcdDisplay.startupCount--;
    //reset the message
    if (lcdDisplay.startupCount == 0) {
      resetLCD(&lcdDisplay);
      lcd.clear();
    }
  }
  
  //Show non startup message
  if (lcdDisplay.startupCount <= 0) {
    //if waiting for a response, need to occasionally blink the LCD
    if (lcdDisplay.responded == 0) {
       digitalWrite(lcdBackLight, blinkLED(&lcdBlink));
       int yesState = blinkLED(&yesnoBlink);
       int noState = reverseLED(yesState);
       digitalWrite(commLEDPins[8], yesState);
       digitalWrite(commLEDPins[9], noState);
    }
    else {
      digitalWrite(lcdBackLight, lcdDisplay.light);    
      //reset this afer a question
      lcdBlink.currentCycle = 0;
      //show regular state information
      String text1="First sensor data";
      text1.toCharArray(lcdDisplay.line1, 21);
      String text2="Second sensor data";
      text2.toCharArray(lcdDisplay.line2, 21);
      
      //show comms issues if there are some
      if (noDataReceived > 10) {
        String text="No pit data: ";
        String toShow = text + noDataReceived;
        toShow.toCharArray(lcdDisplay.line4, 21);
      }
      else {
        clearLine(&lcdDisplay, 3);
      }
    }
    showMessage(&lcd, &lcdDisplay);
  }
}


/**********************************************************************************
* need to integrate into button list and ack list
**********************************************************************************/
void checkButtons()
{
   int buttonAckState = blinkLED(&ledAck);
   int yesNoAckState = blinkLED(&yesnoAck);
  
   for (int i = 0; i < COMM_BUTTON_COUNT; i++) {
      // if we received a pit ack, change car state to 2
      if (commButtons[i].pitState == 1 && commButtons[i].carState == 1) {
        commButtons[i].carState = 2;
      }

      // read the state of the pushbutton value:
      //need to go through each one, then print state
      //for the LED in the array
      int currentPinState = digitalRead(commButtonPins[i]);    
      if (currentPinState == HIGH && commButtons[i].pinState == LOW)  {  
        //if this is an LCD response, let the LCD know
        if (i==8 || i==9) {
          lcdDisplay.responded = 1;
        }
        
        //start with no pushed button
        if (commButtons[i].carState == 0) {
          commButtons[i].carState = 1;
          commButtons[i].pitState = 0;          
        }
        //Stop sending, we don't care and are resetting
        else if (commButtons[i].carState == 1) {
          commButtons[i].carState = 0;
        }
        //only reset if there is a pit ack
        else if (commButtons[i].carState == 2 && commButtons[i].pitState > 0) {
          commButtons[i].carState = 0;
          commButtons[i].pitState = 0;          
        }
      } 
      commButtons[i].pinState = currentPinState;
      
      //if we're showing a button push, but havent received an acknowledgement, show blinking LED
      if (commButtons[i].carState == 1) {
        digitalWrite(commLEDPins[i], buttonAckState);
      }
      //we've received an acknowledgement, now note that with solid
      else if (commButtons[i].carState == 2) {
        //yes/no button? 
        if (i==8 || i==9) {
          digitalWrite(commLEDPins[i], LOW);
          commButtons[i].carState = 0;
        }
        else {
          digitalWrite(commLEDPins[i], HIGH);
        }
      }
      else {
        // turn LED off:
        digitalWrite(commLEDPins[i], LOW); 
      }
   }
}

/**********************************************************************************
* Receive data if it's the appriopriate time, check the buttons (and LEDs) and 
* write to the LCD, then send data
**********************************************************************************/
void loop()
{
  // The following line is the most effective way of using Serial and Messenger's callback
  while (mySerial.available())  {
    messenger.process(mySerial.read());
  }
  
  //add all the buttons here
  checkButtons();
  writeLCD();
  
  // send new data every sec only
  int now = millis();
  if (now-lastSentDataTime >= 1000) {
    sendData();
     
    lastSentDataTime = now;
    noDataReceived++; // this gets reset when new message is received
  }
  delay(10);
}
 