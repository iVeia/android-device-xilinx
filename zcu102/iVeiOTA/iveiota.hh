#ifndef __IVEIOTA_HH
#define __IVEIOTA_HH

// Semantic versioning for the OTA server/client
#define IVEIOTA_MAJOR 1
#define IVEIOTA_MINOR 0
#define IVEIOTA_PATCH 6

//TODO: Much of this should be in a config file, or be passed as a command line
//      parameter.

// Where to get the command line for the kernel
#define IVEIOTA_KERNEL_CMDLINE "/proc/cmdline"

// The place the server will mount partitions for modifications
#define IVEIOTA_MNT_POINT     "/mnt/ota_tmp"

// The UNIX domain socket the server will communicate on
//  the @ gets replaced by \0 for abstract namespace required by Android
#define IVEIOTA_DEFAULT_SOCK_NAME "@/tmp/iVeiOTA.server" 

// Default configuration file for OTA management
#define IVEIOTA_DEFAULT_CONFIG    "/etc/iVeiOTA.conf"

// Name of the environment file for uboot AB configuration storage
#define IVEIOTA_UBOOT_CONF_NAME   "uEnvAB.txt"

// Location of the OTA cache for restoration after a power cycle
#define IVEIOTA_CACHE_LOCATION    "/data/iVeiOTA/cache"

#endif
