import serial
import time
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=0.25)

prefix = '  |'
PRINT_COMMANDS = 1
PRINT_RESPONCES = 1

CHOLD = 45   # HOLD Capacitor inside the 16LF1559 in pF
PRIVATE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8E'
RN4020_Connected = 'NA'
foundMAC = 0
MAC = ['']
foundService = 0
sensor = [0]
cap = [0]

if PRINT_COMMANDS:
	print("> D")
ser.write('D\n')
temp = ser.readline()
while temp != '':
	if PRINT_RESPONCES:
		print(prefix + temp.rstrip('\r\n'))
	splitList = temp.strip().split('=')
	if splitList[0] == 'Connected':
		RN4020_Connected = splitList[1]
	temp = ser.readline()

print("RN4020 Connected -> " + RN4020_Connected) 

if(RN4020_Connected != "no"):
	print("Disconnecting ") 
	if PRINT_COMMANDS:
		print("> K    (disconnect)")
	ser.write('K\n')
	temp = ser.readline()
	while temp != '':
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		temp = ser.readline()
	time.sleep(0.25)

else: 
	if PRINT_COMMANDS:
		print("> X   (stop scan)")
	ser.write('X\n')
	temp = ser.readline()
	while temp != '':
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		temp = ser.readline()

# Scan for bluetooth devices
if PRINT_COMMANDS:
	print("> F   (start scan)")
ser.write('F\n')
temp = ser.readline()
while temp != '':
	if PRINT_RESPONCES:
		print(prefix + temp.rstrip('\r\n'))
	splitList = temp.strip().split(',')
	if len(splitList) > 1:
		MAC[foundMAC] = splitList[0] 
		foundMAC = foundMAC + 1
	temp = ser.readline()

# Assume that the last one is the one we want to connect to.
# Establish a connection
if foundMAC < 1:
	print "No Bluetooth Devices Found!"
else:
	print "Connecting " 
	if PRINT_COMMANDS:
		print "> E,0," + MAC[0] + "   (establish connection)" 
	ser.write('E,0,' + MAC[0] + '\n')
	temp = ser.readline()
	while temp != '':
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		temp = ser.readline()

	if PRINT_COMMANDS:
		print("> LC   (list client services)")
	ser.write('LC\n')
	temp = ser.readline()
	while temp != '':
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		splitList = temp.strip().split(',')
		if len(splitList) == 3:
			if splitList[0] == PRIVATE_SERVICE:
				foundService = 1
		temp = ser.readline()

	# If we found a garden sense stick, read data
	if foundService == 1:
		print "Found GardenSense Sensor"
		gotData = 1
	
		while gotData:
			if PRINT_COMMANDS:
				print("> CURV," + PRIVATE_SERVICE + "   (read sensor)")
			ser.write('CURV,' + PRIVATE_SERVICE + '\n')
			gotData = 0
			temp = ser.readline()
			while temp != '':
				if PRINT_RESPONCES:
					print(prefix + temp.rstrip('\r\n'))
				splitList = temp.strip().strip('.').split(',')
				if len(splitList) == 2:
					sensor[0] = int(splitList[1], 16)
					print "Sensor: ", sensor[0]
					#gotData = 1
					#voltageRatio = sensor[0]/1023.0
					#voltage = voltageRatio*3.3
					#cap[0] = CHOLD*((1-voltageRatio)/voltageRatio)				
					#print "Sensor 0: %.1f" % cap[0]
				
				temp = ser.readline()
		
			time.sleep(2)


