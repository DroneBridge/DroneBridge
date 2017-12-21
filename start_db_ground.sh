#!/bin/bash
file=/boot/DroneBridgeGround.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_id=$(awk -F "=" '/^communication_id/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_selection=$(awk -F "=" '/^interface_selection/ {gsub(/[ \t]/, "", $2); print $2}' $file)

interface_control=$(awk -F "=" '/^interface_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_tel=$(awk -F "=" '/^interface_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_video=$(awk -F "=" '/^interface_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_comm=$(awk -F "=" '/^interface_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)

en_control=$(awk -F "=" '/^en_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_tel=$(awk -F "=" '/^en_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_video=$(awk -F "=" '/^en_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_comm=$(awk -F "=" '/^en_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)

rc_proto=$(awk -F "=" '/^rc_proto/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_drone=$(awk -F "=" '/^port_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_smartphone_ltm=$(awk -F "=" '/^port_smartphone_ltm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_local_smartphone=$(awk -F "=" '/^port_local_smartphone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
ip_drone=$(awk -F "=" '/^ip_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
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
chipset_video=1
chipset_control=1
chipset_tel=1
chipset_comm=1
DRIVER=`cat /sys/class/net/$interface_control/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    chipset_control=2
fi
DRIVER=`cat /sys/class/net/$interface_tel/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    chipset_tel=2
fi
DRIVER=`cat /sys/class/net/$interface_video/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    chipset_video=2
fi
DRIVER=`cat /sys/class/net/$interface_comm/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" == "ath9k_htc" ]; then
    chipset_comm=2
fi

echo "DroneBridge-Ground: Communication ID: $comm_id RC-Protocol $rc_proto"
echo "DroneBridge-Ground: Interfaces: Control: $interface_control Telemetry: $interface_tel Video: $interface_video Communication: $interface_comm"
echo "DroneBridge-Ground: Joystick Interface: $joy_interface - Mode: $mode"
echo "DroneBridge-Ground: Frametypes: Control: $chipset_control Telemetry-Status: $chipset_tel Video: $chipset_video Communication: $chipset_comm"
echo "DroneBridge-Ground: Calibrating RC using: '$joy_cal'"

echo "DroneBridge-Ground: Trying to start individual modules..."
echo "DroneBridge-Ground: Starting ip checker module..."
python3 comm_telemetry/db_ip_checker.py &

if [ $en_comm = "Y" ]; then
	echo "DroneBridge-Ground: Starting communication module..."
	python3 comm_telemetry/db_comm_ground.py -n $interface_comm -p $port_drone -r $ip_drone -u $port_local_smartphone -m $mode -c $comm_id &
fi

echo "DroneBridge-Ground: Starting status module..."
./control_status/status/status -n $interface_tel -m $mode -c $comm_id &

if [ $en_tel = "Y" ]; then
 echo "DroneBridge-Ground: Starting telemetry module..."
 python3 comm_telemetry/db_telemetry_ground.py -n $interface_tel -r $ip_drone -p $port_drone -m $mode -c $comm_id &
fi

if [ $en_control = "Y" ]; then
	echo "DroneBridge-Ground: Starting controller module..."
	./control_status/control/control_ground -n $interface_control -j $joy_interface -m $mode -v $rc_proto -g "$joy_cal" -c $comm_id &
fi
