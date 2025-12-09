#ifndef PROXIMITY_1_H
#define PROXIMITY_1_H

#include "protocol_definitions.h"
#include "frame_sublayer.h"
#include "dataser_sublayer.h"
#include "io_sublayer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

SDUFrame* IO_sublayer_tx(uint8_t *OBC_data, uint8_t PortID, uint8_t PDU_ID,
    uint16_t SC_ID, uint8_t SD_ID, bool *is_sending, bool *is_processing, IOBuffer *buffer, size_t OBC_data_size);
//uint8_t* Frame_sublayer_tx(SDUFrame *frame, SDUFrame *multiplexed_data, int *count);
#endif // PROXIMITY_1_H