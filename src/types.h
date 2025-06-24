#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

typedef struct {
    uint8_t frame_ctrl[2];
    uint8_t duration[2];
    uint8_t access_point[6];
    uint8_t sender[6];
    uint8_t station[6];
    uint8_t bssid[6];
    uint8_t seq_ctrl[2];
    uint8_t reason[2];
} deauth_frame_t;

#endif // TYPES_H