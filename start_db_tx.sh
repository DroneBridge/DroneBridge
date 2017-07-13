#!/bin/bash
file=DroneBridgeTX.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_drone=$(awk -F "=" '/^port_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_smartphone=$(awk -F "=" '/^port_smartphone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
port_local_smartphone=$(awk -F "=" '/^port_local_smartphone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_drone_comm=$(awk -F "=" '/^interface_drone_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
ip_drone=$(awk -F "=" '/^ip_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
mac_drone=$(awk -F "=" '/^mac_drone/ {gsub(/[ \t]/, "", $2); print $2}' $file)
joy_interface=$(awk -F "=" '/^joy_interface/ {gsub(/[ \t]/, "", $2); print $2}' $file)
airbridge_frequ=$(awk -F "=" '/^airbridge_frequ/ {gsub(/[ \t]/, "", $2); print $2}' $file)

echo "DroneBridge-TX: trying to start individual modules...\n"
echo "DroneBridge-TX: Interface long range: $interface_drone_comm - MAC-drone: $mac_drone - Mode: $mode - Joystick interface: $joy_interface"
#python3 /home/pi/airbridge/telemetry/pp_tx.py -i $interface_drone_comm -p $port_drone -r $ip_drone -a $port_local_smartphone -m $mode &
#./home/pi/airbridge/video/Project_Pegasus_video_tx &
echo "DroneBridge-TX: starting controller module...\n"
./ground/control/control_tx -n $interface_drone_comm -j $joy_interface -d $mac_drone -m $mode &
