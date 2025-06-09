// AddressRoomMap.h
#ifndef ADDRESS_ROOM_MAP_H
#define ADDRESS_ROOM_MAP_H

#include <Arduino.h>

/*
 * Don't need a general static (as in not dynamically changing size) map. 
 * So this tightly-coupled-with-app implementation is sufficient.
*/
struct AddressRoomPair {
    String peripheralAddress;
    String room;
};

static const AddressRoomPair roomSimpleMap[] = {
    {"f9:3f:1d:46:f4:0c", "Kitchen"},
    {"f8:ce:3f:2b:5e:55", "Living Room"},
    {"eb:d9:7e:a1:e1:08", "Computer Desk"},
};

static const int roomSimpleMapSize = sizeof(roomSimpleMap) / sizeof(AddressRoomPair);

inline String getRoomNameByAddress(const String& address) {
    for (int i = 0; i < roomSimpleMapSize; i++) {
        if (roomSimpleMap[i].peripheralAddress == address) {
            return roomSimpleMap[i].room;
        }
    }
    return "Unknown Room"; // Default value if not found
}

#endif // ADDRESS_ROOM_MAP_H