import webiopi
import subprocess
import json

from urllib.parse import unquote_plus

# import Serial driver
from webiopi.devices.serial import Serial

# Enable debug output
# webiopi.setDebug()
webiopi.setInfo()

# initialize Serial driver
# serial = Serial("ttyAMA0", 9600)
maxSensor = 4
maxButtons = 10
sensors = [0 for a in range(maxSensor)]
carButtons = [0 for a in range(maxButtons)]
pitButtons = [0 for a in range(maxButtons)]
lastQuestion = ""
lastAnswer = ""

#this will store the line
line = b""

def setup():
    # # let it wake up
    # webiopi.sleep(.5)

    # trigger bypass boot menu
    # serial.writeString("\r")
    # webiopi.sleep(.5)

    # # read menu
    # if serial.available() > 0:
    #   webiopi.debug(serial.readString())

    # # force bypass
    # serial.writeString("B")
    return

def calc_checksum(s):
    """
        Calculates checksum for sending commands to the ELKM1.
        Sums the ASCII character values mod256 and takes
        the lower byte of the two's complement of that value.
        """
    return '%d' % (-(sum(ord(c) for c in s) % 256) & 0xFF)

def process(line):
    webiopi.info("Received: " + line)

    params = line.split("|", 1)     # split checksum from message
    if len(params) != 2:
        webiopi.warn("Invalid line: %s " % line)
        return

    checksum = calc_checksum(params[1])
    if params[0] != checksum:
        webiopi.warn("Invalid checksum: %s (%s instead of %s)" % (params[1], checksum, params[0]))
        return

    params = params[1].split("|")
    count = len(params)
    if count < maxSensor+maxButtons:
        webiopi.warn("Invalid line: %s (%d)" % (line, count))
        return

    # process params
    message = ""
    index = 0
    for data in params:
        if index < maxSensor:
            sensors[index] = int(data)  # store sensor value
        elif index < maxSensor+maxButtons:
            carButtons[index-maxSensor] = int(data) # store button state
        else:
            message += "%s|" % data # last value is a string (may contain commas)
        index += 1
    message = message.strip("|")

    # handle question answer using last 2 buttons
    global lastAnswer, lastQuestion
    # debug = "%s|%s" % (lastQuestion, message)
    # webiopi.debug(debug)
    # if lastQuestion != "" and lastQuestion == message:
    if carButtons[maxButtons-2] >= 1:
        lastAnswer = "Yes"
        lastQuestion = ""
    elif carButtons[maxButtons-1] >= 1:
        lastAnswer = "No"
        lastQuestion = ""

    pitButtons[maxButtons-2] = carButtons[maxButtons-2]
    pitButtons[maxButtons-1] = carButtons[maxButtons-1]
    return 0

def getResponse(isPit, includeSensors, outputjson=False):
    message = ""
    buttons = carButtons
    if isPit:
        buttons = pitButtons

    if outputjson:
        if includeSensors:
            return json.dumps({'sensors' : sensors, 'buttons' : buttons, 'question' : lastQuestion})

        return json.dumps({'buttons' : buttons, 'question' : lastQuestion})

    # return sensor values first if requested
    if includeSensors:
        message += '|'.join(map(str, sensors))
        message += '|'

    # return button states and last question
    message += '|'.join(map(str, buttons))

    if lastQuestion:
        message += "|%s" % lastQuestion

    # checksum
    message = "%s|%s" % (calc_checksum(message), message)
    return message

def updatedb():
    values = ':'.join(map(str, sensors))
    webiopi.debug("Updating db with: %s" % values)
    # update rrdtool db
    subprocess.check_output(["rrdtool update /home/pi/clarice/clarice.rrd N:" + values], shell=True)

def loop():
    while True:
        try:
            updatedb()
            webiopi.sleep(1)
        except:
            continue

# Called by WebIOPi at server shutdown
def destroy():
    webiopi.debug("Script with macros - Destroy")

# this macro sets
@webiopi.macro
def setCarInfo(info):
    return process(info.replace("%2C", ","))

@webiopi.macro
def getCarInfo(includeSensors=False, outputjson=False):
    webiopi.debug("getCarInfo (json=%s)" % str(outputjson))
    return getResponse(False, includeSensors, outputjson)

# this macro returns raw sensor value
@webiopi.macro
def getSensor(channel, outputjson=False):
    webiopi.debug("Sensor Channel: %s (json=%s)" % (channel, str(outputjson)))

    index = int(channel)
    if index < 0 or index >= maxSensor:
        return -1

    if outputjson:
        # webiopi.debug("Channel (" + str(index) + ") :" + str(sensors[index-1]))
        return json.dumps({'name': channel, 'value': str(sensors[index])})

    return str(sensors[index])

# this macro returns button state
@webiopi.macro
def getCarButton(channel):
    index = int(channel)
    if index < 0 or index >= maxButtons:
        return -1

    return str(carButtons[index])

@webiopi.macro
def getPitInfo(outputjson=False):
    webiopi.debug("getPitInfo (json=%s)" % str(outputjson))
    return getResponse(True, False, outputjson)

# this macro returns button state
@webiopi.macro
def setPitButton(channel, state):
    index = int(channel)
    if index < 0 or index >= maxButtons:
        return -1

    pitButtons[index] = state

# this macro sends a question
@webiopi.macro
def sendMessage(message):
    unquoted = unquote_plus(str(message))
    webiopi.info("sendMessage: %s" % unquoted)
    global lastQuestion, lastAnswer
    lastQuestion = unquoted
    lastAnswer = ""

# this macro returns message answer
@webiopi.macro
def getAnswer():
    return lastAnswer
