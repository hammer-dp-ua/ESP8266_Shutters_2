#ifndef DEVICE_SETTINGS
#define DEVICE_SETTINGS

#define ROOM_SHUTTER
//#define KITCHEN_SHUTTER
//#define TESTING

// const are saved at 40200000h (flash memory)
#ifdef TESTING
static const char OWN_IP_ADDRESS[] = "192.168.0.90";
static const char DEVICE_NAME[] = "Testing";
#elif defined(KITCHEN_SHUTTER)
static const char OWN_IP_ADDRESS[] = "192.168.0.29"; // 4MB Flash
static const char DEVICE_NAME[] = "Kitchen shutter";
#elif defined(ROOM_SHUTTER)
static const char OWN_IP_ADDRESS[] = "192.168.0.27"; // 4MB Flash
static const char DEVICE_NAME[] = "Room shutter";
#endif

static const char OWN_NETMASK[] = "255.255.255.0";
static const char OWN_GETAWAY_ADDRESS[] = "192.168.0.1";
static const char SERVER_IP_ADDRESS[] = "192.168.0.2";
static const char ACCESS_POINT_PASSWORD[] = "x>vZf8bqX]l-qg%";

static const char ACCESS_POINT_NAME[] = "SAMSUNG";

#define SERVER_PORT 80
#define LOCAL_SERVER_PORT 80

#endif
