# Android fstab file.
#<src>                                                  <mnt_point>         <type>    <mnt_flags and options>                       <fs_mgr_flags>
# The filesystem that contains the filesystem checker binary (typically /system) cannot
# specify MF_CHECK, and must come before any filesystems that do specify MF_CHECK

/dev/block/mmcblk1p12    /cache              ext4      discard,noauto_da_alloc,data=ordered,user_xattr,discard,barrier=1    wait
/dev/block/mmcblk1p11    /system             ext4      ro                                                    wait
/dev/block/mmcblk1p3   /data               ext4      discard,noauto_da_alloc,data=ordered,user_xattr,discard,barrier=1    wait
