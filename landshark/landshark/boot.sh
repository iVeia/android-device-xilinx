echo "Running the Landshark Android setup"
setenv uramdisk_addr 0x10000000
setenv fdt_file iveia-landshark.dtb
setenv kernel_file Image
setenv uramdisk uramdisk.img
# root=/dev/mmcblk0p2 rw rootwait rootfstype=ext4
setenv bootargs console=ttyPS0,115200n8 earlycon mem=2032M init=/init androidboot.selinux=permissive androidboot.hardware=landshark firmware_class.path=/system/etc/firmware clk_ignore_unused

echo "Setting up I2C dev 1"
mw.l 0xff180080 0x00000040
mw.l 0xff180084 0x00000040

echo "Setting up misc hardware"
gpio clear 28
i2c dev 0
i2c mw 0x2C 0x00 0xFF

echo "Set up"

echo "Loading FPGA..."
fatload mmc 0 ${uramdisk_addr} bd_wrapper.bit;
fpga loadb 0 ${uramdisk_addr} ${filesize}
echo "Loaded FPGA"

echo "Loading device tree"
fatload mmc 0 ${fdt_addr} ${fdt_file}

echo "Loading kernel"
fatload mmc 0 ${kernel_addr} ${kernel_file}

echo "Loading ramdisk"
fatload mmc 0 ${uramdisk_addr} ${uramdisk}

setenv obargs root=/dev/mmcblk0p2 rw rootwait rootfstype=ext4
setenv booturam booti ${kernel_addr} ${uramdisk_addr} ${fdt_addr}
setenv bootdisk booti ${kernel_addr} - ${fdt_addr}

#echo "run booturam to boot ramdisk or bootdisk for flash"
booti ${kernel_addr} ${uramdisk_addr} ${fdt_addr}

