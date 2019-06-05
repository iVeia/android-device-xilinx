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

echo "========= creating partition table"
parted -s ${diskname} mklabel msdos

offset=4
echo "========= creating BOOT partition : 2GiB"
parted -s --align=optimal ${diskname} mkpart primary ${offset}MiB $((offset+2048))MiB
let offset=offset+2048

echo "========= creating Extended partition : 13 GiB"
parted -s --align=optimal ${diskname} mkpart extended ${offset}MiB $((offset+1024*13))MiB
let offset=offset+4 # Only increment by 4 because we are partitioning inside the extended

echo "********* creating container A ****************"
echo "========= creating BootInfo partition : 256 MiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+256))MiB
let offset=offset+512

echo "========= creating ROOT partition : 1GiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+1024))MiB
let offset=offset+1025

echo "========= creating SYS partition : 4 GiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+4*1024))MiB
let offset=offset+4*1024+1

echo "========= creating CACHE partition : 768 MiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+768))MiB
let offset=offset+778

echo "********* creating container B ****************"
echo "========= creating BootInfo partition : 256 MiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+256))MiB
let offset=offset+512

echo "========= creating ROOT partition : 1GiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+1024))MiB
let offset=offset+1025

echo "========= creating SYS partition : 4 GiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+4*1024))MiB
let offset=offset+4*1024+1

echo "========= creating CACHE partition : 768 MiB"
parted -s --align=optimal ${diskname} mkpart logical ${offset}MiB $((offset+768))MiB
let offset=offset+768

offset=$((2048+1024*13+128))
echo "========= creating Data partition : 8 GiB"
parted -s --align=optimal ${diskname} mkpart primary ${offset}MiB $((offset+8*1024))MiB
let offset=offset+8*1024

echo "========= creating Scratch partition"
parted -s --align=optimal ${diskname} mkpart primary ${offset}MiB 100%

sync
sleep 1

for n in `seq 1 12` ; do
	if ! [ -e ${diskname}${prefix}$n ] ; then
		echo "!!! Error: missing partition ${diskname}${prefix}$n" ;
		exit 1;
	fi
	sync
done

echo "========= formating BOOT partition"
mkfs.vfat -F 32 -n BOOT ${diskname}${prefix}1

echo "========= formating BootInfoA partition"
mkfs.ext4 -F -L BIA ${diskname}${prefix}5

echo "========= formating ROOTA partition"
mkfs.ext4 -F -L ROOTA ${diskname}${prefix}6

echo "========= formating SYSTEMA partition"
mkfs.ext4 -F -L SYSTEMA ${diskname}${prefix}7

echo "========= formating CACHEA partition"
mkfs.ext4 -F -L CACHEA ${diskname}${prefix}8

echo "========= formating BootInfoB partition"
mkfs.ext4 -F -L BIB ${diskname}${prefix}9

echo "========= formating ROOTB partition"
mkfs.ext4 -F -L ROOTB ${diskname}${prefix}10

echo "========= formating SYSTEMB partition"
mkfs.ext4 -F -L SYSTEMB ${diskname}${prefix}11

echo "========= formating CACHEB partition"
mkfs.ext4 -F -L CACHEB ${diskname}${prefix}12

echo "========= formating DATA partition"
mkfs.ext4 -F -L DATA ${diskname}${prefix}3

echo "========= formating scratch partition"
mkfs.ext4 -F -L Scratch ${diskname}${prefix}4


echo "========= populating BOOT partition"
if [ -e ${diskname}${prefix}1 ]; then
	mkdir -p /tmp/boot_part
	mount -t vfat ${diskname}${prefix}1 /tmp/boot_part
	cp -rfv /root/boot/* /tmp/boot_part/
	sync
	umount /tmp/boot_part
	rm -rf /tmp/boot_part
else
   echo "!!! Error: missing BOOT partition ${diskname}${prefix}1";
   exit 1
fi

echo "========= populating BootInfoA partition"
if [ -e ${diskname}${prefix}5 ]; then
	mkdir -p /tmp/bi_part
	mount -t ext4 ${diskname}${prefix}5 /tmp/bi_part
	cp -rfv /root/bi/* /tmp/bi_part/
	sync
	umount /tmp/bi_part
	rm -rf /tmp/bi_part
else
   echo "!!! Error: missing BOOT partition ${diskname}${prefix}1";
   exit 1
fi

echo "========= populating BootInfoB partition"
if [ -e ${diskname}${prefix}9 ]; then
	mkdir -p /tmp/bi_part
	mount -t ext4 ${diskname}${prefix}9 /tmp/bi_part
	cp -rfv /root/bi/* /tmp/bi_part/
	sync
	umount /tmp/bi_part
	rm -rf /tmp/bi_part
else
   echo "!!! Error: missing BOOT partition ${diskname}${prefix}1";
   exit 1
fi


echo "========= populating ROOTA partition"
if [ -e ${diskname}${prefix}6 ]; then
    mkdir -p /tmp/root_part
	mount -t ext4 ${diskname}${prefix}6 /tmp/root_part
	cp -r /root/root/* /tmp/root_part/
        cp /tmp/root_part/fstab.zcu102.a /tmp/root_part/fstab.zcu102
	sync
	umount /tmp/root_part
	rm -rf /tmp/root_part
else
	echo "!!! Error: missing ROOT partition ${diskname}${prefix}2";
	exit 1
fi

echo "========= populating ROOTB partition"
if [ -e ${diskname}${prefix}10 ]; then
    mkdir -p /tmp/root_part
	mount -t ext4 ${diskname}${prefix}10 /tmp/root_part
	cp -r /root/root/* /tmp/root_part/
        cp /tmp/root_part/fstab.zcu102.b /tmp/root_part/fstab.zcu102
	sync
	umount /tmp/root_part
	rm -rf /tmp/root_part
else
	echo "!!! Error: missing ROOT partition ${diskname}${prefix}2";
	exit 1
fi

echo "========= populating SYSTEMA partition"
if [ -e ${diskname}${prefix}7 ]; then
	dd if=/root/system.img of=${diskname}${prefix}7
	e2label ${diskname}${prefix}7 SYSTEMA
	e2fsck -f ${diskname}${prefix}7
    mkdir -p /tmp/sys_part
    mount -t ext4 ${diskname}${prefix}7 /tmp/sys_part
    cp -r /root/modules /tmp/sys_part/lib/
    sync
    umount /tmp/sys_part
    rm -rf /tmp/sys_part
else
	echo "!!! Error: missing SYSTEM partition ${diskname}${prefix}5";
	exit 1
fi

echo "========= populating SYSTEMB partition"
if [ -e ${diskname}${prefix}11 ]; then
	dd if=/root/system.img of=${diskname}${prefix}11
	e2label ${diskname}${prefix}11 SYSTEMA
	e2fsck -f ${diskname}${prefix}11
    mkdir -p /tmp/sys_part
    mount -t ext4 ${diskname}${prefix}11 /tmp/sys_part
    cp -r /root/modules /tmp/sys_part/lib/
    sync
    umount /tmp/sys_part
    rm -rf /tmp/sys_part
else
	echo "!!! Error: missing SYSTEM partition ${diskname}${prefix}5";
	exit 1
fi
