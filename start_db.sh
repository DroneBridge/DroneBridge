#!/usr/bin/env bash

function start_osd() {
  echo
  dos2unix -n /boot/osdconfig.txt /boot/osdconfig.txt
  echo
  cd /root/DroneBridge/osd
  echo Building OSD:
  make -j2 || {
    echo
    echo "ERROR: Could not build OSD, check osdconfig.txt!"
  }
}


if vcgencmd get_throttled | nice grep -q -v "0x0"; then
  TEMP=$(cat /sys/class/thermal/thermal_zone0/temp)
  TEMP_C=$(($TEMP / 1000))
  if [[ "$TEMP_C" -lt 75 ]]; then # it must be under-voltage
    mount -o remount,ro /boot
    echo "1" >/tmp/undervolt
  else
    echo "0" >/tmp/undervolt
  fi
else
  echo "0" >/tmp/undervolt
fi

CAM=$(/usr/bin/vcgencmd get_camera | nice grep -c detected=1)

if [[ "$CAM" == "0" ]]; then
  echo -n "Welcome to DroneBridge v0.6 Beta (GND) - "
  echo
  python3.7 /root/DroneBridge/startup/init_wifi.py -g
  start_osd
  python3.7 /root/DroneBridge/startup/start_db_modules.py -g
else
  echo -n "Welcome to DroneBridge v0.6 Beta (UAV) - "
  echo
  python3.7 /root/DroneBridge/startup/init_wifi.py
  python3.7 /root/DroneBridge/startup/start_db_modules.py
fi


#if [ "$CAM" == "0" ]; then # if we are RX ...
#  # if local TTY, set font according to display resolution
#  if [ "$TTY" = "/dev/tty1" ] || [ "$TTY" = "/dev/tty2" ] || [ "$TTY" = "/dev/tty3" ] || [ "$TTY" = "/dev/tty4" ] || [ "$TTY" = "/dev/tty5" ] || [ "$TTY" = "/dev/tty6" ] || [ "$TTY" = "/dev/tty7" ] || [ "$TTY" = "/dev/tty8" ] || [ "$TTY" = "/dev/tty9" ] || [ "$TTY" = "/dev/tty10" ] || [ "$TTY" = "/dev/tty11" ] || [ "$TTY" = "/dev/tty12" ]; then
#    H_RES=$(tvservice -s | cut -f 2 -d "," | cut -f 2 -d " " | cut -f 1 -d "x")
#    if [[ "$H_RES" -ge "1680" ]]; then
#      setfont /usr/share/consolefonts/Lat15-TerminusBold24x12.psf.gz
#    else
#      if [[ "$H_RES" -ge "1280" ]]; then
#        setfont /usr/share/consolefonts/Lat15-TerminusBold20x10.psf.gz
#      else
#        if [[ "$H_RES" -ge "800" ]]; then
#          setfont /usr/share/consolefonts/Lat15-TerminusBold14.psf.gz
#        fi
#      fi
#    fi
#  fi
#fi