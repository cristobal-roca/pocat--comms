#ifndef IO_SUBLAYER_H
#define IO_SUBLAYER_H

#include "protocol_definitions.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Fundamental constants
#define NUM_MAX_SEGMENTS 255
#define FIRST_SEGMENT 0b01 // First segment
#define LAST_SEGMENT 0b10 // Last segment
#define MIDDLE_SEGMENT 0b00 // Middle segment
#define NO_SEGMENT 0b11 // No segment

typedef struct {
    size_t buffer_position;  // Packet position in the buffer
    size_t final_position;   // Final position of the packet in the buffer
    bool in_nextsublayer;    // Indicates if the packet is in the next sublayer
} BufferIndexEntry;

typedef struct {
    SDUFrame frames[NUM_MAX_SEGMENTS]; // Array of SDUFrame with a maximum of NUM_MAX_SEGMENTS
    BufferIndexEntry index[NUM_MAX_SEGMENTS]; // Array of indices to access the frames
    bool packet_id_in_use[NUM_MAX_SEGMENTS]; // Record of packet_id in use (bitmap)
    size_t completframes;               // Number of complete frames in the buffer
    size_t size;                        // Number of frames currently in the array
} IOBuffer; // Buffer to store segmented and non-segmented frames

// Function declarations

bool need_fragmentation(uint8_t *OBC_data, size_t OBC_data_size);

PDUHeader create_pdu_header(uint8_t version, uint8_t qos, uint8_t pdu_id, uint8_t dfc_id,
    uint16_t sc_id, uint8_t pc_id, uint8_t port_id, uint8_t sd_id, uint16_t data_length, uint8_t fsn);

SegmentationHeader create_segmentation_header(uint8_t seg_flag, uint8_t pseudo_packet_id);

void create_buffer(IOBuffer *buffer);

uint32_t generate_packet_id(IOBuffer *buffer);

void release_packet_id(IOBuffer *buffer, uint32_t packet_id);

IOBuffer segment_sdu(uint8_t *OBC_data, size_t OBC_data_size, uint8_t PortID, uint8_t PDU_ID,
    uint16_t SC_ID, uint8_t SD_ID, IOBuffer *buffer);

SDUFrame create_unfragmented_sdu(uint8_t *OBC_data, size_t OBC_data_size, uint8_t PortID,
    uint8_t PDU_ID, uint16_t SC_ID, uint8_t SD_ID, IOBuffer *buffer);

void free_buffer(IOBuffer *buffer, uint32_t packet_id);

// Prepare a copy of the frames associated with `packet_id` for the next sublayer.
// The function allocates an array of SDUFrame and duplicates per-frame SDU bytes.
// On success returns the allocated SDUFrame* and writes the number of elements to *out_count.
// The caller is responsible for freeing the returned frames and their internal SDU buffers.
// On error returns NULL and *out_count is set to 0.
SDUFrame* send_to_next_sublayer(IOBuffer *buffer, uint32_t packet_id, size_t *out_count);

uint32_t get_first_packet_id(IOBuffer *buffer);

bool need_more_seg(SDUFrame frame);

void serialize_to_obc(SDUFrame frame, SerializedData* bufferserialized);

bitsecuence create_bitsecuence(SerializedData* bufferserialized);
#endif // IO_SUBLAYER_H