#!/bin/bash
file=/boot/DroneBridgeGround.ini
mode=$(awk -F "=" '/^mode/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_id=$(awk -F "=" '/^communication_id/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_selection=$(awk -F "=" '/^interface_selection/ {gsub(/[ \t]/, "", $2); print $2}' $file)

interface_control=$(awk -F "=" '/^interface_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_tel=$(awk -F "=" '/^interface_tel/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_video=$(awk -F "=" '/^interface_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_comm=$(awk -F "=" '/^interface_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
interface_proxy=$(awk -F "=" '/^interface_proxy/ {gsub(/[ \t]/, "", $2); print $2}' $file)

en_control=$(awk -F "=" '/^en_control/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_video=$(awk -F "=" '/^en_video/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_comm=$(awk -F "=" '/^en_comm/ {gsub(/[ \t]/, "", $2); print $2}' $file)
en_plugin=$(awk -F "=" '/^en_plugin/ {gsub(/[ \t]/, "", $2); print $2}' $file)

en_rc_overwrite=$(awk -F "=" '/^en_rc_overwrite/ {gsub(/[ \t]/, "", $2); print $2}' $file)

rc_proto=$(awk -F "=" '/^rc_proto/ {gsub(/[ \t]/, "", $2); print $2}' $file)
proxy_port_local_remote=$(awk -F "=" '/^proxy_port_local_remote/ {gsub(/[ \t]/, "", $2); print $2}' $file)
comm_port_local=$(awk -F "=" '/^comm_port_local/ {gsub(/[ \t]/, "", $2); print $2}' $file)
joy_interface=$(awk -F "=" '/^joy_interface/ {gsub(/[ \t]/, "", $2); print $2}' $file)

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
		interface_proxy=$NIC
	done
fi

# 1 = Ralink | 2 = Atheros --> not used by any module on groundstation -> relict
chipset_video=1
chipset_control=1
chipset_tel=1
chipset_comm=1
DRIVER=`cat /sys/class/net/$interface_control/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = 'ath9k_htc' ]; then
    chipset_control=2
fi
DRIVER=`cat /sys/class/net/$interface_tel/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = 'ath9k_htc' ]; then
    chipset_tel=2
fi
DRIVER=`cat /sys/class/net/$interface_video/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = 'ath9k_htc' ]; then
    chipset_video=2
fi
DRIVER=`cat /sys/class/net/$interface_comm/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$DRIVER" = 'ath9k_htc' ]; then
    chipset_comm=2
fi

echo "DroneBridge-Ground: Communication ID: $comm_id RC-Protocol $rc_proto"
echo "DroneBridge-Ground: Long range interfaces - Control:$interface_control Telemetry:$interface_tel Video:$interface_video Communication:$interface_comm Proxy:$interface_proxy"
echo "DroneBridge-Ground: Joystick Interface: $joy_interface"

echo "DroneBridge-Ground: Trying to start individual modules..."
echo "DroneBridge-Ground: Starting ip checker module..."
python3 communication/db_ip_checker.py &

if [ $en_comm = "Y" ]; then
	echo "DroneBridge-Ground: Starting communication module..."
	python3 communication/db_comm_ground.py -n $interface_comm -p 1604 -u $comm_port_local -m $mode -c $comm_id &
fi

echo "DroneBridge-Ground: Starting status module..."
./status/status -n $interface_tel -m $mode -c $comm_id &

echo "DroneBridge-Ground: Starting proxy & telemetry module..."
./proxy/proxy -n $interface_proxy -m $mode -p $proxy_port_local_remote -c $comm_id -i $interface_tel -l 1604 &

if [ $en_control = "Y" ]; then
	echo "DroneBridge-Ground: Starting controller module..."
	./control/control_ground -n $interface_control -j $joy_interface -m $mode -v $rc_proto -o $en_rc_overwrite -c $comm_id &
fi

if [ "$en_plugin" = "Y" ]; then
    echo "DroneBridge-Ground: Starting plugin module..."
    python3 plugin/db_plugin.py -g &
fi
