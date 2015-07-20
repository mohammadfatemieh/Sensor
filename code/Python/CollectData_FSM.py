import fysom
import serial
import time

prefix = '  |'
PRINT_COMMANDS = 1
PRINT_RESPONCES = 1
PRINT_STATES = 1

PRIVATE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8E'
PRIVATE_BAT_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8B'
PRIVATE_MODE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8F'

global fsm 
fsm = None

class RN4020:
	def __init__(self, port):
		self.ser = serial.Serial(port, 115200, timeout=0.25)

		# Check if the Module is connected
		self.write('D', 'Dump Info')
		temp = None
		while temp != '':	# Keep reading until a timeout occurs
			temp = self.read()
			splitList = temp.strip().split('=')
			if splitList[0] == 'Connected':
				self.connected = splitList[1]

		self.rebooted = 'no'

		print("RN4020 Connected -> " + self.connected) 

		# If it is connected, disconnect
		if(self.connected != "no"):
			self.disconnect()
	
	def read(self):
		temp = self.ser.readline()
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		# Automatically search for Connections		
		if temp.strip() == 'Connection End':  self.connected = 'no'
		if temp.strip() == 'Connected':  self.connected = 'yes'
		if temp.strip() == 'CMD':  self.rebooted = 'yes'
		return temp

	def write(self, string, comment = ''):
		self.ser.write(string + '\n')
		if PRINT_COMMANDS:
			print("> " + string + "    (" + comment + ")")
		return

	def setTimeout(self, timeout):
		self.ser.timeout = timeout     

	def stopScan(self):
		self.write('X', 'Stop Scan')
		temp = None
		while temp != '':
			temp = self.read().strip()
			if temp == 'AOK':  return(temp)
			if temp == 'ERR':  return(temp)
			if temp == '': return('TIMEOUT')

	def stopConnecting(self):
		self.write('Z', 'Stop Connecting')
		temp = None
		while temp != '':
			temp = self.read().strip()
			if temp == 'AOK':  return(temp)
			if temp == 'ERR':  return(temp)
			if temp == '': return('TIMEOUT')

	def disconnect(self):
		print "Disconnecting \n" 
		self.write('K', 'Disconnect')
		temp = None
		while temp != '':
			temp = self.read().strip()
			if temp == 'Connection End':  return(temp)
			if temp == '': return('TIMEOUT')


btle = RN4020('/dev/ttyUSB0')
MAC = None
MACIgnore = []

fsm = fysom.Fysom({
	'events': [
		{'name': 'startup'         , 'src': 'none'   , 'dst': 'init'},
		{'name': 'done_init'       , 'src': 'init'   , 'dst': 'scan'},
		{'name': 'found_device'    , 'src': 'scan'   , 'dst': 'connect'},
		{'name': 'service_found'   , 'src': 'connect', 'dst': 'collect'},
		{'name': 'no_service_found', 'src': 'connect', 'dst': 'scan'},
		{'name': 'got_data'        , 'src': 'collect', 'dst': 'scan'}
	],
})


def printstatechange(e):
 	if PRINT_STATES == 1:   print '\n======> event: %s, src: %s, dst: %s' % (e.event, e.src, e.dst)

def on_init(e):
	# Get the list of MAC address to ignore
	fIgnore = open('ignore.txt', 'r')
	line = fIgnore.readline()
	while line != '':
		if(len(line.strip()) > 0):
			MACIgnore.append(line.strip())
		line = fIgnore.readline()
	fIgnore.close()
	print MACIgnore
	fsm.done_init()

def on_scan(e):
	global MAC
	foundMAC = 0
	errorCount = 0
	# Keep trying the scan until a device is found
	while foundMAC == 0:
		print("Scanning... ") 
		# Scan for bluetooth devices
		# Set a long timeout to wait for devices
		foundMAC = 0
		btle.setTimeout(120)     # Search for 60 seconds before timing out.
		btle.write('F', 'Scan')
		# Keep reading the serial port to look for found MACs
		temp = None
		while temp != '' and temp != 'ERR' and foundMAC == 0:
			temp = btle.read().strip()
			splitList = temp.split(',')
			if len(splitList) > 1:
				MAC = splitList[0]
	 			# Check to see if it should be ignored
				if MACIgnore.count(MAC) == 0:
					foundMAC = 1
				elif PRINT_RESPONCES == 1: 
					print "     (on ignore list)"

		btle.setTimeout(0.25)

		if temp == 'ERR' and btle.connected == 'yes':
			btle.disconnect()
			errorCount = errorCount + 1
			if errorCount > 5:
				btle.stopConnecting();
				btle.stopScan();
		if temp == '':
			print 'Timeout!!!'

		# Stop the scan if nothing was found during the timeout
		btle.stopScan();

	#if foundMac == 1:
	print "Found Device: " + MAC
	fsm.found_device()

def on_connect(e):
	global MACIgnore
	time.sleep(1.5)  # Pause to make sure Sensor has time to make data availible after turning on
	# Establish a connection
	btle.setTimeout(3)     # Wait up to 3 seconds for a connection.
	print "Connecting " 
	btle.write('E,0,' + MAC, 'Establish Connection')
	temp = None
	while temp != '' and btle.connected == 'no':
		temp = btle.read()

	btle.setTimeout(0.25)

	foundGardenSenseService = 0
	btle.write('LC', 'List Client Services')
	temp = None
	foundGardentSenseService = 0
	while temp != '' and temp != 'END':
		temp = btle.read().strip()
		splitList = temp.strip().split(',')
		if len(splitList) == 1:     # If it's in the group
			if splitList[0] == PRIVATE_SERVICE:
				foundGardenSenseService = 1
		if len(splitList) == 3:     # If it's an individual service
			if splitList[0] == PRIVATE_SERVICE:
				foundGardenSenseService = 1

	# If garden sense stick wasn't found, add the MAC to the ignore list
	if foundGardenSenseService == 0:
		if temp == 'END' and btle.connected == 1:  # If LC succeeded and we're still connected
			# Add it to the ignore list on disk
			fIgnore = open('ignore.txt', 'a')
			fIgnore.write(MAC+'\n')
			fIgnore.close()
			# Add it to the local ignore list	
			MACIgnore.append(MAC)

		btle.disconnect()

		time.sleep(1) # Wait 1 second to give the module time to disconnect
		fsm.no_service_found()
	else:
		fsm.service_found()

def on_collect(e):
	CHOLD = 45   # HOLD Capacitor inside the 16LF1559 in pF
	# Get the data
	print "It's a GardenSense Sensor. "
	print time.ctime()

	sensor = [0,0,0,0]
	voltageRatio = [0,0,0,0]
	voltage = [0,0,0,0]
	cap = [0,0,0,0]

	btle.write('CURV,' + PRIVATE_SERVICE, 'Read Sensor')
	gotSensorData = 0
	temp = None
	while temp != '' and gotSensorData == 0 and btle.connected == 'yes':
		temp = btle.read()
		splitList = temp.strip().strip('.').split(',')
		if len(splitList) == 2 and splitList[0] == 'R' and len(splitList[1]) >= 16:
			gotSensorData = 1
			sensor[0] = int(splitList[1][0:4], 16)
			sensor[1] = int(splitList[1][4:8], 16)
			sensor[2] = int(splitList[1][8:12], 16)
			sensor[3] = int(splitList[1][12:16], 16)
			#print "S0: %.2fV   S1: %.2fV   S2: %.2fV   S3: %.2fV   " % (voltage[0], voltage[1], voltage[2], voltage[3])	
			# Calculate Capacitance		
			for i in range(0,4):
				voltageRatio[i] = sensor[i]/1023.0
				voltage[i] = voltageRatio[i]*3.3
				if(sensor[1] > 0):
					cap[i] = CHOLD*((1-voltageRatio[i])/voltageRatio[i])				
				else:
					cap[i] = 0
			print "S0: %.1fpF  S1: %.1fpF  S2: %.1fpF  S3: %.1fpF  " % (cap[0], cap[1], cap[2], cap[3])
	

	# Get the battery voltage
	btle.write('CURV,' + PRIVATE_BAT_SERVICE, 'Read Battery')
	gotBatData = 0
	Vbat = 0
	temp = None
	while temp != '' and gotBatData == 0 and btle.connected == 'yes':
		temp = btle.read()
		splitList = temp.strip().strip('.').split(',')
		if len(splitList) == 2 and splitList[0] == 'R' and len(splitList[1]) >= 3:
			gotBatData = 1
			readBat = int(splitList[1], 16)
			Vbat = 2.048*1024/readBat
			print "Battery Voltage: %1.2fV" % Vbat

	# Get the timeout
	btle.write('CURV,' + PRIVATE_MODE_SERVICE, 'Read Timeout')
	gotTimeoutData = 0
	sensorTimeout = 0
	temp = None
	while temp != '' and gotTimeoutData == 0 and btle.connected == 'yes':
		temp = btle.read()
		splitList = temp.strip().strip('.').split(',')
		if len(splitList) == 2 and splitList[0] == 'R' and len(splitList[1]) >= 1:
			gotTimeoutData = 1
			sensorTimeout = int(splitList[1], 16) * 5
			print "Sensor Timeout : %d sec" % sensorTimeout

	fTimeout = open('timeout.txt', 'r')
	line = fTimeout.readline()
	timeout_setpoint = int(line.strip())
	fTimeout.close()

	# Set the timeout
	btle.write('CUWV,' + PRIVATE_MODE_SERVICE + ',%02X' % timeout_setpoint, 'Write Timeout')
	temp = None
	while temp != '' and btle.connected == 'yes':
		temp = btle.read()

	if gotSensorData == 1:	
		dataFile = open(MAC + '.txt', 'a') 
		print >> dataFile, "%f, %4d, %4d, %4d, %4d, %1.2f, %d" % (time.time(), sensor[0], sensor[1], sensor[2], sensor[3], Vbat, sensorTimeout)
		dataFile.flush()
		dataFile.close()


	btle.disconnect()

	time.sleep(1) # Wait 1 second to give the module time to disconnect

	fsm.got_data()


fsm.onchangestate = printstatechange
#fsm.oninit = oninit
#fsm.onscan = onscan
#fsm.onconnect = onconnect
#fsm.oncollect = oncollect

fsm.startup()

while(1):
	if(fsm.current == 'init'):
		on_init(None)
	if(fsm.current == 'scan'):
		on_scan(None)
	if(fsm.current == 'connect'):
		on_connect(None)
	if(fsm.current == 'collect'):
		on_collect(None)



