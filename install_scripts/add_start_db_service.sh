cp /home/pi/DroneBridge/start_db /etc/init.d/start_db
update-rc.d -f start_db remove
update-rc.d start_db defaults