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
const int LEDshort = 2;
const int LEDmedium = 6;
const int LEDlong = 9;
const int LCDack = 20;
const int commCount = 20;
int currentCommCount = commCount;
int counter = 0;

/**
* Serial
**/
SoftwareSerial mySerial(10,11); // RX, TX
Messenger messenger = Messenger(','); 
void messageReady();

/**
* LCD Data
* bloody good: http://www.hacktronics.com/Tutorials/arduino-character-lcd-tutorial.html
**/
LiquidCrystal lcd(12, 7, 6, 5, 4, 3, 2);
int lcdBackLight = 13;    // pin 13 will control the backlight
const int LCDcycleSpeed = LEDshort;
LCDDisplay lcdDisplay = {"", " Hello, Clarice....", "   Tell me about","    the silence ","    of the  cams", 100, 0, 1, HIGH};
BlinkCycle lcdBlink = {LCDack, 1, true, 50, HIGH, HIGH, 0, 0};

/**
* comms button data
**/
const int commButtonCount = 10;
int commButtonPins[commButtonCount] = {22,24,26,28,30,32,34,36,38,40};
int commLEDPins[commButtonCount] = {23,25,27,29,31,33,35,37,39,41};
Button commButtons[commButtonCount];
const int recvdAckCount = 50;
BlinkCycle ledAck = {LEDmedium, LEDmedium, false, 0, HIGH, HIGH, 0, 0};
BlinkCycle yesnoBlink = {LEDshort, LEDshort, false, 0, LOW, HIGH, 0, 0};
BlinkCycle yesnoAck = {LEDlong, 1, true, 2, LOW, HIGH, 0, 0};

/**
* raw data from sensors
**/
const int sensorCount = 4;
//MOCK DATA
int sensors[sensorCount] = {123, 456, 678, 900};

/**
Application state. Describe status of acknowledgements, what display should show, which buttons should be lit, etc
**/
// number of buttons,   number separators, length of message
const int inputLength = commButtonCount + commButtonCount + lcdMessageLength; 
int noDataReceived = 0;

/**********************************************************************************
* Initialize the LCD and communication
**********************************************************************************/
void setup() 
 {
    for (int i=0; i<commButtonCount; i++)
    {
      Button b = {0, 0};
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

/**********************************************************************************
* Read incoming serial data and marshal into the input array
**********************************************************************************/

boolean verifyChecksum(const char* message)
{
  Serial.print("Calculating checksum for line: ");
  Serial.println(message);
  
  // Separate checksum from message
  char *p = (char*)message;
  char *str;
  if ((str = strtok_r(p, ";", &p)) == NULL) // delimiter for checksum is the semicolon
    return false;
    
//  Serial.print("Message: ");
//  Serial.println(str);
  
  // calculate checksum from message
  int checksum = 0;
  int i = 0;
  char c;
  while ((c = str[i++]) != 0) {
    checksum += (int)c;
  }
  checksum = ((-1 * checksum) % 256) & 0xFF;

//  Serial.println(checksum);
    
  // Extract checksum from line
  str = strtok_r(p, ";", &p);
  if (!str) 
  {
    Serial.println("Found checksum delimiter but no checksum");
    return false;
  }
  
//  Serial.print("Line checksum: ");
//  Serial.println(str);
  
  // Compare checksum
  return (checksum == atoi(str)) ? true : false;
}

void messageReady() {
//  Serial.print("Message received. Lag: ");
//  Serial.println(noDataReceived);
  noDataReceived = 0;
  
  char line[lcdMessageLength];
  messenger.copyBuffer(line, lcdMessageLength);
  if (!verifyChecksum(line))
  {
    Serial.print("invalid checksum for new line: ");
    Serial.println(line);
    return;
  }
  
  // read button states as seen by pit in a temp value
  int states[commButtonCount];
  for (int i=0; i<commButtonCount; i++) 
  {
      if (!messenger.available()) {
        Serial.println("invalid line");
        return;
      }
      states[i] = messenger.readInt();
  }
  
  // update all states
  for (int i=0; i<commButtonCount; i++) 
  {
    commButtons[i].pitState = states[i];
  }
  
  // read question if any
  char message[lcdMessageLength];
  boolean messageReceived = false;
  if (messenger.available()) 
  { 
    messenger.copyString(message, lcdMessageLength);
    messageReceived = true;
    // TODO handle comma
  }
    
  char* p = message;
  char *str = strtok_r(p, ";", &p); // delimiter for checksum is the semicolon
  if (!messageReceived || message[0] == 0 || message[0] == ';' || !str || str[0] == 0) 
  {
//    Serial.println("No new question.");
    return;
  }
  
  boolean same = true;  
  int i = 0;
  while (i<lcdMessageLength && str[i] != 0) 
  {
    if (lcdDisplay.message[i] != str[i] && same)
    {
      same = false;
      lcdDisplay.responded = 0;
      lcdDisplay.newMessage = 1;
      commButtons[8].carState = 0;
      commButtons[9].carState = 0;
    }
    lcdDisplay.message[i] = str[i];
    i++;
  } 
  
  if (!same)
  {
    Serial.print("New question received: ");
    Serial.println(str);
    Serial.print("Message: ");
    Serial.println(message);
  }
  
  if (i > 0 && i <= lcdMessageLength)
  {
    lcdDisplay.message[i] = '\0'; // Null terminate the string
  }
}


/**********************************************************************************
* Send sensor data and button data to the Serial
**********************************************************************************/
void sendData() 
{
  String output = "";
  for (int i=0; i<sensorCount; i++) 
  {
    output += sensors[i];
    output += ",";
  }
  for (int i=0; i<commButtonCount; i++) 
  {
    output += commButtons[i].carState;
    output += ",";
  }
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
  if (lcdDisplay.startupCount > 0)
  {
    lcdDisplay.startupCount--;
    //reset the message
    if (lcdDisplay.startupCount == 0)
    {
      resetLCD(&lcdDisplay);
      lcd.clear();
    }
  }
  
  //Show non startup message
  if (lcdDisplay.startupCount <= 0)
  {
    //if waiting for a response, need to occasionally blink the LCD
    if (lcdDisplay.responded == 0)
    {
       digitalWrite(lcdBackLight, blinkLED(&lcdBlink));
       int yesState = blinkLED(&yesnoBlink);
       int noState = reverseLED(yesState);
       digitalWrite(commLEDPins[8], yesState);
       digitalWrite(commLEDPins[9], noState);
    }
    else
    {
      digitalWrite(lcdBackLight, lcdDisplay.light);    
      //reset this afer a question
      lcdBlink.currentCycle = 0;
      //show regular state information
      String text1="First sensor data";
      text1.toCharArray(lcdDisplay.line1, 21);
      String text2="Second sensor data";
      text2.toCharArray(lcdDisplay.line2, 21);
      
      //show comms issues if there are some
      if (noDataReceived > 10)
      {
        String text="No pit data: ";
        String toShow = text + noDataReceived;
        toShow.toCharArray(lcdDisplay.line4, 21);
      }
      else
      {
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
  
   for (int i = 0; i < commButtonCount; i++)
   {
      //if we received a pit ack, change car state to 2
      if (commButtons[i].pitState == 1 && commButtons[i].carState == 1)
      {
        commButtons[i].carState = 2;
      }

      // read the state of the pushbutton value:
      //need to go through each one, then print state
      //for the LED in the array
      int buttonState = digitalRead(commButtonPins[i]);    
      if (buttonState == HIGH) 
      {  
        //stop this up a bit so we can process and know
        //we're not processing the same push again
        while (digitalRead(commButtonPins[i]) == HIGH)
        {
          delay(50);
        }
        //a bit more, just to keep it straight
        delay(50);

        //if this is an LCD response, let the LCD know
        if (i==8 || i==9)
        {
          lcdDisplay.responded = 1;
        }
        
        //start with no pushed button
        if (commButtons[i].carState == 0)
        {
          commButtons[i].carState = 1;
          commButtons[i].pitState = 0;          
        }
        //Stop sending, we don't care and are resetting
        else if (commButtons[i].carState == 1)
        {
          commButtons[i].carState = 0;
        }
        //only reset if there is a pit ack
        else if (commButtons[i].carState == 2 && commButtons[i].pitState > 0)
        {
          commButtons[i].carState = 0;
          commButtons[i].pitState = 0;          
        }
      } 
      
     //if we're showing a button push, but havent received an acknowledgement, show blinking LED
     if (commButtons[i].carState == 1)
     {
         digitalWrite(commLEDPins[i], buttonAckState);
     }
     //we've received an acknowledgement, now note that with solid
     else if (commButtons[i].carState == 2)
     {
        //yes/no button? 
        if (i==8 || i==9)
        {
          digitalWrite(commLEDPins[i], LOW);
          commButtons[i].carState = 0;
        }
        else
        {
          digitalWrite(commLEDPins[i], HIGH);
        }
     }
     else 
     {
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
  currentCommCount--;
  
  // The following line is the most effective way of using Serial and Messenger's callback
  while (mySerial.available())  {
    messenger.process(mySerial.read());
  }
  
  //add all the buttons here
  checkButtons();
  writeLCD();
  
  if (currentCommCount <= 0)
  {
    //this gets reset when processed by messenger
     noDataReceived++;
     currentCommCount = commCount;
     sendData();
  }
  delay(50);
}
 
