# /root/.profile - main EZ-Wifibroadcast script
# (c) 2017 by Rodizio. Licensed under GPL2
# Modified to be used with DroneBridge by Wolfgang Christl

#
# functions
#

function tmessage {
  if [ "$QUIET" == "N" ]; then
    echo $1 "$2"
  fi
}

function collect_errorlog {
  sleep 3
  echo
  if nice dmesg | nice grep -q over-current; then
    echo "ERROR: Over-current detected - potential power supply problems!"
  fi

  # check for USB disconnects (due to power-supply problems)
  if nice dmesg | nice grep -q disconnect; then
    echo "ERROR: USB disconnect detected - potential power supply problems!"
  fi

  nice mount -o remount,rw /boot

  # check if over-temp or under-voltage occured
  if vcgencmd get_throttled | nice grep -q -v "0x0"; then
    TEMP=`cat /sys/class/thermal/thermal_zone0/temp`
    TEMP_C=$(($TEMP/1000))
    if [ "$TEMP_C" -lt 75 ]; then # it must be under-voltage
    echo
    echo "  ---------------------------------------------------------------------------------------------------"
    echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |"
    echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |"
    echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |"
    echo "  ---------------------------------------------------------------------------------------------------"
    echo
    echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | When you have fixed wiring/power-supply, delete this file and make sure it doesn't re-appear!   |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  fi
fi

mv /boot/errorlog.txt /boot/errorlog-old.txt > /dev/null 2>&1
mv /boot/errorlog.png /boot/errorlog-old.png > /dev/null 2>&1
echo -n "Camera: "
nice /usr/bin/vcgencmd get_camera
uptime >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
echo -n "Camera: " >>/boot/errorlog.txt
nice /usr/bin/vcgencmd get_camera >>/boot/errorlog.txt
echo
nice dmesg | nice grep disconnect
nice dmesg | nice grep over-current
nice dmesg | nice grep disconnect >>/boot/errorlog.txt
nice dmesg | nice grep over-current >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
echo

NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb`

for NIC in $NICS
do
  iwconfig $NIC | grep $NIC
done
echo
echo "Detected USB devices:"
lsusb

nice iwconfig >>/boot/errorlog.txt > /dev/null 2>&1
echo >>/boot/errorlog.txt
nice ifconfig >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice iw reg get >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice iw list >>/boot/errorlog.txt
echo >>/boot/errorlog.txt


nice ps ax >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice df -h >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice mount >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice fdisk -l /dev/mmcblk0 >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice lsmod >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice lsusb >>/boot/errorlog.txt
nice lsusb -v >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice ls -la /dev >>/boot/errorlog.txt
nice ls -la /dev/input >>/boot/errorlog.txt
echo
nice vcgencmd measure_temp
nice vcgencmd get_throttled
echo >>/boot/errorlog.txt
nice vcgencmd measure_volts >>/boot/errorlog.txt
nice vcgencmd measure_temp >>/boot/errorlog.txt
nice vcgencmd get_throttled >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice vcgencmd get_config int >>/boot/errorlog.txt

nice /root/wifibroadcast_misc/raspi2png -p /boot/errorlog.png
echo >>/boot/errorlog.txt
nice dmesg >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
echo >>/boot/errorlog.txt

nice cat /etc/modprobe.d/rt2800usb.conf >> /boot/errorlog.txt
nice cat /etc/modprobe.d/ath9k_htc.conf >> /boot/errorlog.txt
nice cat /etc/modprobe.d/ath9k_hw.conf >> /boot/errorlog.txt

echo >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice cat /boot/wifibroadcast-1.txt | egrep -v "^(#|$)" >> /boot/errorlog.txt
echo >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice cat /boot/osdconfig.txt | egrep -v "^(//|$)" >> /boot/errorlog.txt
echo >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice cat /boot/joyconfig.txt | egrep -v "^(//|$)" >> /boot/errorlog.txt
echo >>/boot/errorlog.txt
echo >>/boot/errorlog.txt
nice cat /boot/apconfig.txt | egrep -v "^(#|$)" >> /boot/errorlog.txt

sync
nice mount -o remount,ro /boot
}

function collect_debug {
  sleep 25

  DEBUGPATH=$1
  if [ "$DEBUGPATH" == "/boot" ]; then # if debugpath is boot partition, make it writeable first and move old logs
  nice mount -o remount,rw /boot
  mv /boot/debug.txt /boot/debug-old.txt > /dev/null 2>&1
fi

uptime >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
echo -n "Camera: " >>$DEBUGPATH/debug.txt
nice /usr/bin/vcgencmd get_camera >>$DEBUGPATH/debug.txt
nice dmesg | nice grep disconnect >>$DEBUGPATH/debug.txt
nice dmesg | nice grep over-current >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice tvservice -s >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice tvservice -m CEA >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice tvservice -m DMT >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice iwconfig >>$DEBUGPATH/debug.txt > /dev/null 2>&1
echo >>$DEBUGPATH/debug.txt
nice ifconfig >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice iw reg get >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice iw list >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice ps ax >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice df -h >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice mount >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice fdisk -l /dev/mmcblk0 >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice lsmod >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice lsusb >>$DEBUGPATH/debug.txt
nice lsusb -v >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice ls -la /dev >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice ls -la /dev/input >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice vcgencmd measure_temp >>$DEBUGPATH/debug.txt
nice vcgencmd get_throttled >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice vcgencmd get_config int >>$DEBUGPATH/debug.txt

echo >>$DEBUGPATH/debug.txt
nice dmesg >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt

nice cat /etc/modprobe.d/rt2800usb.conf >> $DEBUGPATH/debug.txt
nice cat /etc/modprobe.d/ath9k_htc.conf >> $DEBUGPATH/debug.txt
nice cat /etc/modprobe.d/ath9k_hw.conf >> $DEBUGPATH/debug.txt

echo >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice cat /boot/wifibroadcast-1.txt | egrep -v "^(#|$)" >> $DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice cat /boot/osdconfig.txt | egrep -v "^(//|$)" >> $DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice cat /boot/joyconfig.txt | egrep -v "^(//|$)" >> $DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
echo >>$DEBUGPATH/debug.txt
nice cat /boot/apconfig.txt | egrep -v "^(#|$)" >> $DEBUGPATH/debug.txt

nice top -n 3 -b -d 2 >>$DEBUGPATH/debug.txt

if [ "$DEBUGPATH" == "/boot" ]; then # if debugpath is boot partition, sync and remount ro
sync
nice mount -o remount,ro /boot
fi

}


function prepare_nic {
  DRIVER=`cat /sys/class/net/$1/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
  if [ "$DRIVER" != "rt2800usb" ] && [ "$DRIVER" != "mt7601u" ] && [ "$DRIVER" != "ath9k_htc" ]; then
    tmessage "WARNING: Unsupported or experimental wifi card: $DRIVER"
  fi

  tmessage -n "Setting up $1: "
  if [ "$DRIVER" == "ath9k_htc" ]; then # set bitrates for Atheros via iw
  ifconfig $1 up || {
    echo
    echo "ERROR: Bringing up interface $1 failed!"
    collect_errorlog
    sleep 365d
  }
  sleep 0.2

  if [ "$CAM" == "0" ]; then # we are RX, set bitrate to uplink bitrate
  #tmessage -n "bitrate $UPLINK_WIFI_BITRATE Mbit "
  if [ "$UPLINK_WIFI_BITRATE" != "19.5" ]; then # only set bitrate if something else than 19.5 is requested (19.5 is default compiled in ath9k_htc firmware)
  iw dev $1 set bitrates legacy-2.4 $UPLINK_WIFI_BITRATE || {
    echo
    echo "ERROR: Setting bitrate on $1 failed!"
    collect_errorlog
    sleep 365d
  }
fi
sleep 0.2
#tmessage -n "done. "
else # we are TX, set bitrate to downstream bitrate
  tmessage -n "bitrate "
  if [ "$VIDEO_WIFI_BITRATE" != "19.5" ]; then # only set bitrate if something else than 19.5 is requested (19.5 is default compiled in ath9k_htc firmware)
  tmessage -n "$VIDEO_WIFI_BITRATE Mbit "
  iw dev $1 set bitrates legacy-2.4 $VIDEO_WIFI_BITRATE || {
    echo
    echo "ERROR: Setting bitrate on $1 failed!"
    collect_errorlog
    sleep 365d
  }
else
  tmessage -n "$VIDEO_WIFI_BITRATE Mbit "
fi
sleep 0.2
tmessage -n "done. "
fi

ifconfig $1 down || {
  echo
  echo "ERROR: Bringing down interface $1 failed!"
  collect_errorlog
  sleep 365d
}
sleep 0.2

tmessage -n "monitor mode.. "
iw dev $1 set monitor none || {
  echo
  echo "ERROR: Setting monitor mode on $1 failed!"
  collect_errorlog
  sleep 365d
}
sleep 0.2
tmessage -n "done. "

ifconfig $1 up || {
  echo
  echo "ERROR: Bringing up interface $1 failed!"
  collect_errorlog
  sleep 365d
}
sleep 0.2

if [ "$2" != "0" ]; then
  tmessage -n "frequency $2 MHz.. "
  iw dev $1 set freq $2 || {
    echo
    echo "ERROR: Setting frequency $2 MHz on $1 failed!"
    collect_errorlog
    sleep 365d
  }
  tmessage "done!"
else
  echo
fi

fi

if [ "$DRIVER" == "rt2800usb" ] || [ "$DRIVER" == "mt7601u" ] || [ "$DRIVER" == "rtl8192cu" ] || [ "$DRIVER" == "8812au" ]; then # do not set bitrate for Ralink, Mediatek, Realtek, done through tx parameter
tmessage -n "monitor mode.. "
iw dev $1 set monitor none || {
  echo
  echo "ERROR: Setting monitor mode on $1 failed!"
  collect_errorlog
  sleep 365d
}
sleep 0.2
tmessage -n "done. "

#tmessage -n "bringing up.. "
ifconfig $1 up || {
  echo
  echo "ERROR: Bringing up interface $1 failed!"
  collect_errorlog
  sleep 365d
}
sleep 0.2
#tmessage -n "done. "

if [ "$2" != "0" ]; then
  tmessage -n "frequency $2 MHz.. "
  iw dev $1 set freq $2 || {
    echo
    echo "ERROR: Setting frequency $2 MHz on $1 failed!"
    collect_errorlog
    sleep 365d
  }
  tmessage "done!"
else
  echo
fi

fi

}


function detect_nics {
  tmessage "Setting up wifi cards ... "
  echo

  # set reg domain to DE to allow channel 12 and 13 for hotspot
  iw reg set DE

  NUM_CARDS=-1
  NICSWL=`ls /sys/class/net | nice grep wlan`

  for NIC in $NICSWL
  do
    # set MTU to 2304
    ifconfig $NIC mtu 2304
    # re-name wifi interface to MAC address
    NAME=`cat /sys/class/net/$NIC/address`
    ip link set $NIC name ${NAME//:}
    let "NUM_CARDS++"
    #sleep 0.1
  done

  if [ "$NUM_CARDS" == "-1" ]; then
    echo "ERROR: No wifi cards detected"
    collect_errorlog
    sleep 365d
  fi

  if [ "$CAM" == "0" ]; then # only do relay/hotspot stuff if RX
  # get wifi hotspot card out of the way
  if [ "$WIFI_HOTSPOT" == "Y" ]; then
    if [ "$WIFI_HOTSPOT_NIC" != "internal" ]; then
      # only configure it if it's there
      if ls /sys/class/net/ | grep -q $WIFI_HOTSPOT_NIC; then
        tmessage -n "Setting up $WIFI_HOTSPOT_NIC for Wifi Hotspot operation.. "
        ip link set $WIFI_HOTSPOT_NIC name wifihotspot0
        ifconfig wifihotspot0 192.168.2.1 up
        tmessage "done!"
        let "NUM_CARDS--"
      else
        tmessage "Wifi Hotspot card $WIFI_HOTSPOT_NIC not found!"
        sleep 0.5
      fi
    else
      # only configure it if it's there
      if ls /sys/class/net/ | grep -q intwifi0; then
        tmessage -n "Setting up intwifi0 for Wifi Hotspot operation.. "
        ip link set intwifi0 name wifihotspot0
        ifconfig wifihotspot0 192.168.2.1 up
        tmessage "done!"
      else
        tmessage "Pi3 Onboard Wifi Hotspot card not found!"
        sleep 0.5
      fi
    fi
  fi
  # get relay card out of the way
  if [ "$RELAY" == "Y" ]; then
    # only configure it if it's there
    if ls /sys/class/net/ | grep -q $RELAY_NIC; then
      ip link set $RELAY_NIC name relay0
      prepare_nic relay0 $RELAY_FREQ
      let "NUM_CARDS--"
    else
      tmessage "Relay card $RELAY_NIC not found!"
      sleep 0.5
    fi
  fi

fi

NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
#	echo "NICS: $NICS"

if [ "$TXMODE" != "single" ]; then
  for i in $(eval echo {0..$NUM_CARDS})
  do
    if [ "$CAM" == "0" ]; then
      prepare_nic ${MAC_RX[$i]} ${FREQ_RX[$i]}
    else
      prepare_nic ${MAC_TX[$i]} ${FREQ_TX[$i]}
    fi
    sleep 0.1
  done
else
  # check if auto scan is enabled, if yes, set freq to 0 to let prepare_nic know not to set channel
  if [ "$FREQSCAN" == "Y" ] && [ "$CAM" == "0" ]; then
    for NIC in $NICS
    do
      prepare_nic $NIC 2484
      sleep 0.1
    done
    # make sure check_alive function doesnt restart hello_video while we are still scanning for channel
    touch /tmp/pausewhile
    /root/wifibroadcast/rx -p 0 -d 1 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEOBLOCKLENGTH $NICS >/dev/null &
    sleep 0.5
    echo
    echo -n "Please wait, scanning for TX ..."
    FREQ=0

    if iw list | nice grep -q 5180; then # cards support 5G and 2.4G
    FREQCMD="/root/wifibroadcast/channelscan 245 $NICS"
  else
    if iw list | nice grep -q 2312; then # cards support 2.3G and 2.4G
    FREQCMD="/root/wifibroadcast/channelscan 2324 $NICS"
  else # cards support only 2.4G
    FREQCMD="/root/wifibroadcast/channelscan 24 $NICS"
  fi
fi

while [ $FREQ -eq 0 ]; do
  FREQ=`$FREQCMD`
done

echo "found on $FREQ MHz"
echo
ps -ef | nice grep "rx -p 0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
for NIC in $NICS
do
  echo -n "Setting frequency on $NIC to $FREQ MHz.. "
  iw dev $NIC set freq $FREQ
  echo "done."
  sleep 0.1
done
# all done
rm /tmp/pausewhile
else
  for NIC in $NICS
  do
    prepare_nic $NIC $FREQ
    sleep 0.1
  done
fi
fi

touch /tmp/nics_configured # let other processes know nics are setup and ready
}


function check_alive_function {
  # function to check if packets coming in, if not, re-start hello_video to clear frozen display
  while true; do
    # pause while saving is in progress
    pause_while
    ALIVE=`nice /root/wifibroadcast/check_alive`
    if [ $ALIVE == "0" ]; then
      echo "no new packets, restarting hello_video and sleeping for 5s ..."
      ps -ef | nice grep "cat /root/videofifo1" | nice grep -v grep | awk '{print $2}' | xargs kill -9
      ps -ef | nice grep "$DISPLAY_PROGRAM" | nice grep -v grep | awk '{print $2}' | xargs kill -9
      ionice -c 1 -n 4 nice -n -10 cat /root/videofifo1 | ionice -c 1 -n 4 nice -n -10 $DISPLAY_PROGRAM > /dev/null 2>&1 &
      sleep 5
    else
      echo "received packets, doing nothing ..."
    fi
  done
}


function check_exitstatus {
  STATUS=$1
  case $STATUS in
    9)
    # rx returned with exit code 9 = the interface went down
    # wifi card must've been removed during running
    # check if wifi card is really gone
    NICS2=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
    if [ "$NICS" == "$NICS2" ]; then
      # wifi card has not been removed, something else must've gone wrong
      echo "ERROR: RX stopped, wifi card _not_ removed!             "
    else
      # wifi card has been removed
      echo "ERROR: Wifi card removed!                               "
    fi
    ;;
    2)
    # something else that is fatal happened during running
    echo "ERROR: RX chain stopped wifi card _not_ removed!             "
    ;;
    1)
    # something that is fatal went wrong at rx startup
    echo "ERROR: could not start RX                           "
    #echo "ERROR: could not start RX                           "
    ;;
    *)
    if [  $RX_EXITSTATUS -lt 128 ]; then
      # whatever it was ...
      #echo "RX exited with status: $RX_EXITSTATUS                        "
      echo -n ""
    fi
  esac
}


function tx_function {
  killall wbc_status > /dev/null 2>&1

  /root/wifibroadcast/sharedmem_init_tx

  if [ "$TXMODE" == "single" ]; then
    echo -n "Waiting for wifi card to become ready ..."
    COUNTER=0
    # loop until card is initialized
    while [ $COUNTER -lt 10 ]; do
      sleep 0.5
      echo -n "."
      let "COUNTER++"
      if [ -d "/sys/class/net/wlan0" ]; then
        echo -n "card ready"
        break
      fi
    done
  else
    # just wait some time
    echo -n "Waiting for wifi cards to become ready ..."
    sleep 3
  fi

  echo
  echo
  detect_nics

  sleep 1
  echo

  if [ -e "$FC_TELEMETRY_SERIALPORT" ]; then
    echo "Configured serial port $FC_TELEMETRY_SERIALPORT found ..."
  else
    echo "ERROR: $FC_TELEMETRY_SERIALPORT not found!"
    collect_errorlog
    sleep 365d
  fi
  echo

  RALINK=0

  if [ "$TXMODE" == "single" ]; then
    DRIVER=`cat /sys/class/net/$NICS/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
    if [ "$DRIVER" != "ath9k_htc" ]; then # in single mode and ralink cards always, use frametype 1 (data)
    VIDEO_FRAMETYPE=0
    RALINK=1
  fi
else # for txmode dual always use frametype 1
  VIDEO_FRAMETYPE=1
  RALINK=1
fi

#echo "Wifi bitrate: $VIDEO_WIFI_BITRATE, Video frametype: $VIDEO_FRAMETYPE"

if [ "$VIDEO_WIFI_BITRATE" == "19.5" ]; then # set back to 18 to make sure -d parameter works (supports only 802.11b/g datarates)
VIDEO_WIFI_BITRATE=18
fi
if [ "$VIDEO_WIFI_BITRATE" == "5.5" ]; then # set back to 6 to make sure -d parameter works (supports only 802.11b/g datarates)
VIDEO_WIFI_BITRATE=5
fi

DRIVER=`cat /sys/class/net/$NICS/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'`
if [ "$CTS_PROTECTION" == "auto" ] && [ "$DRIVER" == "ath9k_htc" ]; then # only use CTS protection with Atheros
echo -n "Checking for other wifi traffic ... "
WIFIPPS=`/root/wifibroadcast/wifiscan $NICS`
echo -n "$WIFIPPS PPS: "
if [ "$WIFIPPS" != "0" ]; then # wifi networks detected, enable CTS
echo "Wifi traffic detected, CTS enabled"
VIDEO_FRAMETYPE=1
TELEMETRY_CTS=1
CTS=Y
else
  echo "No wifi traffic detected, CTS disabled"
  CTS=N
fi
else
  if [ "$CTS_PROTECTION" == "N" ]; then
    echo "CTS Protection disabled in config"
    CTS=N
  else
    if [ "$DRIVER" == "ath9k_htc" ]; then
      echo "CTS Protection enabled in config"
      CTS=Y
    else
      echo "CTS Protection not supported!"
      CTS=N
    fi
  fi
fi

# check if over-temp or under-voltage occured before bitrate measuring
if vcgencmd get_throttled | nice grep -q -v "0x0"; then
  TEMP=`cat /sys/class/thermal/thermal_zone0/temp`
  TEMP_C=$(($TEMP/1000))
  if [ "$TEMP_C" -lt 75 ]; then # it must be under-voltage
  echo
  echo "  ---------------------------------------------------------------------------------------------------"
  echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |"
  echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |"
  echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |"
  echo "  ---------------------------------------------------------------------------------------------------"
  echo
  mount -o remount,rw /boot
  echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  echo "  | When you have fixed wiring/power-supply, delete this file and make sure it doesn't re-appear!   |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
  mount -o remount,ro /boot
  UNDERVOLT=1
  echo "1" > /tmp/undervolt
else # it was either over-temp or both undervolt and over-temp, we set undervolt to 0 anyway, since overtemp can be seen at the temp display on the rx
  UNDERVOLT=0
  echo "0" > /tmp/undervolt
fi
else
  UNDERVOLT=0
  echo "0" > /tmp/undervolt
fi

# if yes, we don't do the bitrate measuring to increase chances we "survive"
if [ "$UNDERVOLT" == "0" ]; then
  if [ "$VIDEO_BITRATE" == "auto" ]; then
    echo -n "Measuring max. available bitrate .. "
    BITRATE_MEASURED=`/root/wifibroadcast/tx_measure -p 77 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEO_BLOCKLENGTH -t $VIDEO_FRAMETYPE -d $VIDEO_WIFI_BITRATE -y 0 $NICS`
    BITRATE=$((BITRATE_MEASURED*$BITRATE_PERCENT/100))
    BITRATE_KBIT=$((BITRATE/1000))
    BITRATE_MEASURED_KBIT=$((BITRATE_MEASURED/1000))
    echo "$BITRATE_MEASURED_KBIT kBit/s * $BITRATE_PERCENT% = $BITRATE_KBIT kBit/s video bitrate"
  else
    BITRATE=$(($VIDEO_BITRATE*1000))
    echo "Using fixed bitrate: $VIDEO_BITRATE kBit"
  fi
else
  BITRATE=$((1000*1000))
  BITRATE_KBIT=1000
  BITRATE_MEASURED_KBIT=2000
  echo "Using reduced bitrate: 1000 kBit due to undervoltage!"
fi

# check again if over-temp or under-voltage occured after bitrate measuring (but only if it didn't occur before yet)
if [ "$UNDERVOLT" == "0" ]; then
  if vcgencmd get_throttled | nice grep -q -v "0x0"; then
    TEMP=`cat /sys/class/thermal/thermal_zone0/temp`
    TEMP_C=$(($TEMP/1000))
    if [ "$TEMP_C" -lt 75 ]; then # it must be under-voltage
    echo
    echo "  ---------------------------------------------------------------------------------------------------"
    echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |"
    echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |"
    echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |"
    echo "  ---------------------------------------------------------------------------------------------------"
    echo
    mount -o remount,rw /boot
    echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | ERROR: Under-Voltage detected on the TX Pi. Your Pi is not supplied with stable 5 Volts.        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki. |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | Video Bitrate will be reduced to 1000kbit to reduce current consumption!                        |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  | When you have fixed wiring/power-supply, delete this file and make sure it doesn't re-appear!   |" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    echo "  ---------------------------------------------------------------------------------------------------" >> /boot/UNDERVOLTAGE-ERROR!!!.txt
    mount -o remount,ro /boot
    UNDERVOLT=1
    echo "1" > /tmp/undervolt
    BITRATE=$((1000*1000))
    BITRATE_KBIT=1000
    BITRATE_MEASURED_KBIT=2000
  else # it was either over-temp or both undervolt and over-temp, we set undervolt to 0 anyway, since overtemp can be seen at the temp display on the rx
    UNDERVOLT=0
    echo "0" > /tmp/undervolt
  fi
else
  UNDERVOLT=0
  echo "0" > /tmp/undervolt
fi
fi

# check for over-current on USB bus (due to card being powered via usb instead directly)
if nice dmesg | nice grep -q over-current; then
  echo "ERROR: Over-current detected - potential power supply problems!"
  collect_errorlog
  sleep 365d
fi

# check for USB disconnects (due to power-supply problems)
if nice dmesg | nice grep -q disconnect; then
  echo "ERROR: USB disconnect detected - potential power supply problems!"
  collect_errorlog
  sleep 365d
fi

echo $BITRATE_KBIT > /tmp/bitrate_kbit
echo $BITRATE_MEASURED_KBIT > /tmp/bitrate_measured_kbit

if [ "$CTS" == "N" ]; then
  echo "0" > /tmp/cts
else
  if [ "$VIDEO_WIFI_BITRATE" == "11" ] || [ "$VIDEO_WIFI_BITRATE" == "5" ]; then # 11mbit and 5mbit bitrates don't support CTS, so set to 0
  echo "0" > /tmp/cts
else
  echo "1" > /tmp/cts
fi
fi

if [ "$DEBUG" == "Y" ]; then
  collect_debug /boot &
fi

/root/wifibroadcast/rssitx $NICS &

echo
echo "Starting transmission in $TXMODE mode, FEC $VIDEO_BLOCKS/$VIDEO_FECS/$VIDEO_BLOCKLENGTH: $WIDTH x $HEIGHT $FPS fps, video bitrate: $BITRATE_KBIT kBit/s, Keyframerate: $KEYFRAMERATE"
nice -n -9 raspivid -w $WIDTH -h $HEIGHT -fps $FPS -b $BITRATE -g $KEYFRAMERATE -t 0 $EXTRAPARAMS -o - | nice -n -9 /root/wifibroadcast/tx_rawsock -p 0 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEO_BLOCKLENGTH -t $VIDEO_FRAMETYPE -d $VIDEO_WIFI_BITRATE -y 0 $NICS

#    v4l2-ctl -d /dev/video0 --set-fmt-video=width=1280,height=720,pixelformat='H264' -p 48 --set-ctrl video_bitrate=7000000,repeat_sequence_header=1,h264_i_frame_period=7,white_balance_auto_preset=5
#    nice -n -9 cat /dev/video0 | /root/wifibroadcast/tx_rawsock -p 0 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEO_BLOCKLENGTH -t $VIDEO_FRAMETYPE -d $VIDEO_WIFI_BITRATE -y 0 $NICS

TX_EXITSTATUS=${PIPESTATUS[1]}
# if we arrive here, either raspivid or tx did not start, or were terminated later
# check if NIC has been removed
NICS2=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
if [ "$NICS" == "$NICS2" ]; then
  # wifi card has not been removed
  if [ "$TX_EXITSTATUS" != "0" ]; then
    echo "ERROR: could not start tx or tx terminated!"
  fi
  collect_errorlog
  sleep 365d
else
  # wifi card has been removed
  echo "ERROR: Wifi card removed!"
  collect_errorlog
  sleep 365d
fi
}



function rx_function {
  /root/wifibroadcast/sharedmem_init_rx

  # start virtual serial port for cmavnode and ser2net
  ionice -c 3 nice socat -lf /wbc_tmp/socat1.log -d -d pty,raw,echo=0 pty,raw,echo=0 & > /dev/null 2>&1
  sleep 1
  ionice -c 3 nice socat -lf /wbc_tmp/socat2.log -d -d pty,raw,echo=0 pty,raw,echo=0 & > /dev/null 2>&1
  sleep 1
  # setup virtual serial ports
  stty -F /dev/pts/0 -icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon 57600
  stty -F /dev/pts/1 -icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon 115200

  echo

  # if USB memory stick is already connected during startup, notify user
  # and pause as long as stick is not removed
  # some sticks show up as sda1, others as sda, check for both
  if [ -e "/dev/sda1" ]; then
    STARTUSBDEV="/dev/sda1"
  else
    STARTUSBDEV="/dev/sda"
  fi

  if [ -e $STARTUSBDEV ]; then
    touch /tmp/donotsave
    STICKGONE=0
    while [ $STICKGONE -ne 1 ]; do
      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "USB memory stick detected - please remove and re-plug after flight" 7 65 0 &
      sleep 4
      if [ ! -e $STARTUSBDEV ]; then
        STICKGONE=1
        rm /tmp/donotsave
      fi
    done
  fi

  killall wbc_status > /dev/null 2>&1

  sleep 1
  detect_nics
  echo

  sleep 0.5

  # videofifo1: local display, hello_video.bin
  # videofifo2: secondary display, hotspot/usb-tethering
  # videofifo3: recording
  # videofifo4: wbc relay

  if [ "$VIDEO_TMP" == "sdcard" ]; then
    touch /tmp/pausewhile # make sure check_alive doesn't do it's stuff ...
    tmessage "Saving to SDCARD enabled, preparing video storage ..."
    if cat /proc/partitions | nice grep -q mmcblk0p3; then # partition has not been created yet
    echo
  else
    echo
    echo -e "n\np\n3\n3674112\n\nw" | fdisk /dev/mmcblk0 > /dev/null 2>&1
    partprobe > /dev/null 2>&1
    mkfs.ext4 /dev/mmcblk0p3 -F > /dev/null 2>&1 || {
      tmessage "ERROR: Could not format video storage on SDCARD!"
      collect_errorlog
      sleep 365d
    }
  fi
  e2fsck -p /dev/mmcblk0p3 > /dev/null 2>&1
  mount -t ext4 -o noatime /dev/mmcblk0p3 /video_tmp > /dev/null 2>&1 || {
    tmessage "ERROR: Could not mount video storage on SDCARD!"
    collect_errorlog
    sleep 365d
  }
  VIDEOFILE=/video_tmp/videotmp.raw
  echo "VIDEOFILE=/video_tmp/videotmp.raw" > /tmp/videofile
  rm $VIDEOFILE > /dev/null 2>&1
else
  VIDEOFILE=/wbc_tmp/videotmp.raw
  echo "VIDEOFILE=/wbc_tmp/videotmp.raw" > /tmp/videofile
fi

#/root/wifibroadcast/tracker /wifibroadcast_rx_status_0 >> /wbc_tmp/tracker.txt &
#sleep 1

killall wbc_status > /dev/null 2>&1

if [ "$AIRODUMP" == "Y" ]; then
  touch /tmp/pausewhile # make sure check_alive doesn't do it's stuff ...
  echo "AiroDump is enabled, running airodump-ng for $AIRODUMP_SECONDS seconds ..."
  sleep 3
  # strip newlines
  NICS_COMMA=`echo $NICS | tr '\n' ' '`
  # strip space at end
  NICS_COMMA=`echo $NICS_COMMA | sed 's/ *$//g'`
  # replace spaces by comma
  NICS_COMMA=${NICS_COMMA// /,}

  if [ "$FREQ" -gt 3000 ]; then
    AIRODUMP_CHANNELS="5180,5200,5220,5240,5260,5280,5300,5320,5500,5520,5540,5560,5580,5600,5620,5640,5660,5680,5700,5745,5765,5785,5805,5825"
  else
    AIRODUMP_CHANNELS="2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472"
  fi

  airodump-ng --showack -h --berlin 60 --ignore-negative-one --manufacturer --output-format pcap --write /wbc_tmp/wifiscan --write-interval 2 -C $AIRODUMP_CHANNELS  $NICS_COMMA &
  sleep $AIRODUMP_SECONDS
  sleep 2
  ionice -c 3 nice -n 19 /root/wifibroadcast_misc/raspi2png -p /wbc_tmp/airodump.png >> /dev/null
  killall airodump-ng
  sleep 1
  printf "\033c"
  for NIC in $NICS
  do
    iw dev $NIC set freq $FREQ
  done
  sleep 1
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
  echo
fi

if [ "$DEBUG" == "Y" ]; then
  collect_debug /wbc_tmp &
fi
wbclogger_function &

if vcgencmd get_throttled | nice grep -q -v "0x0"; then
  TEMP=`cat /sys/class/thermal/thermal_zone0/temp`
  TEMP_C=$(($TEMP/1000))
  if [ "$TEMP_C" -lt 75 ]; then
    echo "  ---------------------------------------------------------------------------------------------------"
    echo "  | ERROR: Under-Voltage detected on the RX Pi. Your Pi is not supplied with stable 5 Volts.        |"
    echo "  |                                                                                                 |"
    echo "  | Either your power-supply or wiring is not sufficent, check the wiring instructions in the Wiki! |"
    echo "  ---------------------------------------------------------------------------------------------------"
    echo "1" >> /tmp/undervolt
    sleep 5
  fi
  echo "0" >> /tmp/undervolt
else
  echo "0" >> /tmp/undervolt
fi

if [ -e "/tmp/pausewhile" ]; then
  rm /tmp/pausewhile # remove pausewhile file to make sure check_alive and everything runs again
fi

while true; do
  pause_while

  ionice -c 1 -n 4 nice -n -10 cat /root/videofifo1 | ionice -c 1 -n 4 nice -n -10 $DISPLAY_PROGRAM > /dev/null 2>&1 &
  ionice -c 3 nice cat /root/videofifo3 >> $VIDEOFILE &

  if [ "$RELAY" == "Y" ]; then
    ionice -c 1 -n 4 nice -n -10 cat /root/videofifo4 | /root/wifibroadcast/tx_rawsock -p 0 -b $RELAY_VIDEO_BLOCKS -r $RELAY_VIDEO_FECS -f $RELAY_VIDEO_BLOCKLENGTH -t $VIDEO_FRAMETYPE -d 24 -y 0 relay0 > /dev/null 2>&1 &
  fi

  # update NICS variable in case a NIC has been removed (exclude devices with wlanx)
  NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v wlan | nice grep -v relay | nice grep -v wifihotspot`

  tmessage "Starting RX ... (FEC: $VIDEO_BLOCKS/$VIDEO_FECS/$VIDEO_BLOCKLENGTH)"
  ionice -c 1 -n 3 /root/wifibroadcast/rx -p 0 -d 1 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEO_BLOCKLENGTH $NICS | ionice -c 1 -n 4 nice -n -10 tee >(ionice -c 1 -n 4 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo2 > /dev/null 2>&1) >(ionice -c 1 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo4 > /dev/null 2>&1) >(ionice -c 3 nice /root/wifibroadcast_misc/ftee /root/videofifo3 > /dev/null 2>&1) | ionice -c 1 -n 4 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo1 > /dev/null 2>&1

  RX_EXITSTATUS=${PIPESTATUS[0]}
  check_exitstatus $RX_EXITSTATUS
  ps -ef | nice grep "$DISPLAY_PROGRAM" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "rx -p 0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "ftee /root/videofifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "cat /root/videofifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9
done
}


function rssirx_function {
  echo
  echo -n "Waiting until video is running ..."
  VIDEORXRUNNING=0
  while [ $VIDEORXRUNNING -ne 1 ]; do
    sleep 0.5
    VIDEORXRUNNING=`pidof $DISPLAY_PROGRAM | wc -w`
    echo -n "."
  done
  echo
  # get NICS (exclude devices with wlanx)
  NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v wlan | nice grep -v relay | nice grep -v wifihotspot`
  echo "Starting RSSI RX ..."
  nice /root/wifibroadcast/rssirx $NICS
}


## runs on RX (ground pi)
function osdrx_function {
  echo
  # Convert osdconfig from DOS format to UNIX format
  ionice -c 3 nice dos2unix -n /boot/osdconfig.txt /tmp/osdconfig.txt
  echo
  cd /root/wifibroadcast_osd
  echo Building OSD:
  ionice -c 3 nice make -j2 || {
    echo
    echo "ERROR: Could not build OSD, check osdconfig.txt!"
    sleep 5
    nice /root/wifibroadcast_status/wbc_status "ERROR: Could not build OSD, check osdconfig.txt for errors." 7 55 0
    sleep 5
  }
  echo

  while true; do
    killall wbc_status > /dev/null 2>&1

    echo -n "Waiting until video is running ..."
    VIDEORXRUNNING=0
    while [ $VIDEORXRUNNING -ne 1 ]; do
      sleep 0.5
      VIDEORXRUNNING=`pidof rx | wc -w`
      echo -n "."
    done
    echo
    echo "Video running, starting OSD processes ..."

    if [ "$TELEMETRY_TRANSMISSION" == "wbc" ]; then
      echo "Telemetry transmission WBC chosen, using wbc rx"
      TELEMETRY_RX_CMD="/root/wifibroadcast/rx_rc_telemetry_buf -p 1 -o 1 -r 99"
    elif [ "$TELEMETRY_TRANSMISSION" == "external" ]; then
      echo "Telemetry transmission external chosen, using cat from serialport"
      nice stty -F $EXTERNAL_TELEMETRY_SERIALPORT_GROUND $EXTERNAL_TELEMETRY_SERIALPORT_GROUND_STTY_OPTIONS $EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE
      #nice /root/wifibroadcast/setupuart -d 0 -s $EXTERNAL_TELEMETRY_SERIALPORT_GROUND -b $EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE
      TELEMETRY_RX_CMD="cat $EXTERNAL_TELEMETRY_SERIALPORT_GROUND"
    fi

    if [ "$ENABLE_SERIAL_TELEMETRY_OUTPUT" == "Y" ]; then
      echo "enable_serial_telemetry_output is Y, sending telemetry stream to $TELEMETRY_OUTPUT_SERIALPORT_GROUND"
      nice stty -F $TELEMETRY_OUTPUT_SERIALPORT_GROUND $TELEMETRY_OUTPUT_SERIALPORT_GROUND_STTY_OPTIONS $TELEMETRY_OUTPUT_SERIALPORT_GROUND_BAUDRATE
      #nice /root/wifibroadcast/setupuart -d 1 -s $TELEMETRY_OUTPUT_SERIALPORT_GROUND -b $TELEMETRY_OUTPUT_SERIALPORT_GROUND_BAUDRATE
      nice cat /root/telemetryfifo6 > $TELEMETRY_OUTPUT_SERIALPORT_GROUND &
    fi

    # telemetryfifo1: local display, osd
    # telemetryfifo2: secondary display, hotspot/usb-tethering
    # telemetryfifo3: recording
    # telemetryfifo4: wbc relay
    # telemetryfifo5: mavproxy downlink
    # telemetryfifo6: serial downlink

    ionice -c 3 nice cat /root/telemetryfifo3 >> /wbc_tmp/telemetrydowntmp.raw &
    pause_while
    /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &

    if [ "$RELAY" == "Y" ]; then
      ionice -c 1 -n 4 nice -n -9 cat /root/telemetryfifo4 | nice /root/wifibroadcast/tx_telemetry -p 1 -c $TELEMETRY_CTS -r 2 -x $TELEMETRY_TYPE -d 12 -y 0 relay0 > /dev/null 2>&1 &
    fi

    # update NICS variable in case a NIC has been removed (exclude devices with wlanx)
    NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v wlan | nice grep -v relay | nice grep -v wifihotspot`

    if [ "$TELEMETRY_TRANSMISSION" == "wbc" ]; then
      if [ "$DEBUG" == "Y" ]; then
        $TELEMETRY_RX_CMD -d 1 $NICS 2>/wbc_tmp/telemetrydowndebug.txt | tee >(/root/wifibroadcast_misc/ftee /root/telemetryfifo2 > /dev/null 2>&1) >(/root/wifibroadcast_misc/ftee /root/telemetryfifo3 > /dev/null 2>&1) >(ionice -c 1 nice -n -9 /root/wifibroadcast_misc/ftee /root/telemetryfifo4 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo5 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo6 > /dev/null 2>&1) | /root/wifibroadcast_misc/ftee /root/telemetryfifo1 > /dev/null 2>&1
      else
        nice -n -5 $TELEMETRY_RX_CMD $NICS | tee >(/root/wifibroadcast_misc/ftee /root/telemetryfifo2 > /dev/null 2>&1) >(/root/wifibroadcast_misc/ftee /root/telemetryfifo3 > /dev/null 2>&1) >(ionice -c 1 nice -n -9 /root/wifibroadcast_misc/ftee /root/telemetryfifo4 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo5 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo6 > /dev/null 2>&1) | /root/wifibroadcast_misc/ftee /root/telemetryfifo1 > /dev/null 2>&1
      fi
    elif [ "$TELEMETRY_TRANSMISSION" == "external" ]; then
      $TELEMETRY_RX_CMD | tee >(/root/wifibroadcast_misc/ftee /root/telemetryfifo2 > /dev/null 2>&1) >(/root/wifibroadcast_misc/ftee /root/telemetryfifo3 > /dev/null 2>&1) >(ionice -c 1 nice -n -9 /root/wifibroadcast_misc/ftee /root/telemetryfifo4 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo5 > /dev/null 2>&1) >(ionice nice /root/wifibroadcast_misc/ftee /root/telemetryfifo6 > /dev/null 2>&1) | /root/wifibroadcast_misc/ftee /root/telemetryfifo1 > /dev/null 2>&1
    else
      sleep 365d
    fi
    echo "ERROR: Telemetry RX has been stopped - restarting RX and OSD ..."
    ps -ef | nice grep "rx -p 1" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    ps -ef | nice grep "ftee /root/telemetryfifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    ps -ef | nice grep "/tmp/osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    ps -ef | nice grep "cat /root/telemetryfifo3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    sleep 1
  done
}

## runs on TX (air pi)
function osdtx_function {
  # setup serial port
  stty -F $FC_TELEMETRY_SERIALPORT $FC_TELEMETRY_STTY_OPTIONS $FC_TELEMETRY_BAUDRATE

  echo
  echo -n "Waiting until nics are configured ..."
  while [ ! -f /tmp/nics_configured ]; do
    sleep 0.5
    #echo -n "."
  done
  sleep 1
  echo
  echo "nics configured, starting Downlink telemetry TX processes ..."

  NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi`

  echo "telemetry CTS: $TELEMETRY_CTS"

  echo
  while true; do
    echo "Starting downlink telemetry transmission in $TXMODE mode (FC Serialport: $FC_TELEMETRY_SERIALPORT)"
    nice cat $FC_TELEMETRY_SERIALPORT | nice /root/wifibroadcast/tx_telemetry -p 1 -c $TELEMETRY_CTS -r 2 -x $TELEMETRY_TYPE -d 12 -y 0 $NICS
    ps -ef | nice grep "cat $FC_TELEMETRY_SERIALPORT" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    ps -ef | nice grep "tx_telemetry -p 1" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    echo "Downlink Telemetry TX exited - restarting ..."
    sleep 1
  done
}

## runs on RX (ground pi)
function uplinktx_function {
  # wait until video is running to make sure NICS are configured
  echo
  echo -n "Waiting until video is running ..."
  VIDEORXRUNNING=0
  while [ $VIDEORXRUNNING -ne 1 ]; do
    VIDEORXRUNNING=`pidof $DISPLAY_PROGRAM | wc -w`
    sleep 1
    echo -n "."
  done
  sleep 1
  echo
  echo

  while true; do
    echo "Starting uplink telemetry transmission"
    if [ "$TELEMETRY_TRANSMISSION" == "wbc" ]; then
      echo "telemetry transmission = wbc, starting tx_telemetry ..."
      NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
      echo -n "NICS:"
      echo $NICS
      if [ "$TELEMETRY_UPLINK" == "mavlink" ]; then
        VSERIALPORT=/dev/pts/0
        UPLINK_TX_CMD="nice /root/wifibroadcast/tx_telemetry -p 3 -c 0 -r 2 -x 0 -d 12 -y 0"
      else # MSP
        VSERIALPORT=/dev/pts/2
        UPLINK_TX_CMD="nice /root/wifibroadcast/tx_telemetry -p 3 -c 0 -r 2 -x 1 -d 12 -y 0"
      fi

      if [ "$DEBUG" == "Y" ]; then
        nice cat $VSERIALPORT | $UPLINK_TX_CMD -z 1 $NICS 2>/wbc_tmp/telemetryupdebug.txt
      else
        nice cat $VSERIALPORT | $UPLINK_TX_CMD $NICS
      fi
    elif [ "$TELEMETRY_TRANSMISSION" == "external" ]; then
      echo "telemetry transmission = external, sending data to $EXTERNAL_TELEMETRY_SERIALPORT_GROUND ..."
      nice stty -F $EXTERNAL_TELEMETRY_SERIALPORT_GROUND $EXTERNAL_TELEMETRY_SERIALPORT_GROUND_STTY_OPTIONS $EXTERNAL_TELEMETRY_SERIALPORT_GROUND_BAUDRATE
      if [ "$TELEMETRY_UPLINK" == "mavlink" ]; then
        VSERIALPORT=/dev/pts/0
      else # MSP
        VSERIALPORT=/dev/pts/2
      fi
      UPLINK_TX_CMD="$EXTERNAL_TELEMETRY_SERIALPORT_GROUND"
      if [ "$DEBUG" == "Y" ]; then
        nice cat $VSERIALPORT > $UPLINK_TX_CMD
      else
        nice cat $VSERIALPORT > $UPLINK_TX_CMD
      fi
    fi
    ps -ef | nice grep "cat $VSERIALPORT" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    ps -ef | nice grep "tx_telemetry -p 3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  done
}

# runs on RX (ground pi)
function rctx_function {
  # Convert joystick config from DOS format to UNIX format
  ionice -c 3 nice dos2unix -n /boot/joyconfig.txt /tmp/rctx.h > /dev/null 2>&1
  echo
  echo Building RC ...
  cd /root/wifibroadcast_rc
  ionice -c 3 nice gcc -lrt -lpcap rctx.c -o /tmp/rctx `sdl-config --libs` `sdl-config --cflags` || {
    echo "ERROR: Could not build RC, check joyconfig.txt!"
  }
  # wait until video is running to make sure NICS are configured and wifibroadcast_rx_status shmem is available

  echo
  echo -n "Waiting until nics are configured ..."
  while [ ! -f /tmp/nics_configured ]; do
    sleep 0.5
    echo -n "."
  done
  sleep 0.5

  NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
  #echo "NICS: $NICS"
  pause_while
  echo
  echo "Starting R/C TX ..."
  while true; do
    nice -n -5 /tmp/rctx $NICS
  done
}

## runs on TX (air pi)
function uplinkrx_and_rcrx_function {
  echo "FC_TELEMETRY_SERIALPORT: $FC_TELEMETRY_SERIALPORT"
  echo "FC_MSP_SERIALPORT: $FC_MSP_SERIALPORT"
  echo "FC_RC_SERIALPORT: $FC_RC_SERIALPORT"
  echo
  echo -n "Waiting until nics are configured ..."
  while [ ! -f /tmp/nics_configured ]; do
    sleep 0.5
    #echo -n "."
  done
  sleep 1

  NICS=`ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot`
  echo -n "NICS:"
  echo $NICS
  echo

  stty -F $FC_TELEMETRY_SERIALPORT $FC_TELEMETRY_STTY_OPTIONS $FC_TELEMETRY_BAUDRATE

  echo "Starting Uplink telemetry and R/C RX ..."
  if [ "$RC" != "disabled" ]; then # with R/C
  case $RC in
    "msp")
    RC_PROTOCOL=0
    ;;
    "mavlink")
    RC_PROTOCOL=1
    ;;
    "sumd")
    RC_PROTOCOL=2
    ;;
    "ibus")
    RC_PROTOCOL=3
    ;;
    "srxl")
    RC_PROTOCOL=4
    ;;
  esac
  if [ "$FC_TELEMETRY_SERIALPORT" == "$FC_RC_SERIALPORT" ]; then # TODO: check if this logic works in all cases
  if [ "$TELEMETRY_UPLINK" == "mavlink" ]; then # use the telemetry serialport and baudrate as it's the same anyway
  /root/wifibroadcast/rx_rc_telemetry -p 3 -o 0 -b $FC_TELEMETRY_BAUDRATE -s $FC_TELEMETRY_SERIALPORT -r $RC_PROTOCOL $NICS
else # use the configured r/c serialport and baudrate
  /root/wifibroadcast/rx_rc_telemetry -p 3 -o 0 -b $FC_RC_BAUDRATE -s $FC_RC_SERIALPORT -r $RC_PROTOCOL $NICS
fi
else
  #/root/wifibroadcast/setupuart -d 1 -s $FC_TELEMETRY_SERIALPORT -b $FC_TELEMETRY_BAUDRATE
  /root/wifibroadcast/rx_rc_telemetry -p 3 -o 1 -b $FC_RC_BAUDRATE -s $FC_RC_SERIALPORT -r $RC_PROTOCOL $NICS > $FC_TELEMETRY_SERIALPORT
fi
else # without R/C
  #/root/wifibroadcast/setupuart -d 1 -s $FC_TELEMETRY_SERIALPORT -b $FC_TELEMETRY_BAUDRATE
  nice /root/wifibroadcast/rx_rc_telemetry -p 3 -o 1 -r 99 $NICS > $FC_TELEMETRY_SERIALPORT
fi
}


function screenshot_function {
  while true; do
    # pause loop while saving is in progress
    pause_while
    SCALIVE=`nice /root/wifibroadcast/check_alive`
    # do nothing if no video being received (so we don't take unnecessary screeshots)
    LIMITFREE=3000 # 3 mbyte
    if [ "$SCALIVE" == "1" ]; then
      # check if tmp disk is full, if yes, do not save screenshot
      FREETMPSPACE=`df -P /wbc_tmp/ | awk 'NR==2 {print $4}'`
      if [ $FREETMPSPACE -gt $LIMITFREE ]; then
        PNG_NAME=/wbc_tmp/screenshot`ls /wbc_tmp/screenshot* | wc -l`.png
        echo "Taking screenshot: $PNG_NAME"
        ionice -c 3 nice -n 19 /root/wifibroadcast_misc/raspi2png -p $PNG_NAME
      else
        echo "RAM disk full - no screenshot taken ..."
      fi
    else
      echo "Video not running - no screenshot taken ..."
    fi
    sleep 5
  done
}


function save_function {
  # let screenshot and check_alive function know that saving is in progrss
  touch /tmp/pausewhile
  # kill OSD so we can safeley start wbc_status
  ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  # kill video and telemetry recording and also local video display
  ps -ef | nice grep "cat /root/videofifo3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "cat /root/telemetryfifo3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "$DISPLAY_PROGRAM" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "cat /root/videofifo1" | nice grep -v grep | awk '{print $2}' | xargs kill -9

  # kill video rx
  ps -ef | nice grep "rx -p 0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
  ps -ef | nice grep "ftee /root/videofifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9

  # find out if video is on ramdisk or sd
  source /tmp/videofile
  echo "VIDEOFILE: $VIDEOFILE"

  # start re-play of recorded video ....
  nice /opt/vc/src/hello_pi/hello_video/hello_video.bin.player $VIDEOFILE $FPS &

  killall wbc_status > /dev/null 2>&1
  nice /root/wifibroadcast_status/wbc_status "Saving to USB. This may take some time ..." 7 55 0 &

  echo -n "Accessing file system.. "

  # some sticks show up as sda1, others as sda, check for both
  if [ -e "/dev/sda1" ]; then
    USBDEV="/dev/sda1"
  else
    USBDEV="/dev/sda"
  fi

  echo "USBDEV: $USBDEV"

  if mount $USBDEV /media/usb; then
    TELEMETRY_SAVE_PATH="/telemetry"
    SCREENSHOT_SAVE_PATH="/screenshot"
    VIDEO_SAVE_PATH="/video"
    RSSI_SAVE_PATH=/"rssi"

    if [ -d "/media/usb$RSSI_SAVE_PATH" ]; then
      echo "RSSI save path $RSSI_SAVE_PATH found"
    else
      echo "Creating RSSI save path $RSSI_SAVE_PATH.. "
      mkdir /media/usb$RSSI_SAVE_PATH
    fi

    if [ -d "/media/usb$TELEMETRY_SAVE_PATH" ]; then
      echo "Telemetry save path $TELEMETRY_SAVE_PATH found"
    else
      echo "Creating Telemetry save path $TELEMETRY_SAVE_PATH.. "
      mkdir /media/usb$TELEMETRY_SAVE_PATH
    fi

    killall rssilogger
    killall syslogger
    killall wifibackgroundscan

    gnuplot -e "load '/root/gnuplot/videorssi.gp'" &
    gnuplot -e "load '/root/gnuplot/videopackets.gp'"
    gnuplot -e "load '/root/gnuplot/telemetrydownrssi.gp'" &
    gnuplot -e "load '/root/gnuplot/telemetrydownpackets.gp'"

    if [ "$TELEMETRY_UPLINK" != "disabled" ]; then
      gnuplot -e "load '/root/gnuplot/telemetryuprssi.gp'" &
      gnuplot -e "load '/root/gnuplot/telemetryuppackets.gp'"
    fi
    if [ "$RC" != "disabled" ]; then
      gnuplot -e "load '/root/gnuplot/rcrssi.gp'" &
      gnuplot -e "load '/root/gnuplot/rcpackets.gp'"
    fi

    if [ "$DEBUG" == "Y" ]; then
      gnuplot -e "load '/root/gnuplot/wifibackgroundscan.gp'" &
    fi

    cp /wbc_tmp/*.csv /media/usb$RSSI_SAVE_PATH/

    if [ -s "/wbc_tmp/telemetrydowntmp.raw" ]; then
      cp /wbc_tmp/telemetrydowntmp.raw /media/usb$TELEMETRY_SAVE_PATH/telemetrydown`ls /media/usb$TELEMETRY_SAVE_PATH/*.raw | wc -l`.raw
      cp /wbc_tmp/telemetrydowntmp.txt /media/usb$TELEMETRY_SAVE_PATH/telemetrydown`ls /media/usb$TELEMETRY_SAVE_PATH/*.txt | wc -l`.txt
    fi

    killall tshark
    cp /wbc_tmp/*.pcap /media/usb
    cp /wbc_tmp/*.cap /media/usb

    cp /wbc_tmp/airodump.png /media/usb

    if [ "$ENABLE_SCREENSHOTS" == "Y" ]; then
      if [ -d "/media/usb$SCREENSHOT_SAVE_PATH" ]; then
        echo "Screenshots save path $SCREENSHOT_SAVE_PATH found"
      else
        echo "Creating screenshots save path $SCREENSHOT_SAVE_PATH.. "
        mkdir /media/usb$SCREENSHOT_SAVE_PATH
      fi
      DIR_NAME_SCREENSHOT=/media/usb$SCREENSHOT_SAVE_PATH/`ls /media/usb$SCREENSHOT_SAVE_PATH | wc -l`
      mkdir $DIR_NAME_SCREENSHOT
      cp /wbc_tmp/screenshot* $DIR_NAME_SCREENSHOT > /dev/null 2>&1
    fi

    if [ -s "$VIDEOFILE" ]; then
      if [ -d "/media/usb$VIDEO_SAVE_PATH" ]; then
        echo "Video save path $VIDEO_SAVE_PATH found"
      else
        echo "Creating video save path $VIDEO_SAVE_PATH.. "
        mkdir /media/usb$VIDEO_SAVE_PATH
      fi
      FILE_NAME_AVI=/media/usb$VIDEO_SAVE_PATH/video`ls /media/usb$VIDEO_SAVE_PATH | wc -l`.avi
      echo "FILE_NAME_AVI: $FILE_NAME_AVI"
      nice avconv -framerate $FPS -i $VIDEOFILE -vcodec copy $FILE_NAME_AVI > /dev/null 2>&1 &
      AVCONVRUNNING=1
      while [ $AVCONVRUNNING -eq 1 ]; do
        AVCONVRUNNING=`pidof avconv | wc -w`
        #echo "AVCONVRUNNING: $AVCONVRUNNING"
        sleep 4
        killall wbc_status > /dev/null 2>&1
        nice /root/wifibroadcast_status/wbc_status "Saving - please wait ..." 7 65 0 &
      done
    fi
    #cp /wbc_tmp/tracker.txt /media/usb/
    cp /wbc_tmp/debug.txt /media/usb/
    cp /wbc_tmp/telemetrydowndebug.txt /media/usb$TELEMETRY_SAVE_PATH/
    cp /wbc_tmp/telemetryupdebug.txt /media/usb$TELEMETRY_SAVE_PATH/

    nice umount /media/usb
    STICKGONE=0
    while [ $STICKGONE -ne 1 ]; do
      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Done - USB memory stick can be removed now" 7 65 0 &
      sleep 4
      if [ ! -e "/dev/sda" ]; then
        STICKGONE=1
      fi
    done
    killall wbc_status > /dev/null 2>&1
    killall hello_video.bin.player > /dev/null 2>&1
    rm /wbc_tmp/* > /dev/null 2>&1
    rm /video_tmp/* > /dev/null 2>&1
    sync
  else
    STICKGONE=0
    while [ $STICKGONE -ne 1 ]; do
      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "ERROR: Could not access USB memory stick!" 7 65 0 &
      sleep 4
      if [ ! -e "/dev/sda" ]; then
        STICKGONE=1
      fi
    done
    killall wbc_status > /dev/null 2>&1
    killall hello_video.bin.player > /dev/null 2>&1
  fi

  #killall tracker
  # re-start video/telemetry recording
  ionice -c 3 nice cat /root/videofifo3 >> $VIDEOFILE &
  ionice -c 3 nice cat /root/telemetryfifo3 >> /wbc_tmp/telemetrydowntmp.raw &

  killall wbc_status > /dev/null 2>&1
  OSDRUNNING=`pidof /tmp/osd | wc -w`
  if [ $OSDRUNNING  -ge 1 ]; then
    echo "OSD already running!"
  else
    killall wbc_status > /dev/null 2>&1
    /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
  fi
  # let screenshot function know that it can continue taking screenshots
  rm /tmp/pausewhile
}

function wbclogger_function {
  # Waiting until video is running ...
  VIDEORXRUNNING=0
  while [ $VIDEORXRUNNING -ne 1 ]; do
    VIDEORXRUNNING=`pidof rx | wc -w`
    sleep 1
  done
  echo
  sleep 5
  nice /root/wifibroadcast/rssilogger /wifibroadcast_rx_status_0 >> /wbc_tmp/videorssi.csv &
  nice /root/wifibroadcast/rssilogger /wifibroadcast_rx_status_1 >> /wbc_tmp/telemetrydownrssi.csv &
  nice /root/wifibroadcast/syslogger /wifibroadcast_rx_status_sysair >> /wbc_tmp/system.csv &

  if [ "$TELEMETRY_UPLINK" != "disabled" ]; then
    nice /root/wifibroadcast/rssilogger /wifibroadcast_rx_status_uplink >> /wbc_tmp/telemetryuprssi.csv &
  fi
  if [ "$RC" != "disabled" ]; then
    nice /root/wifibroadcast/rssilogger /wifibroadcast_rx_status_rc >> /wbc_tmp/rcrssi.csv &
  fi

  if [ "$DEBUG" == "Y" ]; then
    nice /root/wifibroadcast/wifibackgroundscan $NICS >> /wbc_tmp/wifibackgroundscan.csv &
  fi
  sleep 365d
}

function pause_while {
  if [ -f "/tmp/pausewhile" ]; then
    PAUSE=1
    while [ $PAUSE -ne 0 ]; do
      if [ ! -f "/tmp/pausewhile" ]; then
        PAUSE=0
      fi
      sleep 1
    done
  fi
}

function tether_check_function {
  while true; do
    # pause loop while saving is in progress
    pause_while
    if [ -d "/sys/class/net/usb0" ]; then
      echo
      echo "USB tethering device detected. Configuring IP ..."
      nice pump -h wifibrdcast -i usb0 --no-dns --keep-up --no-resolvconf --no-ntp || {
        echo "ERROR: Could not configure IP for USB tethering device!"
        nice killall wbc_status > /dev/null 2>&1
        nice /root/wifibroadcast_status/wbc_status "ERROR: Could not configure IP for USB tethering device!" 7 55 0
        collect_errorlog
        sleep 365d
      }
      # find out smartphone IP to send video stream to
      PHONE_IP=`ip route show 0.0.0.0/0 dev usb0 | cut -d\  -f3`
      echo "Android IP: $PHONE_IP"

      nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$PHONE_IP:$TELEMETRY_UDP_PORT &
      nice /root/wifibroadcast/rssi_forward $PHONE_IP 5003 &

      if [ "$FORWARD_STREAM" == "rtp" ]; then
        ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$PHONE_IP > /dev/null 2>&1 &
      else
        ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$PHONE_IP:$VIDEO_UDP_PORT &
      fi

      if cat /boot/osdconfig.txt | grep -q "^#define MAVLINK"; then
        cat /root/telemetryfifo5 > /dev/pts/0 &
        if [ "$MAVLINK_FORWARDER" == "mavlink-routerd" ]; then
          ionice -c 3 nice /root/mavlink-router/mavlink-routerd -e $PHONE_IP:14550 /dev/pts/1:57600 &
        elif ["$MAVLINK_FORWARDER" == "cmavnode"]; then
          cp /boot/cmavnode.conf /tmp/
          echo "targetip=$PHONE_IP" >> /tmp/cmavnode.conf
          ionice -c 3 nice /root/cmavnode/cmavnode --file /tmp/cmavnode.conf &
        fi

        if [ "$DEBUG" == "Y" ]; then
          tshark -i usb0 -f "udp and port 14550" -w /wbc_tmp/mavlink`date +%s`.pcap &
        fi
      fi

      if [ "$TELEMETRY_UPLINK" == "msp" ]; then
        cat /root/mspfifo > /dev/pts/2 &
        #socat /dev/pts/3 tcp-listen:23
        ser2net
      fi

      # kill and pause OSD so we can safeley start wbc_status
      ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Secondary display connected (USB)" 7 55 0

      # re-start osd
      killall wbc_status > /dev/null 2>&1
      OSDRUNNING=`pidof /tmp/osd | wc -w`
      if [ $OSDRUNNING  -ge 1 ]; then
        echo "OSD already running!"
      else
        killall wbc_status > /dev/null 2>&1
        /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
      fi

      # check if smartphone has been disconnected
      PHONETHERE=1
      while [  $PHONETHERE -eq 1 ]; do
        if [ -d "/sys/class/net/usb0" ]; then
          PHONETHERE=1
          echo "Android device still connected ..."
        else
          echo "Android device gone"
          # kill and pause OSD so we can safeley start wbc_status
          ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          killall wbc_status > /dev/null 2>&1
          nice /root/wifibroadcast_status/wbc_status "Secondary display disconnected (USB)" 7 55 0
          # re-start osd
          OSDRUNNING=`pidof /tmp/osd | wc -w`
          if [ $OSDRUNNING  -ge 1 ]; then
            echo "OSD already running!"
          else
            killall wbc_status > /dev/null 2>&1
            /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
          fi
          PHONETHERE=0
          # kill forwarding of video and osd to secondary display
          ps -ef | nice grep "socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "gst-launch-1.0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/telemetryfifo5" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cmavnode" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "mavlink-routerd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "tshark" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "rssi_forward" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          # kill msp processes
          ps -ef | nice grep "cat /root/mspfifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          #ps -ef | nice grep "socat /dev/pts/3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "ser2net" | nice grep -v grep | awk '{print $2}' | xargs kill -9
        fi
        sleep 1
      done
    else
      echo "Android device not detected ..."
    fi
    sleep 1
  done
}

function hotspot_check_function {
  # Convert hostap config from DOS format to UNIX format
  ionice -c 3 nice dos2unix -n /boot/apconfig.txt /tmp/apconfig.txt

  if [ "$ETHERNET_HOTSPOT" == "Y" ]; then
    # setup hotspot on RPI3 internal ethernet chip
    nice ifconfig eth0 192.168.1.1 up
    nice udhcpd -I 192.168.1.1 /etc/udhcpd-eth.conf
  fi

  if [ "$WIFI_HOTSPOT" == "Y" ]; then
    nice udhcpd -I 192.168.2.1 /etc/udhcpd-wifi.conf
    nice -n 5 hostapd -B -d /tmp/apconfig.txt
  fi

  while true; do
    # pause loop while saving is in progress
    pause_while
    IP=0
    if [ "$ETHERNET_HOTSPOT" == "Y" ]; then
      if nice ping -I eth0 -c 1 -W 1 -n -q 192.168.1.2 > /dev/null 2>&1; then
        IP="192.168.1.2"
        echo "Ethernet device detected. IP: $IP"
        nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$IP:$TELEMETRY_UDP_PORT &
        nice /root/wifibroadcast/rssi_forward $IP 5003 &
        if [ "$FORWARD_STREAM" == "rtp" ]; then
          ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$IP > /dev/null 2>&1 &
        else
          ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$IP:$VIDEO_UDP_PORT &
        fi
        if cat /boot/osdconfig.txt | grep -q "^#define MAVLINK"; then
          nice cat /root/telemetryfifo5 > /dev/pts/0 &
          if [ "$MAVLINK_FORWARDER" == "mavlink-routerd" ]; then
            ionice -c 3 nice /root/mavlink-router/mavlink-routerd -e $IP:14550 /dev/pts/1:57600 &
          elif ["$MAVLINK_FORWARDER" == "cmavnode"]; then
            cp /boot/cmavnode.conf /tmp/
            echo "targetip=$IP" >> /tmp/cmavnode.conf
            ionice -c 3 nice /root/cmavnode/cmavnode --file /tmp/cmavnode.conf &
          fi
          if [ "$DEBUG" == "Y" ]; then
            tshark -i eth0 -f "udp and port 14550" -w /wbc_tmp/mavlink`date +%s`.pcap &
          fi
        fi
        if [ "$TELEMETRY_UPLINK" == "msp" ]; then
          cat /root/mspfifo > /dev/pts/2 &
          #socat /dev/pts/3 TCP-LISTEN:23
          ser2net
        fi
      fi
    fi
    if [ "$WIFI_HOTSPOT" == "Y" ]; then
      if [[ $(hostapd_cli -i wifihotspot0 all_sta | wc -c) -ne 0 ]]; then
        IP="192.168.2.2"
        echo "Wifi device detected. IP: $IP"
        # nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$IP:$TELEMETRY_UDP_PORT &
        nice /root/wifibroadcast/rssi_forward $IP 5003 &
        if [ "$FORWARD_STREAM" == "rtp" ]; then
          ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$IP > /dev/null 2>&1 &
        else
          ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$IP:$VIDEO_UDP_PORT &
        fi
        if cat /boot/osdconfig.txt | grep -q "^#define MAVLINK"; then
          cat /root/telemetryfifo5 > /dev/pts/0 &
          if [ "$MAVLINK_FORWARDER" == "mavlink-routerd" ]; then
            ionice -c 3 nice /root/mavlink-router/mavlink-routerd -e $IP:14550 /dev/pts/1:57600 &
          elif ["$MAVLINK_FORWARDER" == "cmavnode"]; then
            cp /boot/cmavnode.conf /tmp/
            echo "targetip=$IP" >> /tmp/cmavnode.conf
            ionice -c 3 nice /root/cmavnode/cmavnode --file /tmp/cmavnode.conf &
          fi
          if [ "$DEBUG" == "Y" ]; then
            tshark -i wifihotspot0 -f "udp and port 14550" -w /wbc_tmp/mavlink`date +%s`.pcap &
          fi
        fi

        if [ "$TELEMETRY_UPLINK" == "msp" ]; then
          cat /root/mspfifo > /dev/pts/2 &
          #socat /dev/pts/3 TCP-LISTEN:23
          ser2net
        fi
      fi
    fi
    if [ "$IP" != "0" ]; then
      # kill and pause OSD so we can safeley start wbc_status
      ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Secondary display connected (Hotspot)" 7 55 0

      # re-start osd
      OSDRUNNING=`pidof /tmp/osd | wc -w`
      if [ $OSDRUNNING  -ge 1 ]; then
        echo "OSD already running!"
      else
        killall wbc_status > /dev/null 2>&1
        /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
      fi

      # check if connection is still connected
      IPTHERE=1
      while [  $IPTHERE -eq 1 ]; do
        if [[ $(hostapd_cli -i wifihotspot0 all_sta | wc -c) -ne 0 ]]; then
          IPTHERE=1
          echo "IP $IP still connected ..."
          sleep 5
        else
          echo "IP $IP gone"
          # kill and pause OSD so we can safeley start wbc_status
          ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

          killall wbc_status > /dev/null 2>&1
          nice /root/wifibroadcast_status/wbc_status "Secondary display disconnected (Hotspot)" 7 55 0
          # re-start osd
          OSDRUNNING=`pidof /tmp/osd | wc -w`
          if [ $OSDRUNNING  -ge 1 ]; then
            echo "OSD already running!"
          else
            killall wbc_status > /dev/null 2>&1
            OSDRUNNING=`pidof /tmp/osd | wc -w`
            if [ $OSDRUNNING  -ge 1 ]; then
              echo "OSD already running!"
            else
              killall wbc_status > /dev/null 2>&1
              /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
            fi
          fi
          IPTHERE=0
          # kill forwarding of video and telemetry to secondary display
          ps -ef | nice grep "socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "gst-launch-1.0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/telemetryfifo5" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cmavnode" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "mavlink-routerd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "tshark" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "rssi_forward" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          # kill msp processes
          ps -ef | nice grep "cat /root/mspfifo" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          #ps -ef | nice grep "socat /dev/pts/3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "ser2net" | nice grep -v grep | awk '{print $2}' | xargs kill -9

        fi
        sleep 1
      done
    else
      echo "No IP detected ..."
    fi
    sleep 1
  done
}

function dronebridge_ground_function {
  echo
  cd /root/dronebridge
  # wait until video is running to make sure NICS are configured and wifibroadcast_rx_status shmem is available
  echo
  echo -n "Waiting until setup is complete ..."
  VIDEORXRUNNING=0
  while [ $VIDEORXRUNNING -ne 1 ]; do
    VIDEORXRUNNING=`pidof rx | wc -w`
    sleep 1
    echo -n "."
  done

  echo
  echo "Starting DroneBridge ground station modules..."
  nice -n -9 ./start_db_ground.sh &
}

function dronebridge_air_function {
  # wait until tx is running to make sure NICS are configured
  echo -n "Waiting until setup is complete ..."
  VIDEOTXRUNNING=0
  while [ $VIDEOTXRUNNING -ne 1 ]; do
    VIDEOTXRUNNING=`pidof raspivid | wc -w`
    sleep 1
    echo -n "."
  done

  echo
  echo "Starting DroneBridge UAV modules ..."
  cd /root/dronebridge
  nice -n -9 ./start_db_air.sh &
}


#
# Start of script
#

#setcolors /boot/console-color.txt

printf "\033c"

TTY=`tty`

# check if cam is detected to determine if we're going to be RX or TX
# only do this on one tty so that we don't run vcgencmd multiple times (which may make it hang)
if [ "$TTY" == "/dev/tty1" ]; then
  CAM=`/usr/bin/vcgencmd get_camera | nice grep -c detected=1`
  if [ "$CAM" == "0" ]; then # if we are RX ...
  echo  "0" > /tmp/cam
else # else we are TX ...
  touch /tmp/TX
  echo  "1" > /tmp/cam
fi
else
  #echo -n "Waiting until TX/RX has been determined"
  while [ ! -f /tmp/cam ]; do
    sleep 0.5
    #echo -n "."
  done
  CAM=`cat /tmp/cam`
fi

if [ "$CAM" == "0" ]; then # if we are RX ...
# if local TTY, set font according to display resolution
if [ "$TTY" = "/dev/tty1" ] || [ "$TTY" = "/dev/tty2" ] || [ "$TTY" = "/dev/tty3" ] || [ "$TTY" = "/dev/tty4" ] || [ "$TTY" = "/dev/tty5" ] || [ "$TTY" = "/dev/tty6" ] || [ "$TTY" = "/dev/tty7" ] || [ "$TTY" = "/dev/tty8" ] || [ "$TTY" = "/dev/tty9" ] || [ "$TTY" = "/dev/tty10" ] || [ "$TTY" = "/dev/tty11" ] || [ "$TTY" = "/dev/tty12" ]; then
  H_RES=`tvservice -s | cut -f 2 -d "," | cut -f 2 -d " " | cut -f 1 -d "x"`
  if [ "$H_RES" -ge "1680" ]; then
    setfont /usr/share/consolefonts/Lat15-TerminusBold24x12.psf.gz
  else
    if [ "$H_RES" -ge "1280" ]; then
      setfont /usr/share/consolefonts/Lat15-TerminusBold20x10.psf.gz
    else
      if [ "$H_RES" -ge "800" ]; then
        setfont /usr/share/consolefonts/Lat15-TerminusBold14.psf.gz
      fi
    fi
  fi
fi
fi


if [ -e "/tmp/settings.sh" ]; then
  OK=`bash -n /tmp/settings.sh`
  if [ "$?" == "0" ]; then
    source /tmp/settings.sh
  else
    echo "ERROR: wifibroadcast config file contains syntax error(s)!"
    collect_errorlog
    sleep 365d
  fi
else
  echo "ERROR: wifibroadcast config file not found!"
  collect_errorlog
  sleep 365d
fi

# enable jit compiler for BPF filter (may improve bpf filter performance?)
#echo 1 > /proc/sys/net/core/bpf_jit_enable

case $DATARATE in
  1)
  UPLINK_WIFI_BITRATE=11
  TELEMETRY_WIFI_BITRATE=11
  VIDEO_WIFI_BITRATE=5.5
  ;;
  2)
  UPLINK_WIFI_BITRATE=11
  TELEMETRY_WIFI_BITRATE=11
  VIDEO_WIFI_BITRATE=11
  ;;
  3)
  UPLINK_WIFI_BITRATE=11
  TELEMETRY_WIFI_BITRATE=12
  VIDEO_WIFI_BITRATE=12
  ;;
  4)
  UPLINK_WIFI_BITRATE=11
  TELEMETRY_WIFI_BITRATE=19.5
  VIDEO_WIFI_BITRATE=19.5
  ;;
  5)
  UPLINK_WIFI_BITRATE=11
  TELEMETRY_WIFI_BITRATE=24
  VIDEO_WIFI_BITRATE=24
  ;;
  6)
  UPLINK_WIFI_BITRATE=12
  TELEMETRY_WIFI_BITRATE=36
  VIDEO_WIFI_BITRATE=36
  ;;
esac

FC_TELEMETRY_STTY_OPTIONS="-icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon"

# mmormota's stutter-free hello_video.bin: "hello_video.bin.30-mm" (for 30fps) or "hello_video.bin.48-mm" (for 48 and 59.9fps)
# befinitiv's hello_video.bin: "hello_video.bin.240-befi" (for any fps, use this for higher than 59.9fps)

if [ "$FPS" == "59.9" ]; then
  DISPLAY_PROGRAM=/opt/vc/src/hello_pi/hello_video/hello_video.bin.48-mm
else
  if [ "$FPS" -eq 30 ]; then
    DISPLAY_PROGRAM=/opt/vc/src/hello_pi/hello_video/hello_video.bin.30-mm
  fi
  if [ "$FPS" -lt 60 ]; then
    DISPLAY_PROGRAM=/opt/vc/src/hello_pi/hello_video/hello_video.bin.48-mm
  fi
  if [ "$FPS" -gt 60 ]; then
    DISPLAY_PROGRAM=/opt/vc/src/hello_pi/hello_video/hello_video.bin.240-befi
  fi
fi

VIDEO_UDP_BLOCKSIZE=1024
TELEMETRY_UDP_BLOCKSIZE=128

RELAY_VIDEO_BLOCKS=8
RELAY_VIDEO_FECS=4
RELAY_VIDEO_BLOCKLENGTH=1024

EXTERNAL_TELEMETRY_SERIALPORT_GROUND_STTY_OPTIONS="-icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon"
TELEMETRY_OUTPUT_SERIALPORT_GROUND_STTY_OPTIONS="-icrnl -ocrnl -imaxbel -opost -isig -icanon -echo -echoe -ixoff -ixon"

RSSI_UDP_PORT=5003

if cat /boot/osdconfig.txt | grep -q "^#define LTM"; then
  TELEMETRY_UDP_PORT=5001
  TELEMETRY_TYPE=1
fi
if cat /boot/osdconfig.txt | grep -q "^#define FRSKY"; then
  TELEMETRY_UDP_PORT=5002
  TELEMETRY_TYPE=1
fi
if cat /boot/osdconfig.txt | grep -q "^#define SMARTPORT"; then
  TELEMETRY_UDP_PORT=5010
  TELEMETRY_TYPE=1
fi
if cat /boot/osdconfig.txt | grep -q "^#define MAVLINK"; then
  TELEMETRY_UDP_PORT=5004
  TELEMETRY_TYPE=0
fi


if [ "$CTS_PROTECTION" == "Y" ]; then
  VIDEO_FRAMETYPE=1 # use standard data frames, so that CTS is generated for Atheros
  TELEMETRY_CTS=1
else # auto or N
  VIDEO_FRAMETYPE=2 # use RTS frames (no CTS protection)
  TELEMETRY_CTS=1 # use RTS frames, (always use CTS for telemetry (only atheros anyway))
fi

if [ "$TXMODE" != "single" ]; then # always type 1 in dual tx mode since ralink beacon injection broken
VIDEO_FRAMETYPE=1
TELEMETRY_CTS=1
fi

case $TTY in
  /dev/tty1) # video stuff and general stuff like wifi card setup etc.
  printf "\033[12;0H"
  echo
  tmessage "Display: `tvservice -s | cut -f 3-20 -d " "`"
  echo
  if [ "$CAM" == "0" ]; then
    rx_function
  else
    tx_function
  fi
  ;;
  /dev/tty2) # osd stuff
  echo "================== OSD (tty2) ==========================="
  # only run osdrx if no cam found
  if [ "$CAM" == "0" ]; then
    osdrx_function
  else
    # only run osdtx if cam found, osd enabled and telemetry input is the tx
    if [ "$CAM" == "1" ] && [ "$TELEMETRY_TRANSMISSION" == "wbc" ]; then
      osdtx_function
    fi
  fi
  echo "OSD not enabled in configfile"
  sleep 365d
  ;;
  /dev/tty3) # r/c stuff
  echo "================== R/C TX (tty3) ==========================="
  # only run rctx if no cam found and rc is not disabled
  if [ "$CAM" == "0" ] && [ "$RC" != "disabled" ]; then
    echo "R/C enabled ... we are R/C TX (Ground Pi)"
    rctx_function
  fi
  echo "R/C not enabled in configfile or we are R/C RX (Air Pi)"
  sleep 365d
  ;;
  /dev/tty4) # unused
  echo "================== DroneBridge v0.6 Beta (tty4) ==========================="
  if [ "$CAM" == "0" ]; then
    if [ "$RC" != "disabled" ]; then
      rssirx_function
    fi
    sleep 0.5
    dronebridge_ground_function
  else
    dronebridge_air_function
  fi
  sleep 365d
  ;;
  /dev/tty5) # screenshot stuff
  echo "================== SCREENSHOT (tty5) ==========================="
  echo
  # only run screenshot function if cam found and screenshots are enabled
  if [ "$CAM" == "0" ] && [ "$ENABLE_SCREENSHOTS" == "Y" ]; then
    echo "Waiting some time until everything else is running ..."
    sleep 20
    echo "Screenshots enabled - starting screenshot function ..."
    screenshot_function
  fi
  echo "Screenshots not enabled in configfile or we are TX"
  sleep 365d
  ;;
  /dev/tty6)
  echo "================== SAVE FUNCTION (tty6) ==========================="
  echo
  # # only run save function if we are RX
  if [ "$CAM" == "0" ]; then
    echo "Waiting some time until everything else is running ..."
    sleep 30
    echo "Waiting for USB stick to be plugged in ..."
    KILLED=0
    LIMITFREE=3000 # 3 mbyte
    while true; do
      if [ ! -f "/tmp/donotsave" ]; then
        if [ -e "/dev/sda" ]; then
          echo "USB Memory stick detected"
          save_function
        fi
      fi
      # check if tmp disk is full, if yes, kill cat process
      if [ "$KILLED" != "1" ]; then
        FREETMPSPACE=`nice df -P /wbc_tmp/ | nice awk 'NR==2 {print $4}'`
        if [ $FREETMPSPACE -lt $LIMITFREE ]; then
          echo "RAM disk full, killing cat video file writing  process ..."
          ps -ef | nice grep "cat /root/videofifo3" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          KILLED=1
        fi
      fi
      sleep 1
    done
  fi
  echo "Save function not enabled, we are TX"
  sleep 365d
  ;;
  /dev/tty7) # check tether
  echo "================== CHECK TETHER (tty7) ==========================="
  if [ "$CAM" == "0" ]; then
    echo "Waiting some time until everything else is running ..."
    sleep 6
    tether_check_function
  else
    echo "Cam found, we are TX, Check tether function disabled"
    sleep 365d
  fi
  ;;
  /dev/tty8) # check hotspot
  echo "================== CHECK HOTSPOT (tty8) ==========================="
  if [ "$CAM" == "0" ]; then
    if [ "$ETHERNET_HOTSPOT" == "Y" ] || [ "$WIFI_HOTSPOT" == "Y" ]; then
      echo
      echo -n "Waiting until video is running ..."
      HVIDEORXRUNNING=0
      while [ $HVIDEORXRUNNING -ne 1 ]; do
        sleep 0.5
        HVIDEORXRUNNING=`pidof $DISPLAY_PROGRAM | wc -w`
        echo -n "."
      done
      echo
      echo "Video running, starting hotspot processes ..."
      sleep 1
      hotspot_check_function
    else
      echo "Check hotspot function not enabled in config file"
      sleep 365d
    fi
  else
    echo "Check hotspot function not enabled - we are TX (Air Pi)"
    sleep 365d
  fi
  ;;
  /dev/tty9) # check alive
  echo "================== CHECK ALIVE (tty9) ==========================="
  #	sleep 365d

  if [ "$CAM" == "0" ]; then
    echo "Waiting some time until everything else is running ..."
    sleep 15
    check_alive_function
    echo
  else
    echo "Cam found, we are TX, check alive function disabled"
    sleep 365d
  fi
  ;;
  /dev/tty10)
  echo "================== WBC programms removed (tty10) ==========================="
  sleep 7
  sleep 365d
;;
/dev/tty11) # tty for dhcp and login
echo "================== eth0 DHCP client (tty11) ==========================="
# sleep until everything else is loaded (atheros cards and usb flakyness ...)
sleep 6
if [ "$CAM" == "0" ]; then
  EZHOSTNAME="dronebridge-g"
else
  EZHOSTNAME="dronebridge-a"
fi
# only configure ethernet network interface via DHCP if ethernet hotspot is disabled
if [ "$ETHERNET_HOTSPOT" == "N" ]; then
  # disabled loop, as usual, everything is flaky on the Pi, gives kernel stall messages ...
  nice ifconfig eth0 up
  sleep 2
  if cat /sys/class/net/eth0/carrier | nice grep -q 1; then
    echo "Ethernet connection detected"
    CARRIER=1
    if nice pump -i eth0 --no-ntp -h $EZHOSTNAME; then
      ETHCLIENTIP=`ifconfig eth0 | grep "inet addr" | cut -d ':' -f 2 | cut -d ' ' -f 1`
      # kill and pause OSD so we can safeley start wbc_status
      ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
      killall wbc_status > /dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Ethernet connected. IP: $ETHCLIENTIP" 7 55 0
      pause_while # make sure we don't restart osd while in pause state
      OSDRUNNING=`pidof /tmp/osd | wc -w`
      if [ $OSDRUNNING  -ge 1 ]; then
        echo "OSD already running!"
      else
        killall wbc_status > /dev/null 2>&1
        if [ "$CAM" == "0" ]; then # only (re-)start OSD if we are RX
        /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
      fi
    fi
  else
    ps -ef | nice grep "pump -i eth0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    nice ifconfig eth0 down
    echo "DHCP failed"
    ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
    killall wbc_status > /dev/null 2>&1
    nice /root/wifibroadcast_status/wbc_status "ERROR: Could not acquire IP via DHCP!" 7 55 0
    pause_while # make sure we don't restart osd while in pause state
    OSDRUNNING=`pidof /tmp/osd | wc -w`
    if [ $OSDRUNNING  -ge 1 ]; then
      echo "OSD already running!"
    else
      killall wbc_status > /dev/null 2>&1
      if [ "$CAM" == "0" ]; then # only (re-)start OSD if we are RX
      /tmp/osd >> /wbc_tmp/telemetrydowntmp.txt &
    fi
  fi
fi
else
  echo "No ethernet connection detected"
fi
else
  echo "Ethernet Hotspot enabled, doing nothing"
fi
sleep 365d
;;
/dev/tty12) # tty for local interactive login
echo
if [ "$CAM" == "0" ]; then
  echo -n "Welcome to DroneBridge v0.6 Beta (Ground) - "
  read -p "Press <enter> to login"
  killall osd
  rw
else
  echo -n "Welcome to DroneBridge v0.6 Beta (UAV) - "
  read -p "Press <enter> to login"
  rw
fi
;;
*) # all other ttys used for interactive login
if [ "$CAM" == "0" ]; then
  echo "Welcome to DroneBridge v0.6 Beta (Ground) - type 'ro' to switch filesystems back to read-only"
  rw
else
  echo "Welcome to DroneBridge v0.6 Beta (UAV) - type 'ro' to switch filesystems back to read-only"
  rw
fi
;;
esac
