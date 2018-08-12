#!/bin/bash
file=/boot/DroneBridgeAir.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_id=$(awk -F "=" '/^communication_id/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_selection=$(awk -F "=" '/^interface_selection/ {gsub(/[ \t]/, "", $2); print $2}' $file)

en_tel=$(awk -F "=" '/^en_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_video=$(awk -F "=" '/^en_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_comm=$(awk -F "=" '/^en_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_control=$(awk -F "=" '/^en_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_plugin=$(awk -F "=" '/^en_plugin/ {gsub(/[ \t]/, "", $2); print $2}' $file)

interface_control=$(awk -F "=" '/^interface_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_tel=$(awk -F "=" '/^interface_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_video=$(awk -F "=" '/^interface_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_comm=$(awk -F "=" '/^interface_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_gopro=$(awk -F "=" '/^interface_gopro/ {gsub(/[ \t]/, "", $2); print $2}' $file)
serial_int_tel=$(awk -F "=" '/^serial_int_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
baud_tel=$(awk -F "=" '/^baud_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
tel_proto=$(awk -F "=" '/^tel_proto/ {gsub(/[ \t]/, "", $2); print $2}' $file)
serial_int_cont=$(awk -F "=" '/^serial_int_cont/ {gsub(/[ \t]/, "", $2); print $2}' $file)
serial_prot=$(awk -F "=" '/^serial_prot/ {gsub(/[ \t]/, "", $2); print $2}' $file)
pass_through_packet_size=$(awk -F "=" '/^pass_through_packet_size/ {gsub(/[ \t]/, "", $2); print $2}' $file)
baud_control=$(awk -F "=" '/^baud_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
enable_sumd_rc=$(awk -F "=" '/^enable_sumd_rc/ {gsub(/[ \t]/, "", $2); print $2}' $file)
serial_int_sumd=$(awk -F "=" '/^serial_int_sumd/ {gsub(/[ \t]/, "", $2); print $2}' $file)

if [ "$interface_selection" = 'auto' ]; then
    NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
    echo -n "Network adapters: "
    echo $NICS
    for NIC in $NICS
	do
		interface_control=$NIC
		interface_tel=$NIC
		interface_video=$NIC
		interface_comm=$NIC
	done
fi

# 1 = Ralink | 2 = Atheros --> currently only used by control module to read RSSI from radiotap header
chipset_video=1
chipset_control=1
chipset_tel=1
chipset_comm=1
DRIVER=`cat /sys/class/net/$interface_control/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = "ath9k_htc" ]; then
    chipset_control=2
fi
DRIVER=`cat /sys/class/net/$interface_tel/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = "ath9k_htc" ]; then
    chipset_tel=2
fi
DRIVER=`cat /sys/class/net/$interface_video/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = "ath9k_htc" ]; then
    chipset_video=2
fi
DRIVER=`cat /sys/class/net/$interface_comm/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = "ath9k_htc" ]; then
    chipset_comm=2
fi

if [ "$serial_int_cont" = "$serial_int_tel" ] && [ "$en_tel" = "Y" ] && [ "$en_control" = "Y" ]; then
	echo "Error: Control module and telemetry module are assigned to the same serial port. Disabling telemetry module. Control module only supports MAVLink telemetry."
	en_tel="N"
fi

if [ "$serial_int_cont" = "$serial_int_sumd" ] && [ "$enable_sumd_rc" = "Y" ] && [ "$en_control" = "Y" ]; then
	echo "Error: Control module and SUMD output are assigned to the same serial port. Disabling SUMD."
	enable_sumd_rc="N"
fi

echo "DroneBridge-Air: Communication ID: $comm_id"
echo "DroneBridge-Air: Long range interfaces - Control:$interface_control Telemetry:$interface_tel Video:$interface_video Communication:$interface_comm"
echo "DroneBridge-Air: Serial portocol MSP/MAVLInk: $serial_int_cont, SUMD activated: $enable_sumd_rc (on port $serial_int_sumd)"
echo "DroneBridge-Air: Chipset types: Control: $chipset_control"
echo "DroneBridge-Air: Trying to start individual modules..."

if [ "$en_comm" = "Y" ]; then
	echo "DroneBridge-Air: Starting communication module..."
	python3 communication/db_comm_air.py -n $interface_comm -m $mode -c $comm_id &
fi

if [ "$en_tel" = "Y" ]; then
	echo "DroneBridge-Air: Starting telemetry module..."
	./telemetry/telemetry_air -n $interface_tel -f $serial_int_tel -r $baud_tel -m $mode -c $comm_id -l $tel_proto &
fi

if [ "$en_control" = "Y" ]; then
	echo "DroneBridge-Air: Starting controller module..."
	./control/control_air -n $interface_control -u $serial_int_cont -m $mode -c $comm_id -a $chipset_control -v $serial_prot -l $pass_through_packet_size -r $baud_control -e $enable_sumd_rc -s $serial_int_sumd &
fi

if [ "$en_plugin" = "Y" ]; then
    echo "DroneBridge-Air: Starting plugin module..."
    python3 plugin/db_plugin.py &
fi