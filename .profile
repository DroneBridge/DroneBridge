#!/bin/bash
# /root/.profile - main EZ-Wifibroadcast script
# (c) 2017 by Rodizio. Licensed under GPL2
# Modified to be used with DroneBridge by seeul8er
#

function tmessage() {
  if [ "$QUIET" == "N" ]; then
    echo $1 "$2"
  fi
}

function pause_while() {
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

## runs on RX (ground pi)
function osdrx_function() {
  echo
  # Convert osdconfig from DOS format to UNIX format
  ionice -c 3 nice dos2unix -n /boot/osdconfig.txt /tmp/osdconfig.txt
  echo
  cd /root/dronebridge/osd
  echo Building OSD:
  ionice -c 3 nice make -j2 || {
    echo
    echo "ERROR: Could not build OSD, check osdconfig.txt!"
    sleep 5
    nice /root/wifibroadcast_status/wbc_status "ERROR: Could not build OSD, check osdconfig.txt for errors." 7 55 0
    sleep 5
  }
  echo
  /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
}

function tether_check_function() {
  while true; do
    # pause loop while saving is in progress
    pause_while
    if [ -d "/sys/class/net/usb0" ]; then
      echo
      echo "USB tethering device detected. Configuring IP ..."
      nice pump -h wifibrdcast -i usb0 --no-dns --keep-up --no-resolvconf --no-ntp || {
        echo "ERROR: Could not configure IP for USB tethering device!"
        nice killall wbc_status >/dev/null 2>&1
        nice /root/wifibroadcast_status/wbc_status "ERROR: Could not configure IP for USB tethering device!" 7 55 0
        collect_errorlog
        sleep 365d
      }
      # find out smartphone IP to send video stream to
      PHONE_IP=$(ip route show 0.0.0.0/0 dev usb0 | cut -d\  -f3)
      echo "Android IP: $PHONE_IP"

      #nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$PHONE_IP:$TELEMETRY_UDP_PORT &
      #nice /root/wifibroadcast/rssi_forward $PHONE_IP 5003 &

      if [ "$FORWARD_STREAM" == "rtp" ]; then
        ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$PHONE_IP >/dev/null 2>&1 &
      else
        ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$PHONE_IP:$VIDEO_UDP_PORT &
      fi

      # kill and pause OSD so we can safeley start wbc_status
      ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

      killall wbc_status >/dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Secondary display connected (USB)" 7 55 0

      # re-start osd
      killall wbc_status >/dev/null 2>&1
      OSDRUNNING=$(pidof /tmp/osd | wc -w)
      if [ $OSDRUNNING -ge 1 ]; then
        echo "OSD already running!"
      else
        killall wbc_status >/dev/null 2>&1
        /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
      fi

      # check if smartphone has been disconnected
      PHONETHERE=1
      while [ $PHONETHERE -eq 1 ]; do
        if [ -d "/sys/class/net/usb0" ]; then
          PHONETHERE=1
          echo "Android device still connected ..."
        else
          echo "Android device gone"
          # kill and pause OSD so we can safeley start wbc_status
          ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          killall wbc_status >/dev/null 2>&1
          nice /root/wifibroadcast_status/wbc_status "Secondary display disconnected (USB)" 7 55 0
          # re-start osd
          OSDRUNNING=$(pidof /tmp/osd | wc -w)
          if [ $OSDRUNNING -ge 1 ]; then
            echo "OSD already running!"
          else
            killall wbc_status >/dev/null 2>&1
            /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
          fi
          PHONETHERE=0
          # kill forwarding of video and osd to secondary display
          ps -ef | nice grep "socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "gst-launch-1.0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
        fi
        sleep 1
      done
    else
      echo "Android device not detected ..."
    fi
    sleep 1
  done
}

function hotspot_check_function() {
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
      if nice ping -I eth0 -c 1 -W 1 -n -q 192.168.1.2 >/dev/null 2>&1; then
        IP="192.168.1.2"
        echo "Ethernet device detected. IP: $IP"
        #nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$IP:$TELEMETRY_UDP_PORT &
        #nice /root/wifibroadcast/rssi_forward $IP 5003 &
        if [ "$FORWARD_STREAM" == "rtp" ]; then
          ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$IP >/dev/null 2>&1 &
        else
          ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$IP:$VIDEO_UDP_PORT &
        fi
      fi
    fi
    if [ "$WIFI_HOTSPOT" == "Y" ]; then
      if [[ $(hostapd_cli -i wifihotspot0 all_sta | wc -c) -ne 0 ]]; then
        IP="192.168.2.2"
        echo "Wifi device detected. IP: $IP"
        # nice socat -b $TELEMETRY_UDP_BLOCKSIZE GOPEN:/root/telemetryfifo2 UDP4-SENDTO:$IP:$TELEMETRY_UDP_PORT &
        #nice /root/wifibroadcast/rssi_forward $IP 5003 &
        if [ "$FORWARD_STREAM" == "rtp" ]; then
          ionice -c 1 -n 4 nice -n -5 cat /root/videofifo2 | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=5 ! udpsink port=$VIDEO_UDP_PORT host=$IP >/dev/null 2>&1 &
        else
          ionice -c 1 -n 4 nice -n -10 socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2 UDP4-SENDTO:$IP:$VIDEO_UDP_PORT &
        fi
      fi
    fi
    if [ "$IP" != "0" ]; then
      # kill and pause OSD so we can safeley start wbc_status
      ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

      killall wbc_status >/dev/null 2>&1
      nice /root/wifibroadcast_status/wbc_status "Secondary display connected (Hotspot)" 7 55 0

      # re-start osd
      OSDRUNNING=$(pidof /tmp/osd | wc -w)
      if [ $OSDRUNNING -ge 1 ]; then
        echo "OSD already running!"
      else
        killall wbc_status >/dev/null 2>&1
        /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
      fi

      # check if connection is still connected
      IPTHERE=1
      while [ $IPTHERE -eq 1 ]; do
        if [[ $(hostapd_cli -i wifihotspot0 all_sta | wc -c) -ne 0 ]]; then
          IPTHERE=1
          echo "IP $IP still connected ..."
          sleep 5
        else
          echo "IP $IP gone"
          # kill and pause OSD so we can safeley start wbc_status
          ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9

          killall wbc_status >/dev/null 2>&1
          nice /root/wifibroadcast_status/wbc_status "Secondary display disconnected (Hotspot)" 7 55 0
          # re-start osd
          OSDRUNNING=$(pidof /tmp/osd | wc -w)
          if [ $OSDRUNNING -ge 1 ]; then
            echo "OSD already running!"
          else
            killall wbc_status >/dev/null 2>&1
            OSDRUNNING=$(pidof /tmp/osd | wc -w)
            if [ $OSDRUNNING -ge 1 ]; then
              echo "OSD already running!"
            else
              killall wbc_status >/dev/null 2>&1
              /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
            fi
          fi
          IPTHERE=0
          # kill forwarding of video and telemetry to secondary display
          ps -ef | nice grep "socat -b $VIDEO_UDP_BLOCKSIZE GOPEN:/root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "gst-launch-1.0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          ps -ef | nice grep "cat /root/videofifo2" | nice grep -v grep | awk '{print $2}' | xargs kill -9
          # kill msp processes

        fi
        sleep 1
      done
    else
      echo "No IP detected ..."
    fi
    sleep 1
  done
}

#
# Start of script
#

printf "\033c"

TTY=$(tty)

# check if cam is detected to determine if we're going to be RX or TX
# only do this on one tty so that we don't run vcgencmd multiple times (which may make it hang)
if [ "$TTY" == "/dev/tty1" ]; then
  CAM=$(/usr/bin/vcgencmd get_camera | nice grep -c detected=1)
  if [ "$CAM" == "0" ]; then # if we are RX ...
    echo "0" >/tmp/cam
  # else we are TX ...
  else
    touch /tmp/TX
    echo "1" >/tmp/cam
  fi
else
  #echo -n "Waiting until TX/RX has been determined"
  while [ ! -f /tmp/cam ]; do
    sleep 0.5
    #echo -n "."
  done
  CAM=$(cat /tmp/cam)
fi

if [ "$CAM" == "0" ]; then # if we are RX ...
  # if local TTY, set font according to display resolution
  if [ "$TTY" = "/dev/tty1" ] || [ "$TTY" = "/dev/tty2" ] || [ "$TTY" = "/dev/tty3" ] || [ "$TTY" = "/dev/tty4" ] || [ "$TTY" = "/dev/tty5" ] || [ "$TTY" = "/dev/tty6" ] || [ "$TTY" = "/dev/tty7" ] || [ "$TTY" = "/dev/tty8" ] || [ "$TTY" = "/dev/tty9" ] || [ "$TTY" = "/dev/tty10" ] || [ "$TTY" = "/dev/tty11" ] || [ "$TTY" = "/dev/tty12" ]; then
    H_RES=$(tvservice -s | cut -f 2 -d "," | cut -f 2 -d " " | cut -f 1 -d "x")
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
  if [ "$?" == "0" ]; then
    source /tmp/settings.sh
  else
    echo "ERROR: wifibroadcast config file contains syntax error(s)!"
    sleep 365d
  fi
else
  echo "ERROR: wifibroadcast config file not found!"
  sleep 365d
fi

  # set reg domain to DE to allow channel 12 and 13 for hotspot
  iw reg set DE

  NUM_CARDS=-1
  NICSWL=$(ls /sys/class/net | nice grep wlan)

for NIC in $NICSWL; do
# set MTU to 2304
ifconfig $NIC mtu 2304
# re-name wifi interface to MAC address
NAME=$(cat /sys/class/net/$NIC/address)
ip link set $NIC name ${NAME//:/}
let "NUM_CARDS++"
#sleep 0.1
done

if vcgencmd get_throttled | nice grep -q -v "0x0"; then
  TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
  TEMP_C=$(($TEMP / 1000))
  if [ "$TEMP_C" -lt 75 ]; then # it must be under-voltage
    mount -o remount,ro /boot
    UNDERVOLT=1
    echo "1" >/tmp/undervolt
    BITRATE=$((1000 * 1000))
    BITRATE_KBIT=1000
    BITRATE_MEASURED_KBIT=2000
  # it was either over-temp or both undervolt and over-temp, we set undervolt to 0 anyway, since overtemp can be seen at the temp display on the rx
  else
    UNDERVOLT=0
    echo "0" >/tmp/undervolt
  fi
else
  UNDERVOLT=0
  echo "0" >/tmp/undervolt
fi

case $TTY in
/dev/tty1) # video stuff and general stuff like wifi card setup etc.
  printf "\033[12;0H"
  echo
  tmessage "Display: $(tvservice -s | cut -f 3-20 -d " ")"
  ifconfig eth0 up
  if [ "$CAM" == "0" ]; then
    /root/dronebridge/video/legacy/sharedmem_init_rx
  	python3 /root/dronebridge/startup/init_wifi.py -g
    python3 /root/dronebridge/startup/start_db_modules.py -g
    NICS=$(ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v wlan | nice grep -v relay | nice grep -v wifihotspot)
    ionice -c 1 -n 3 /root/dronebridge/video/legacy/rx -p 0 -d 1 -b $VIDEO_BLOCKS -r $VIDEO_FECS -f $VIDEO_BLOCKLENGTH $NICS | ionice -c 1 -n 4 nice -n -10 tee >(ionice -c 1 -n 4 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo2 >/dev/null 2>&1) >(ionice -c 1 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo4 >/dev/null 2>&1) >(ionice -c 3 nice /root/wifibroadcast_misc/ftee /root/videofifo3 >/dev/null 2>&1) | ionice -c 1 -n 4 nice -n -10 /root/wifibroadcast_misc/ftee /root/videofifo1 >/dev/null 2>&1
    ionice -c 1 -n 4 nice -n -10 cat /root/videofifo1 | ionice -c 1 -n 4 nice -n -10 /opt/vc/src/hello_pi/hello_video/hello_video.bin.240-befi >/dev/null 2>&1
  else
    /root/dronebridge/video/legacy/sharedmem_init_tx
  	python3 /root/dronebridge/startup/init_wifi.py
    python3 /root/dronebridge/startup/start_db_modules.py
  fi
  ;;
/dev/tty2) # osd stuff
  echo "================== OSD (tty2) ==========================="
  # only run osdrx if no cam found
  if [ "$CAM" == "0" ]; then
    osdrx_function
  fi
  echo "OSD not enabled in configfile"
  sleep 365d
  ;;
/dev/tty3)
  echo "==================(tty3) ==========================="
  sleep 365d
  ;;
/dev/tty4)
  echo "================== DroneBridge v0.6 Beta (tty4) ==========================="
  # if [ "$CAM" == "0" ]; then
  #   dronebridge_ground_function
  # else
  #   dronebridge_air_function
  # fi
  sleep 365d
  ;;
/dev/tty5)
  echo "================== (tty5) ==========================="
  sleep 365d
  ;;
/dev/tty6)
  echo "================== (tty6) ==========================="
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
        HVIDEORXRUNNING=$(pidof rx | wc -w)
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
/dev/tty9)
  echo "================== (tty9) ==========================="
  sleep 365d
  ;;
/dev/tty10)
  echo "================== (tty10) ==========================="
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
      if nice pump -i eth0 --no-ntp -h $EZHOSTNAME; then
        ETHCLIENTIP=$(ifconfig eth0 | grep "inet addr" | cut -d ':' -f 2 | cut -d ' ' -f 1)
        # kill and pause OSD so we can safeley start wbc_status
        ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
        killall wbc_status >/dev/null 2>&1
        nice /root/wifibroadcast_status/wbc_status "Ethernet connected. IP: $ETHCLIENTIP" 7 55 0
        pause_while # make sure we don't restart osd while in pause state
        OSDRUNNING=$(pidof /tmp/osd | wc -w)
        if [ $OSDRUNNING -ge 1 ]; then
          echo "OSD already running!"
        else
          killall wbc_status >/dev/null 2>&1
          if [ "$CAM" == "0" ]; then # only (re-)start OSD if we are RX
            /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
          fi
        fi
      else
        ps -ef | nice grep "pump -i eth0" | nice grep -v grep | awk '{print $2}' | xargs kill -9
        nice ifconfig eth0 down
        echo "DHCP failed"
        ps -ef | nice grep "osd" | nice grep -v grep | awk '{print $2}' | xargs kill -9
        killall wbc_status >/dev/null 2>&1
        nice /root/wifibroadcast_status/wbc_status "ERROR: Could not acquire IP via DHCP!" 7 55 0
        pause_while # make sure we don't restart osd while in pause state
        OSDRUNNING=$(pidof /tmp/osd | wc -w)
        if [ $OSDRUNNING -ge 1 ]; then
          echo "OSD already running!"
        else
          killall wbc_status >/dev/null 2>&1
          if [ "$CAM" == "0" ]; then # only (re-)start OSD if we are RX
            /tmp/osd >>/wbc_tmp/telemetrydowntmp.txt &
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
