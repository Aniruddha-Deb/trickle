#pragma once

#include <cinttypes>

const uint8_t REQ = 1;
const uint8_t OPEN = 2;
const uint8_t REG = 3;

struct FileChunk {
    uint32_t id;
    uint16_t size;
    char data[1024];
};

struct ControlMessage {
    uint8_t msgtype;
    uint32_t client_id;
    uint32_t chunk_id;
};