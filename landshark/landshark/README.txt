The files in this directory are built externally.  Here is what they do and where they come from:

bd_wrapper.bit:
The v4 bitfile.  Built from

scm/projects/helios-z8.git : 78fd3d1334f00fb661414c886fe5a614f400499b
in the landshark_v4 folder

BOOT.BIN:
FSBL specifically for the landshark v4, and containing the setup for the
media pipeline.  The PS/PL clocks and AXI interfaces are different from
the usual iVeia configuration.  Built from

scm/target/fsbl/helios-z8.git : 150c77d443f90813e1ac50c8a801bca8daa20272
scm/target/arm-trusted-firmware.git : 0651b10967ea7cb946788918be8590edfbcb058a
scm/target/uboot/atlas-ii-z8.git : 96db26438ebecd40387740c5a58f3b04d0d3949d

uEnv.txt:
u-boot environment to boot the landshark Android setup.

boot.scr:
The u-boot script to load and boot.  It is created from the boot.sh script
using the command
mkimage -T script -C none -A arm -n 'Landshark Boot Script' -d boot.sh boot.scr

empty.txt:
This gets copied to boot/iveia-helios-z8.dtb.  u-boot checks for the existence
of this file to determine whether to boot from SD or eMMC.  Rather than have
a special u-boot build, we just create this empty file to trigger
the correct action.

