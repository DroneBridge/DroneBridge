#!/bin/bash
file=/boot/DroneBridgeRX.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_id=$(awk -F "=" '/^communication_id/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_selection=$(awk -F "=" '/^interface_selection/ {gsub(/[ \t]/, "", $2); print $2}' $file)

interface_control=$(awk -F "=" '/^interface_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_tel=$(awk -F "=" '/^interface_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_video=$(awk -F "=" '/^interface_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_comm=$(awk -F "=" '/^interface_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_gopro=$(awk -F "=" '/^interface_gopro/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_telemetry=$(awk -F "=" '/^interface_telemetry/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_MSP=$(awk -F "=" '/^interface_MSP/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_telemetry=$(awk -F "=" '/^port_telemetry/ {gsub(/[ \t]/, "", $2); print $2}' $file)

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

echo "DroneBridge-RX: Communication ID: $comm_id"
echo "DroneBridge-RX: Interfaces: Control: $interface_control Telemetry: $interface_tel Video: $interface_video Communication: $interface_comm - Interface MSP: $interface_MSP - Mode: $mode"
echo "DroneBridge-RX: Frametypes: Control: $frametype_control Telemetry: $frametype_tel Video: $frametype_video Communication: $frametype_comm"
echo "DroneBridge-RX: Trying to start individual modules..."

echo "DroneBridge-RX: Starting communication module..."
python3 comm_telemetry/db_comm_rx.py -i $interface_comm -p $port_telemetry -m $mode -a $frametype_comm -c $comm_id &

echo "DroneBridge-RX: Starting telemetry module..."
python3 comm_telemetry/db_telemetry_rx.py -i $interface_tel -f $interface_telemetry -p $port_telemetry -m $mode -t yes -a $frametype_tel -c $comm_id &

#python3 /home/pi/airbridge/video/GoPro_keepAlive.py &
#./home/pi/airbridge/video/Project_Pegasus_video_rx &

echo "DroneBridge-RX: Starting controller module..."
./control/air/control_rx -n $interface_control -u $interface_MSP -m $mode &