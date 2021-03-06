#!/system/bin/sh
INITIAL_FILE="/MinibarRxApp.apk"
FILE="/sdcard/Download/MinibarRxApp.apk"
REBOOT_FILE="/data/data/com.minibarna.vending.machine.rx/app_rx_scripts/noReboot.txt"
polling_interval=60
echo "looking for new apk..."

while true; do

	if [ -f $INITIAL_FILE ]; then
		if [ -e "/sdcard/Download" ]; then
			echo "$0: Initial install..."
			# Move the initial apk, only happens the first time
			mount -o rw,remount /
			mv $INITIAL_FILE $FILE
			mount -o ro,remount /
		else
			echo "$0: still booting up..."
		fi
	fi
	
    # Check if we have noReboot.txt file, if so we don't want to check
    # since we don't want a reboot when the user is logged in
    if [[ -e $REBOOT_FILE ]]; then
        echo "$0: Reboot file exists, user is logged in, do not reboot."
        sleep $polling_interval
        continue
    fi

    if [ -f $FILE ]; then
        echo "$0: New App Update does exist, updating..."
        pm install -r $FILE
        if [ $? -eq 0 ]; then
            echo "$0: Success, starting app now."
            am start -n com.minibarna.vending.machine.rx/com.minibarna.vending.machine.rx.ActivityMain
            rm -f $FILE
        else
            echo "$0: Failed to update app."
        fi
    else
        echo "$0: No app to update."
    fi

    sleep $polling_interval;
done
