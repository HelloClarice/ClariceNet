#!/usr/bin/env python

import subprocess, time, json, os, sys
from xml.dom import minidom

def getChildrenByTagName(node, tagName):
    for child in node.childNodes:
        if child.nodeType==child.ELEMENT_NODE and (tagName=='*' or child.tagName==tagName):
            yield child

def export():
	# export data from rrd as xml
	data = subprocess.check_output(["rrdtool xport -s now-15m -e now --step 3 DEF:a=/home/pi/clarice/clarice.rrd:sensor1:AVERAGE DEF:b=/home/pi/clarice/clarice.rrd:sensor2:AVERAGE XPORT:a:'sensor1' XPORT:b:'sensor2' --enumds"], shell=True)
	# print data

	# json syntax is:
	# { about: 'RRDtool xport JSON output',
	#   meta: {
	#     start: 1388868876,
	#     step: 9,
	#     end: 1388868876,
	#     legend: [
	#       'mem used',
	#       'mem free'
	#           ]
	#      },
	#   data: [
	#     [ null, null ],
	#     [ 1.5265982429e+05, 3.5624175707e+04 ],
	#     [ 1.5264174186e+05, 3.5642258141e+04 ],
	# 	...
	#     [ null, null  ]
	#   ]
	# }

	# xml syntax is:
	# <xport>
	# 	<meta>
	# 		<start>1388869740</start>
	# 		<step>9</step>
	# 		<end>1388869740</end>
	# 		<rows>401</rows>
	# 		<columns>2</columns>
	# 		<legend>
	#   		<entry>mem used</entry>
	#   		<entry>mem free</entry>
	# 		</legend>
	# 	</meta>
	# 	<data>
    #		<row><t>1388859588</t><v0>1.6190174283e+05</v0><v1>2.6382257170e+04</v1></row>
    #		<row><t>1388859597</t><v0>1.6189991007e+05</v0><v1>2.6384089932e+04</v1></row>
    #		<row><t>1388859606</t><v0>NaN</v0><v1>NaN</v1></row>
    #		<row><t>1388859615</t><v0>NaN</v0><v1>NaN</v1></row>
	#	</data>
	# </xport>

	xmldoc = minidom.parseString(data)
	legends = xmldoc.getElementsByTagName('entry')
	datasequences = []
	for legend in legends:
		# print legend.firstChild.data
		datasequences.append({"title":legend.firstChild.data, "refreshEveryNSeconds":"15", "datapoints":[]})

	data = xmldoc.getElementsByTagName('row')
	# print len(data)
	for row in data :
		t = list(getChildrenByTagName(row, 't'))[0].firstChild.data
		for i in range(0, len(legends)) :
			val = list(getChildrenByTagName(row, 'v' + str(i)))[0].firstChild.data
			if val == 'NaN':
				continue
			# print t, val
			datasequences[i]["datapoints"].append({"title":t, "value":int(float(val))})

	output = json.dumps({"graph":{"title":"Clarice", "type":"line", "datasequences":datasequences}})
	with open("/home/pi/output/data.json", "w") as f:
		f.write(output)
		f.close()

def main(argv):
	while True:
		export()

		# repeat every 3 secs
		time.sleep(3)

if __name__ == "__main__":
   main(sys.argv[1:])