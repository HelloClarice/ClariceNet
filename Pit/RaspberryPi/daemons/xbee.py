#!/usr/bin/env python

"""XBeeModem.py bypasses the XBee's 802.15.4 capabilities and simply uses it modem for communications

	You don't have to master 802.15.4 and a large set of XBee commands
	to make a very simple but potentially useful network.  At its core,
	the XBee radio is  a modem and you can use it directly for simple serial communications.

	Reference Materials:
	Non-blocking read from stdin in python - http://repolinux.wordpress.com/2012/10/09/non-blocking-read-from-stdin-in-python/
	Non-blocking read on a subprocess.PIPE in python - http://stackoverflow.com/questions/375427/non-blocking-read-on-a-subprocess-pipe-in-python

	Originally Created By:
	Jeff Irland (jeff.irland@gmail.com) in March 2013
	"""

# imported modules
# import os                   # portable way of using operating system dependent functionality
import sys                  # provides access to some variables used or maintained by the interpreter
import time                 # provides various time-related functions
# import fcntl                # performs file control and I/O control on file descriptors
# import serial               # encapsulates the access for the serial port
# import urllib

from serial import Serial

# from pretty import switchColor, printc  # provides colored text for xterm & VT100 type terminals using ANSI escape sequences
from webiopi.clients import PiHttpClient, Macro
from webiopi.utils.logger import exception, setDebug, info, debug, logToFile
from webiopi.utils.thread import stop

VERSION = '1.0'

def displayHelp():
	print("Xbee command-line usage")
	print("xbee [-h] [-c config] [-l log] [-d] [port]")
	print("")
	print("Options:")
	print("  -h, --help           Display this help")
	print("  -c, --config file    Load config from file")
	print("  -l, --log file       Log to file")
	print("  -d, --debug          Enable DEBUG")
	print("")
	print("Arguments:")
	print("  port                 WebIOPi port")
	exit()

def main(argv):
	port = 8000
	configfile = None
	logfile = None

	i = 1
	while i < len(argv):
		if argv[i] in ["-c", "-C", "--config-file"]:
			configfile = argv[i+1]
			i+=1
		elif argv[i] in ["-l", "-L", "--log-file"]:
			logfile = argv[i+1]
			i+=1
		elif argv[i] in ["-h", "-H", "--help"]:
			displayHelp()
		elif argv[i] in ["-d", "--debug"]:
			setDebug()
		else:
			try:
				port = int(argv[i])
			except ValueError:
				displayHelp()
		i+=1

	if logfile:
		logToFile(logfile)

	info("Starting XBee %s" % VERSION)

	# setup serial
	serial = Serial()
	serial.port = '/dev/ttyAMA0'
	serial.baudrate = 9600
	serial.timeout = 1
	serial.writeTimeout = 1
	serial.open()

	# disregard any pending data in xbee buffer
	serial.flushInput()

	# force to show xbee boot menu
	time.sleep(.5)
	serial.writelines("\r")
	time.sleep(.5)

	# read menu
	while serial.inWaiting() > 0:
		debug("%s" % serial.readline())

	# trigger bypass automatically
	serial.writelines("B")

	# post startup message to other XBee's and at stdout
	#serial.writelines("RPi #1 is up and running.\r\n")
	info("RPi #1 is up and running.")

	try:
		while True:
			waitToSend = True

			# read a line from XBee and convert it from b'xxx\r\n' to xxx and send to webiopi
			while serial.inWaiting() > 0:
				try:
					line = serial.readline().decode('utf-8').strip('\n\r')
					if line:
						waitToSend = False
						debug("Received: %s" % line)
						try:
							client = PiHttpClient("127.0.0.1")
							macro = Macro(client, "setCarInfo")
							macro.call(line.replace(",", "%2C"))
						except:
							exception("setting car info failed!")

				except KeyboardInterrupt:
					raise
				except Exception as e:
					exception(e)
					time.sleep(1.)

			try:
				time.sleep(1.)

				client = PiHttpClient("127.0.0.1")
				macro = Macro(client, "getPitInfo")
				data = macro.call()
				if data:
					debug("Sending: %s" % data)
					serial.writelines(data + "\n")

			except KeyboardInterrupt:
				raise
			except Exception as e:
				exception(e)
				time.sleep(1.)

	except KeyboardInterrupt:
		info("*** Ctrl-C keyboard interrupt ***")

if __name__ == "__main__":
	try:
		main(sys.argv)
	except Exception as e:
		exception(e)
		stop()
		info("RPi #1 is going down")
