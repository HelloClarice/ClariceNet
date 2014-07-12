ClariceNet
==========

24 Hrs Of Lemons Team "Hello Clarice"  Communication System

When Team Clarice decided to race at the 24 Hrs of Lemons on March 2014, we asked other teams what was most challenging to them and they responded that it was to not being able to clearly communicate with a driver from the pit.

How could we make sure that we could communicate (not necessarily talk) to the driver and the driver communicate back to us in a clear way.

So we sat down and when you're a geek, when there's a problem there's a solution.

We started with the asumption that we would not have any good cell coverage so using 3G/4G was out of the question.
Next WiFi was not going to be strong enough to cover the entire track.
Then we learn about ZigBee or XBee.

The system we put in place works like this

#Car
* Sensors + Buttons on the dash + LCD hooked to Arduino
* Arduino interfacing with an XBee shield
  
#Pit
* XBee shield connected to Raspberry Pi
* WiFi dongle connected to Raspberry Pi configured as access point
* iPad running Status Board connected to Raspberry Pi access point
![Status Board](/rpi+xbee.jpg)
![Status Board](/statusboard.jpg)

#Usage
Pit can monitor sensors in real time and communicate to drivers by typing a question on the iPad.
Driver sees question show up on the LCD in the car and responds with "YES" or "NO" by pressing a button on the Left or Right of the wheel to acknowledge it. Additionally, the driver can press more buttons on the dash to reflect current situations or problems which are immediately reflected on the status board with blinking leds. Pit can acknowledge reception by pressing the corresponding buttons.

#Codebase
* Pi: webpi, python
* Arduino

For more info on our team
https://www.tumblr.com/blog/raceclarice#
