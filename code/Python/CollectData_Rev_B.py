import fysom
import serial
import time

prefix = '  |'
PRINT_COMMANDS = 1
PRINT_RESPONCES = 1
PRINT_STATES = 1

PRIVATE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8E'
PRIVATE_BAT_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8B'
PRIVATE_TEMP_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8D'
PRIVATE_MODE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8F'

global fsm 
fsm = None

class RN4020:
	def __init__(self, port):
		self.ser = serial.Serial(port, 115200, timeout=0.5)

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

		# Set the TX power level to the maximum setting
		self.write('SP,7', 'Set Max TX Power')
	
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


# Read the btle serial port from the text file
fPort = open('ttyport.txt', 'r')
line = fPort.readline()
BTLEport = line.strip()
fPort.close()

print "Opening Blue Tooth Device on %s...\n" % BTLEport
btle = RN4020(BTLEport)
MAC = None
DataPath = ""

fsm = fysom.Fysom({
	'events': [
		{'name': 'startup'         , 'src': 'none'         , 'dst': 'init'},
		{'name': 'done_init'       , 'src': 'init'         , 'dst': 'wait_connect'},
		{'name': 'connected'       , 'src': 'wait_connect' , 'dst': 'read_data'},
		{'name': 'no_connection'   , 'src': 'wait_connect' , 'dst': 'wait_connect'},
		{'name': 'got_data'        , 'src': 'read_data'    , 'dst': 'wait_connect'},
	],
})


def printstatechange(e):
 	if PRINT_STATES == 1:   print '\n======> event: %s, src: %s, dst: %s' % (e.event, e.src, e.dst)

def on_init(e):
	global DataPath
	global TimeoutSetpoint

	# Read the timeout setpoint from the text file
	fTimeout = open('timeout.txt', 'r')
	line = fTimeout.readline()
	TimeoutSetpoint = int(line.strip())
	fTimeout.close()

	btle.write('SUW,' + PRIVATE_MODE_SERVICE + ',%02X' % TimeoutSetpoint, 'Write Timeout')
	temp = None
	while temp != '' and temp !='ERR' and temp!='AOK':
		temp = btle.read()

	# Get the path to write the data to
	fDataPath = open('data_path.txt', 'r')
	line = fDataPath.readline()
	while line != '':
		if(len(line.strip()) > 0):
			DataPath = line.strip()
			line = ''
		else:
			line = fDataPath.readline()
	fDataPath.close()

	fsm.done_init()

def on_wait_connect(e):
	# Keep trying the scan until a device is found
	btle.setTimeout(10)
	temp = None
	while temp != '' and temp != 'ERR' and btle.connected == 'no':
		temp = btle.read().strip()

	if btle.connected == 'yes':
		fsm.connected()
	else:
		fsm.no_connection()

def on_read_data(e):
	global DataPath

	foundBTA = 0
	foundData = 0
	foundBattery = 0
	foundTemp = 0
	temp = None
	while temp != '' and btle.connected == 'yes':
		temp = btle.read().strip()
		splitList = temp.strip().split(',')
		if len(splitList) == 3:    
			if splitList[1] == '000B':   # Check if it's the BTA
				foundBTA = 1
				BTA = splitList[2].strip('.')
			if splitList[1] == '000D':   # Check if it's the sensor data
				data = splitList[2]
				if len(data) > 6:
					foundData = 1
			if splitList[1] == '000F':   # Check if it's the battery level
				battery = splitList[2].strip('.')
				if len(battery) > 2:
					foundBattery = 1
			if splitList[1] == '0011':   # Check if it's the temperature
				temperature = splitList[2].strip('.')
				if len(temperature) > 2:
					foundTemp = 1

	# If we time out and are still connected	
	if btle.connected == 'yes':
		btle.disconnect()

	if foundData:
		CHOLD = 45   # HOLD Capacitor inside the 16LF1559 in pF
		# Convert the moisture data
		print time.ctime()

		sensor = [0,0,0,0,0,0,0]
		voltageRatio = [0,0,0,0,0,0,0]
		voltage = [0,0,0,0,0,0,0]
		cap = [0,0,0,0,0,0,0]

		sensor[0] = int(data[0:4], 16)
		sensor[1] = int(data[4:8], 16)
		sensor[2] = int(data[8:12], 16)
		sensor[3] = int(data[12:16], 16)
		sensor[4] = int(data[16:20], 16)
		sensor[5] = int(data[20:24], 16)
		sensor[6] = int(data[24:28], 16)
		#print "S0: %.2fV   S1: %.2fV   S2: %.2fV   S3: %.2fV   " % (voltage[0], voltage[1], voltage[2], voltage[3])	
		# Calculate Capacitance		
		for i in range(0,7):
			voltageRatio[i] = sensor[i]/1023.0
			voltage[i] = voltageRatio[i]*3.3
			if(sensor[1] > 0):
				cap[i] = CHOLD*((1-voltageRatio[i])/voltageRatio[i])
			else:
				cap[i] = 0
		print "S0: %.1fpF  S1: %.1fpF  S2: %.1fpF  S3: %.1fpF  S4: %.1fpF  S5: %.1fpF  S6: %.1fpF  " % (cap[0], cap[1], cap[2], cap[3], cap[4], cap[5], cap[6])
	
		Vbat = 0
		if foundBattery:
			# Convert the battery voltage
			readBat = int(battery, 16)
			if readBat > 0:
				Vbat = 1.024*1023/readBat
			else:
				Vbat = 0;
			print "Battery Voltage: %1.2fV" % Vbat

		Temperature = 0
		if foundTemp:
			readTemp = int(temperature, 16)
			if readTemp > 0:
				Temperature = readTemp
			else:
				Temperature = 0;
			print "Temperature: %3.1f Degrees F" % Temperature

		sensorTimeout = 0 

		# Get the timeout
		#btle.write('CURV,' + PRIVATE_MODE_SERVICE, 'Read Timeout')
		#gotTimeoutData = 0
		#sensorTimeout = 0
		#temp = None
		#while temp != '' and gotTimeoutData == 0 and btle.connected == 'yes':
		#	temp = btle.read()
		#	splitList = temp.strip().strip('.').split(',')
		#	if len(splitList) == 2 and splitList[0] == 'R' and len(splitList[1]) >= 1:
		#		gotTimeoutData = 1
		#		sensorTimeout = int(splitList[1], 16) * 5
		#		print "Sensor Timeout : %d sec" % sensorTimeout

		# Read the timeout setpoint from the text file
		#fTimeout = open('timeout.txt', 'r')
		#line = fTimeout.readline()
		#timeout_setpoint = int(line.strip())
		#fTimeout.close()

		# Set the timeout if we were able to read it and it's not at the setpoint 
		#if sensorTimeout > 0 and sensorTimeout != timeout_setpoint * 5:
		#	btle.write('CUWV,' + PRIVATE_MODE_SERVICE + ',%02X' % timeout_setpoint, 'Write Timeout')
		#	temp = None
		#	while temp != '' and btle.connected == 'yes':
		#		temp = btle.read()

		dataFile = open(DataPath + BTA + '.txt', 'a') 
		print >> dataFile, "%f, %4d, %4d, %4d, %4d, %4d, %4d, %4d, %1.2f, %3.1f, %d" % (time.time(), sensor[0], sensor[1], sensor[2], sensor[3], sensor[4], sensor[5], sensor[6], Vbat, Temperature, sensorTimeout)
		dataFile.flush()
		dataFile.close()


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
	if(fsm.current == 'wait_connect'):
		on_wait_connect(None)
	if(fsm.current == 'read_data'):
		on_read_data(None)



