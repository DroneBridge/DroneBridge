#!/bin/bash
file=/boot/DroneBridgeTX.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_id=$(awk -F "=" '/^communication_id/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_selection=$(awk -F "=" '/^interface_selection/ {gsub(/[ \t]/, "", $2); print $2}' $file)

interface_control=$(awk -F "=" '/^interface_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_tel=$(awk -F "=" '/^interface_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_video=$(awk -F "=" '/^interface_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_comm=$(awk -F "=" '/^interface_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)

port_drone=$(awk -F "=" '/^port_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_smartphone_ltm=$(awk -F "=" '/^port_smartphone_ltm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_local_smartphone=$(awk -F "=" '/^port_local_smartphone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
ip_drone=$(awk -F "=" '/^ip_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
mac_drone=$(awk -F "=" '/^mac_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
joy_interface=$(awk -F "=" '/^joy_interface/ {gsub(/[ \t]/, "", $2); print $2}' $file)
joy_cal=$(sed -n -e 's/^\s*joy_cal\s*=\s*//p' $file)

if [[ interface_selection=='auto' ]]; then
	NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
    echo -n "NICS:"
    echo $NICS
    for NIC in $NICS 
	do
		interface_control=$NIC
		interface_tel=$NIC
		interface_video=$NIC
		interface_comm=$NIC
	done
fi

# 1 = Ralink | 2 = Atheros
frametype_video=1
frametype_control=1
frametype_tel=1
frametype_comm=1
DRIVER=`cat /sys/class/net/$interface_control/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    frametype_control=2
fi
DRIVER=`cat /sys/class/net/$interface_tel/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    frametype_tel=2
fi
DRIVER=`cat /sys/class/net/$interface_video/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    frametype_video=2
fi
DRIVER=`cat /sys/class/net/$interface_comm/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    frametype_comm=2
fi

echo "DroneBridge-TX: Communication ID: $comm_id"
echo "DroneBridge-TX: Interfaces: Control: $interface_control Telemetry: $interface_tel Video: $interface_video Communication: $interface_comm - Joystick Interface: $joy_interface - Mode: $mode"
echo "DroneBridge-TX: Frametypes: Control: $frametype_control Telemetry: $frametype_tel Video: $frametype_video Communication: $frametype_comm"
echo "DroneBridge-TX: calibrating RC using: '$joy_cal'"

echo "DroneBridge-TX: Trying to start individual modules..."

echo "DroneBridge-TX: Starting communication module..."
python3 comm_telemetry/db_comm_tx.py -i $interface_comm -p $port_drone -r $ip_drone -u $port_local_smartphone -m $mode -a $frametype_comm -c $comm_id &

echo "DroneBridge-TX: Starting telemetry module..."
python3 comm_telemetry/db_telemetry_tx.py -i $interface_tel -r $ip_drone -p $port_drone -m $mode -a $frametype_tel -c $comm_id &

#./home/pi/airbridge/video/Project_Pegasus_video_tx &

echo "DroneBridge-TX: Starting controller module...\n"
./control/ground/control_tx -n $interface_control -j $joy_interface -d $mac_drone -m $mode -g "$joy_cal" -a $frametype_control -c $comm_id &
