#!/usr/bin/python
import sys
import matplotlib.pyplot as plt
import numpy as np
import time

file = None
plotType = 'Data'
if(len(sys.argv) > 1):
	if(sys.argv[1] == '1' or sys.argv[1] == "Tomato"):
		file = open('/Users/pherget/Dropbox/GardenSense/001EC02641C1.txt')
		titleString = "Tomato"
	if(sys.argv[1] == '2' ):
		file = open('/Users/pherget/Dropbox/GardenSense/001EC026253C.txt')
		titleString = "Flower"
	if(sys.argv[1] == '3'):
		file = open('/Users/pherget/Dropbox/GardenSense/001EC022A8B7.txt')
else:
	print "Usage: PlotData {sensor} {type}"
	print "  sensor = 1 or \"Tomato\""
	print "  sensor = 2 "
	print "  sensor = 3 "
	print "  type = D for data, V for voltage, or T for temperature "
	sys.exit()

if(len(sys.argv) > 2):
	if(sys.argv[2] == 'V' or sys.argv[2] == 'v'):
		plotType = "Voltage"
	if(sys.argv[2] == 'T' or sys.argv[2] == 't'):
		plotType = "Temperature"
	
CHOLD = 45   # HOLD Capacitor inside the 16LF1559 in pF

timeSave = []
s = [[],[],[],[]]
cap = [[],[],[],[]]
volt = []
temperature = []
timeout = []

i = 0
line = None
while line != '':
	line = file.readline()
	data = line.strip().split(',')
	if len(data) > 5:
		timeSave.append(float(data[0]))
		for j in range(0,4):
			raw = float(data[j+1])
			voltageRatio = raw/1023.0
			voltage = voltageRatio*3.3
			if(raw > 0):
				cap[j].append(CHOLD*((1-voltageRatio)/voltageRatio))				
			else:
				cap[j].append(0)
			s[j].append(data[j+1])
		volt.append(data[5])
		alpha = 0.7528
		tempC = (alpha-(float(data[5])/2)*(1-float(data[6])/1023))/0.00132-40
		temperature.append(tempC*9/5+32)
		timeout.append(data[7])
	
		i = i + 1

file.close()

# Subtract time_0 and convert to hours
size = i
time0 = time.time()
t = time.localtime(time0) 
print "Right now is   %2i/%2i/%4i at %2i:%2i" % (t[1], t[2], t[0], t[3], t[4])
# Find the beginning of today.
midnight = time.mktime([t[0],t[1],t[2],0,0,0,t[6],t[7],t[8]])
t = time.localtime(timeSave[0]) 
print "Data starts at %2i/%2i/%4i at %2i:%2i" % (t[1], t[2], t[0], t[3], t[4])
t = time.localtime(timeSave[size-1]) 
print "Data ends at   %2i/%2i/%4i at %2i:%2i" % (t[1], t[2], t[0], t[3], t[4])

# Change Scales to hours and find day boundaries
dayIndex = []
dayList = []
dayNameList = []
for i in range(0,size):
	t = time.localtime(timeSave[i]) 
	dayName = "%i/%i" % (t[1], t[2])
	timeSave[i] = (timeSave[i] - midnight)/60/60 
	day = int((timeSave[i]-24)/24)
	# print "time %f day %f" % (timeSave[i],day)
	if not(day in dayList):
		dayList.append(day)
		dayNameList.append(dayName)
		dayIndex.append(i)
		print "Day %d " % day
		print "Starts at %f" % (timeSave[i]/24)
midnight = (midnight - time0)/60/60
print dayIndex
print dayList
print dayNameList

# Find the endpoint for the selected day
endDay = dayList[len(dayList)-1]  # The last day to plot
daysToPlot = 100                # The total days to plot
if daysToPlot > len(dayList):
	daysToPlot = len(dayList)
day = endDay-daysToPlot + 1
#day = -1
if 1 == 1:
	print day
	i = dayList.index(day)
	ind = 0
	ind2 = 0
	if(i + daysToPlot == len(dayList)):
		# It's the last day in the list
		ind = dayIndex[i]
		ind2 = len(timeSave)-1
	else:
		ind = dayIndex[i]
		ind2 = dayIndex[i+daysToPlot]
	size = ind2-ind	
	
	print "%d -> %d" % (ind, ind2)
	print "%f -> %f" % (timeSave[ind], timeSave[ind2])

	t = np.array(timeSave[ind:ind2])	
	if plotType == 'Data':
		max = 300 # max(cap[0])
		plt.plot(t-24*day, cap[0][ind:ind2], 'k.-', label='Bottom')
		plt.plot(t-24*day, cap[1][ind:ind2], 'b.-', label='B Middle')
		plt.plot(t-24*day, cap[2][ind:ind2], 'g.-', label='T Middle')
		plt.plot(t-24*day, cap[3][ind:ind2], 'r.-', label='Top')
	elif plotType == 'Voltage':
		max = 3.5   # float(max(volt))
		plt.plot(t-24*day, volt[ind:ind2], 'm.-', label='Voltage')
	else:
		max = 100
		plt.plot(t-24*day, temperature[ind:ind2], 'm.-', label='Temperature')


plt.plot([12, 12], [0,max], 'y--')
for i in range(1,daysToPlot ):
	plt.plot([24*i, 24*i], [0,max], 'k--')
	plt.plot([24*i+12, 24*i+12], [0,max], 'y--')
	plt.text(24*i, max/100, dayNameList[i + len(dayList) - daysToPlot])
plt.xlabel('Time (hours)')
if plotType == 'Data':
	plt.ylabel('Moisture')
elif plotType == 'Voltage':
	plt.ylabel('Voltage')
else:
	plt.ylabel('Temperature')

#plt.xlim((0,24))
plt.legend(loc=2)
plt.title(titleString)
plt.show()

