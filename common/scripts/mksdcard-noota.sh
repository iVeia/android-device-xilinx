#!/bin/bash

if [ $# -lt 1 ]; then
	echo "Usage: $0 /dev/diskname [product=zcu102]"
	echo " "
	echo "product   - Android build product name. Default is zcu102"
	echo "            Used to find folder with build results"
	echo "silicon   - Silicon revision of the SoC on the board."
	echo "            es1 or es2. Default is es1"
	exit -1 ;
fi

if [ $# -ge 2 ]; then
	product=$2;
else
	product=zcu102;
fi

removable_disks() {
	for f in `ls /dev/disk/by-path/* | grep -v part` ; do
		diskname=$(basename `readlink $f`);
		type=`cat /sys/class/block/$diskname/device/type` ;
		size=`cat /sys/class/block/$diskname/size` ;
		issd=0 ;
		# echo "checking $diskname/$type/$size" ;
		if [ $size -ge 3906250 ]; then
			if [ $size -lt 62500000 ]; then
				issd=1 ;
			fi
		fi
		if [ "$issd" -eq "1" ]; then
			echo -n "/dev/$diskname ";
			# echo "removable disk /dev/$diskname, size $size, type $type" ;
			#echo -n -e "\tremovable? " ; cat /sys/class/block/$diskname/removable ;
		fi
	done
	echo;
}
diskname=$1
removables=`removable_disks`

for disk in $removables ; do
	echo "Found available removable disk: $disk" ;
	if [ "$diskname" = "$disk" ]; then
		matched=1 ;
		break ;
	fi
done

if [ -z "$matched" -a -z "$force" ]; then
	if [ "${diskname:5:6}" == "mmcblk" ]; then
		echo "mmcblk not seen as removable but will try it anyway"
	else
		echo "Invalid disk $diskname" ;
		exit -1;
	fi
fi

prefix='';

if [[ "$diskname" =~ "mmcblk" ]]; then
	prefix=p
fi

echo "reasonable disk $diskname, partitions ${diskname}${prefix}1..." ;

umount ${diskname}${prefix}*

echo "<<iVeia_recovery:update:part,.1>>"
echo "========= creating partition table"
parted -s ${diskname} mklabel msdos

echo "<<iVeia_recovery:update:part,.15>>"
echo "========= creating BOOT partition"
parted -s --align=optimal ${diskname} mkpart primary 4MiB 132MiB

echo "<<iVeia_recovery:update:part,.2>>"
echo "========= creating ROOT partition"
parted -s --align=optimal ${diskname} mkpart primary 132MiB 260MiB

# Making extended partition. Reserve 3GiB.
# It will contain system and cache partitions for now.
# Additional misc. partitions (vendor, misc) should be placed
# on this extended partition after cache.
echo "<<iVeia_recovery:update:part,.25>>"
parted -s --align=optimal ${diskname} mkpart extended 260MiB 3332MiB

echo "<<iVeia_recovery:update:part,.3>>"
echo "========= creating SYSTEM partition"
parted -s --align=optimal ${diskname} mkpart logical 264MiB 2312MiB

echo "<<iVeia_recovery:update:part,.4>>"
echo "========= creating CACHE partition"
parted -s --align=optimal ${diskname} mkpart logical 2316MiB 2828MiB

echo "<<iVeia_recovery:update:part,.5>>"
echo "========= creating DATA partition"
parted -s --align=optimal ${diskname} mkpart primary 3332MiB 100%

sync
sleep 1

for n in `seq 1 6` ; do
	if ! [ -e ${diskname}${prefix}$n ] ; then
		echo "!!! Error: missing partition ${diskname}${prefix}$n" ;
		echo "<<iVeia_recovery:complete:part,false>>"
		exit 1;
	fi
	sync
done

echo "<<iVeia_recovery:update:part,.6>>"
echo "========= formating BOOT partition"
mkfs.vfat -F 32 -n BOOT ${diskname}${prefix}1

echo "<<iVeia_recovery:update:part,.65>>"
echo "========= formating ROOT partition"
mkfs.ext4 -F -L ROOT ${diskname}${prefix}2

echo "<<iVeia_recovery:update:part,.7>>"
echo "========= formating SYSTEM partition"
mkfs.ext4 -F -L SYSTEM ${diskname}${prefix}5

echo "<<iVeia_recovery:update:part,.8>>"
echo "========= formating CACHE partition"
mkfs.ext4 -F -L CACHE ${diskname}${prefix}6

echo "<<iVeia_recovery:update:part,.9>>"
echo "========= formating DATA partition"
mkfs.ext4 -F -L DATA ${diskname}${prefix}4

echo "<<iVeia_recovery:complete:part,true>>"


echo "<<iVeia_recovery:update:pboot,.1>>"
echo "========= populating BOOT partition"
if [ -e ${diskname}${prefix}1 ]; then
	mkdir -p /tmp/boot_part
	mount -t vfat ${diskname}${prefix}1 /tmp/boot_part
        echo "<<iVeia_recovery:update:pboot,.2>>"
	cp -rfv /root/boot/* /tmp/boot_part/
	echo "<<iVeia_recovery:update:pboot,.4>>"
        mv /tmp/boot_part/uEnv-noota.txt /tmp/boot_part/uEnv.txt 
	echo "<<iVeia_recovery:update:pboot,.6>>"
        sync
	echo "<<iVeia_recovery:update:pboot,.8>>"
        umount /tmp/boot_part
	rm -rf /tmp/boot_part
        echo "<<iVeia_recovery:complete:pboot,true>>"
else
   echo "!!! Error: missing BOOT partition ${diskname}${prefix}1";
   echo "<<iVeia_recovery:complete:pboot,false>>"
   exit 1
fi

echo "========= populating ROOT partition"
echo "<<iVeia_recovery:update:proot,.1>>"
if [ -e ${diskname}${prefix}2 ]; then
	mkdir -p /tmp/root_part
	mount -t ext4 ${diskname}${prefix}2 /tmp/root_part
        echo "<<iVeia_recovery:update:proot,.3>>"
        cp -r /root/root/* /tmp/root_part/
        echo "<<iVeia_recovery:update:proot,.6>>"       
	sync
        echo "<<iVeia_recovery:update:proot,.9>>"
	umount /tmp/root_part
	rm -rf /tmp/root_part
	echo "<<iVeia_recovery:complete:proot,true>>"
else
	echo "!!! Error: missing ROOT partition ${diskname}${prefix}2";
	echo "<<iVeia_recovery:complete:proot,false>>"
	exit 1
fi

sys_size=$(stat -c%s /root/system.img)
echo "<<iVeia_recovery:ddcoming:psystem,$sys_size,0.5,0.95>>"
echo "========= populating SYSTEM partition"
if [ -e ${diskname}${prefix}5 ]; then
	dd if=/root/system.img of=${diskname}${prefix}5
	echo "<<iVeia_recovery:complete:psystem,true>>"
else
	echo "!!! Error: missing SYSTEM partition ${diskname}${prefix}5";
	echo "<<iVeia_recovery:complete:psystem,false>>"
	exit 1
fi
