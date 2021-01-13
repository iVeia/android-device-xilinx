#!/system/bin/sh

echo "Running installer "

cp $1 /storage/sdcard0/Download/rand.bin

echo "Copied md5sum: "
md5sum $1

echo "Original md5sum"
md5sum /storage/sdcard0/Download/rand.bin

echo "Finished running the installer script program thing.  Deleting the copied file"

rm /storage/sdcard0/Download/rand.bin

echo "Exiting with status number"

exit 17
