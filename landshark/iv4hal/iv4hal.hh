#ifndef __IV4HAL_HH
#define __IV4HAL_HH

// Semantic versioning for the OTA server/client
#define IV4HAL_MAJOR 0
#define IV4HAL_MINOR 6
#define IV4HAL_PATCH 1

// The UNIX domain socket the server will communicate on
//  the @ gets replaced by \0 for abstract namespace required by Android
#define IV4HAL_DEFAULT_SOCK_NAME "@/tmp/iv4hal.server" 


#define IV4HAL_EVENT_SOCK_NAME   "@/tmp/iv4halev.server" 

// Default configuration file for OTA management
#define IV4HAL_DEFAULT_CONFIG    "/etc/iv4hal.conf"

// The largest amount of data we can stuff into a packet payload
#define IV4HAL_MAX_PACKET_LEN (1024 * 1024 * 32)

#endif
