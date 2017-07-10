#!/bin/bash
file=/root/dronebridge/DroneBridgeRX.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_TX=$(awk -F "=" '/^interface_TX/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_gopro=$(awk -F "=" '/^interface_gopro/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_telemetry=$(awk -F "=" '/^interface_telemetry/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_MSP=$(awk -F "=" '/^interface_MSP/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_telemetry=$(awk -F "=" '/^port_telemetry/ {gsub(/[ \t]/, "", $2); print $2}' $file)
airbridge_frequ=$(awk -F "=" '/^airbridge_frequ/ {gsub(/[ \t]/, "", $2); print $2}' $file)

echo "DroneBridge-RX: trying to start individual modules...\n"
#python3 /home/pi/airbridge/telemetry/pp_rx.py -i $interface_TX -f $interface_telemetry -p $port_telemetry -m $mode &
#python3 /home/pi/airbridge/video/GoPro_keepAlive.py &
#./home/pi/airbridge/video/Project_Pegasus_video_rx &
echo "starting controller module...\n"
./root/dronebridge/air/control/control_rx -n $interface_TX -u $interface_MSP -m $mode && fg

