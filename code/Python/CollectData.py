import serial
import time
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=0.25)

prefix = '  |'
PRINT_COMMANDS = 0
PRINT_RESPONCES = 0

CHOLD = 45   # HOLD Capacitor inside the 16LF1559 in pF
PRIVATE_SERVICE = 'ACD63933AE9C393DA313F75B3E5F9A8E'
RN4020_Connected = 'NA'
foundMAC = 1
MAC = ['', '', '', '', '', '']
foundGardenSenseService = 0
sensor = [0,0,0,0]
voltageRatio = [0,0,0,0]
voltage = [0,0,0,0]
cap = [0,0,0,0]

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

while foundMAC > 0:
	# Scan for bluetooth devices
	# Set a long timeout to wait for devices
	foundMAC = 0
	if PRINT_COMMANDS:
		print("> F   (start scan)")
	ser.timeout = 60     # Search for 60 seconds before timing out.
	ser.write('F\n')
	temp = ser.readline()
	while temp != '' and foundMAC == 0:
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
		splitList = temp.strip().split(',')
		if len(splitList) > 1:
			MAC[foundMAC] = splitList[0] 
			foundMAC = foundMAC + 1
		else:
			temp = ser.readline()
	ser.timeout = 0.25

	if PRINT_COMMANDS:
		print("> X   (stop scan)")
	ser.write('X\n')
	temp = ser.readline()
	while temp != '':
		if PRINT_RESPONCES:
			print(prefix + temp.rstrip('\r\n'))
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
					foundGardenSenseService = 1
			temp = ser.readline()

		# If we found a garden sense stick, read data
		if foundGardenSenseService == 1:
			print "Found GardenSense Sensor: " + MAC[0]
			gotData = 1
			dataFile = open(MAC[0] + '.txt', 'a') 
	
			if PRINT_COMMANDS:
				print("> CURV," + PRIVATE_SERVICE + "   (read sensor)")
			ser.write('CURV,' + PRIVATE_SERVICE + '\n')
			temp = ser.readline()
			while temp != '':
				if PRINT_RESPONCES:
					print(prefix + temp.rstrip('\r\n'))
				splitList = temp.strip().strip('.').split(',')
				if len(splitList) == 2:
					sensor[0] = int(splitList[1][0:4], 16)
					sensor[1] = int(splitList[1][4:8], 16)
					sensor[2] = int(splitList[1][8:12], 16)
					sensor[3] = int(splitList[1][12:16], 16)
					print >> dataFile, "%f, %4d, %4d, %4d, %4d" % (time.time(), sensor[0], sensor[1], sensor[2], sensor[3])
					dataFile.flush()
					#print "S0: %.2fV   S1: %.2fV   S2: %.2fV   S3: %.2fV   " % (voltage[0], voltage[1], voltage[2], voltage[3])					
					for i in range(0,4):
						voltageRatio[i] = sensor[i]/1023.0
						voltage[i] = voltageRatio[i]*3.3
						cap[i] = CHOLD*((1-voltageRatio[i])/voltageRatio[i])				
					print "S0: %.1fpF  S1: %.1fpF  S2: %.1fpF  S3: %.1fpF  " % (cap[0], cap[1], cap[2], cap[3])
			
				temp = ser.readline()
	
			dataFile.close()

		print "Disconnecting " 
		if PRINT_COMMANDS:
			print("> K   (disconnect)")
		ser.write('K\n')
		temp = ser.readline()
		while temp != '':
			if PRINT_RESPONCES:
				print(prefix + temp.rstrip('\r\n'))
			temp = ser.readline()

	time.sleep(1) # Wait 1 second to give the module time to disconnect





