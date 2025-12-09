#ifndef FRAME_SUBLAYER_H
#define FRAME_SUBLAYER_H

#include "protocol_definitions.h"
#include "io_sublayer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

SerializedData serialize_sdu_frame(const SDUFrame* frame);
size_t serialize_into(const SDUFrame* frame, uint8_t* buffer, size_t buffer_size);
void choose_priority(SDUFrame** MultiplexedData, int* count, SDUFrame newData);
bool check_data(SDUFrame** MultiplexedData, int* count);
SerializedData send_to_LoRa(SDUFrame** MultiplexedData, int* count);
SDUFrame deserialize_sdu_frame(const uint8_t* data);
bool check_sdu_frame(const SDUFrame* frame);

#endif // FRAME_SUBLAYER_H