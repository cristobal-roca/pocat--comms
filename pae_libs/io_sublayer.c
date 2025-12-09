#include "io_sublayer.h"
#include "protocol_definitions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Define the maximum fragmented SDU size
#define NUM_MAX_FRAGMENTS_SDU 1024
#define MAX_BITSEQUENCE_SIZE 8192

// Static counter for pseudo packet ID generation (0-63)
static uint8_t pseudo_packet_counter = 0;

// Check if the SDU is too large to be unsegmented
bool need_fragmentation(uint8_t *OBC_data, size_t OBC_data_size) {
    return (OBC_data_size > MAX_FRAGMENTED_SDU_SIZE);
}
// Create a new PDU header
PDUHeader create_pdu_header(uint8_t version, uint8_t qos, uint8_t pdu_id, uint8_t dfc_id,
    uint16_t sc_id, uint8_t pc_id, uint8_t port_id, uint8_t sd_id, 
    uint16_t data_length, uint8_t fsn) {
    PDUHeader header = {0}; // Initialize header to zero

    // Assign values to the header fields
    header.VersionNum = version; 
    header.QoS = qos; 
    header.PDU_ID = pdu_id;
    header.DFC_ID = dfc_id;
    header.SC_ID_part1 = (sc_id >> 8) & 0xFF;;
    header.SC_ID_part2 = sc_id & 0xFF;
    header.PC_ID = pc_id;
    header.PortID = port_id;
    header.SD_ID = sd_id;
    header.data_length_high = (data_length >> 8) & 0x07; // High part of data_length
    header.data_length_low = data_length & 0xFF; // Low part of data_length
    header.FSN = fsn; // Initialize FSN with the provided value or 0 by default

    return header;
}
// Create a new segmentation header
SegmentationHeader create_segmentation_header(uint8_t seg_flag, uint8_t pseudo_packet_id) {
    SegmentationHeader header;
    header.SegFlag = seg_flag;
    header.PseudoPacketID = pseudo_packet_id;
    return header;
}
// Create a new frame buffer
void create_buffer(IOBuffer *buffer) {
    memset(buffer, 0, sizeof(IOBuffer)); // Initialize entire buffer to zero
    buffer->size = 0;
    buffer->completframes = 0;
}
// Create a new packet_id, used for tracking across all sublayers
uint32_t generate_packet_id(IOBuffer *buffer) {
    // Find an available packet_id in the buffer
    for (uint32_t id = 0; id < NUM_MAX_SEGMENTS; id++) {
        if (!buffer->packet_id_in_use[id]) {
            buffer->packet_id_in_use[id] = true; // Mark ID as in use
            return id;
        }
    }
    fprintf(stderr, "Error: No available packet_id.\n");
    return UINT32_MAX; // Indicate that there are no available IDs
}

// Release a packet_id
void release_packet_id(IOBuffer *buffer, uint32_t packet_id) {
    if (packet_id < NUM_MAX_SEGMENTS) {
        buffer->packet_id_in_use[packet_id] = false; // Mark ID as not in use
    } else {
        fprintf(stderr, "Error: Invalid packet_id.\n");
    }
}

// Segment into multiple segments
IOBuffer segment_sdu(uint8_t *OBC_data, size_t OBC_data_size, uint8_t PortID, uint8_t PDU_ID, uint16_t SC_ID, uint8_t SD_ID, IOBuffer *buffer) {
    size_t num_segments = OBC_data_size / MAX_FRAGMENTED_SDU_SIZE;
    if (OBC_data_size % MAX_FRAGMENTED_SDU_SIZE != 0) {
        num_segments++;
    }

    if (num_segments > NUM_MAX_SEGMENTS) {
        fprintf(stderr, "Error: OBC data size exceeds maximum fragmented numbers.\n");
        return *buffer;
    }

    // Generate unique pseudo packet ID using counter (wraps at 64 since it's 6 bits)
    uint8_t pseudo_packet_id = pseudo_packet_counter;
    pseudo_packet_counter = (pseudo_packet_counter + 1) & 0x3F; // Increment and wrap at 64
    
    uint32_t packet_id = generate_packet_id(buffer);
    buffer->index[packet_id].buffer_position = buffer->size; 
    buffer->index[packet_id].final_position = buffer->size + num_segments - 1;

    for (size_t i = 0; i < num_segments; i++) {
        size_t segment_size = MAX_FRAGMENTED_SDU_SIZE;
        if (i == num_segments - 1) {
             size_t rem = OBC_data_size % MAX_FRAGMENTED_SDU_SIZE;
             if (rem != 0) segment_size = rem;
        }

        PDUHeader pdu_header = create_pdu_header(VERSION_3, EXPEDITED, PDU_ID, DFC_FRAGMENTED, SC_ID, PRIMARYCANAL,
                                                 PortID, SD_ID, segment_size, i);

        SegmentationHeader seg_header;
        if (num_segments == 1) {
            seg_header = create_segmentation_header(NO_SEGMENT, pseudo_packet_id);
        } else if (i == 0) {
            seg_header = create_segmentation_header(FIRST_SEGMENT, pseudo_packet_id);
        } else if (i == num_segments - 1) {
            seg_header = create_segmentation_header(LAST_SEGMENT, pseudo_packet_id);
        } else {
            seg_header = create_segmentation_header(MIDDLE_SEGMENT, pseudo_packet_id);
        }

        SDUFrame frame = {0};
        frame.type = FRAME_FRAGMENTED;
        frame.data.fragmented.pdu_header = pdu_header;
        frame.data.fragmented.seg_header = seg_header;

        frame.data.fragmented.sdu = (uint8_t *)malloc(segment_size);
        if (frame.data.fragmented.sdu == NULL) {
            fprintf(stderr, "Error: Memory allocation failed for fragmented SDU.\n");
            return *buffer;
        }
        memcpy(frame.data.fragmented.sdu, OBC_data + (i * MAX_FRAGMENTED_SDU_SIZE), segment_size);

        buffer->frames[buffer->size] = frame;
        buffer->size++;
    }
    buffer->completframes++;
    return *buffer;
}

// Function to create an unfragmented SDU
SDUFrame create_unfragmented_sdu(uint8_t *OBC_data, size_t OBC_data_size, uint8_t PortID, uint8_t PDU_ID, uint16_t SC_ID, uint8_t SD_ID, IOBuffer *buffer) {
    SDUFrame frame = {0}; // Initialize frame to zero

    // Check if OBC_data is too large to be unfragmented
    if (OBC_data_size > MAX_UNFRAGMENTED_SDU_SIZE) {
        fprintf(stderr, "Error: OBC data size exceeds maximum unfragmented size.\n");
        return frame; // Return an empty frame in case of error
    }

    frame.type = FRAME_UNFRAGMENTED; // Indicate that it is an unfragmented frame

    // Create the PDU header
    PDUHeader pdu_header = create_pdu_header(VERSION_3, EXPEDITED, PDU_ID, DFC_PACKETS, SC_ID, PRIMARYCANAL,
        PortID, SD_ID, OBC_data_size, 0);
    frame.data.unfragmented.header = pdu_header;

    // Allocate dynamic memory for the SDU and copy the data
    frame.data.unfragmented.sdu = (uint8_t *)malloc(OBC_data_size);
    if (frame.data.unfragmented.sdu == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for unfragmented SDU.\n");
        return frame;
    }
    memcpy(frame.data.unfragmented.sdu, OBC_data, OBC_data_size);

    // Save the frame in the buffer
    uint32_t packet_id = generate_packet_id(buffer);
    buffer->index[packet_id].buffer_position = buffer->size;
    buffer->index[packet_id].final_position = buffer->size;
    buffer->frames[buffer->size] = frame;
    buffer->size++;
    buffer->completframes++;

    return frame;
}

// Function to free the frame buffer
void free_buffer(IOBuffer *buffer, uint32_t packet_id) {
    if (packet_id < NUM_MAX_SEGMENTS) {
        size_t start = buffer->index[packet_id].buffer_position;
        size_t end = buffer->index[packet_id].final_position;
        size_t num_frames_to_remove = end - start + 1;

        // Free the frames associated with the packet_id
        for (size_t i = start; i <= end; i++) {
            if (buffer->frames[i].type == FRAME_UNFRAGMENTED) {
                free(buffer->frames[i].data.unfragmented.sdu); // Free memory of unfragmented SDU
            } else if (buffer->frames[i].type == FRAME_FRAGMENTED) {
                free(buffer->frames[i].data.fragmented.sdu); // Free memory of fragmented SDU
            }
            memset(&buffer->frames[i], 0, sizeof(SDUFrame)); // Clear the frame
        }

        // Move the remaining frames to fill the gap
        for (size_t i = end + 1; i < buffer->size; i++) {
            buffer->frames[i - num_frames_to_remove] = buffer->frames[i];
        }

        // Update buffer size
        buffer->size -= num_frames_to_remove;

        // Update the index positions in the buffer
        for (uint32_t i = 0; i < NUM_MAX_SEGMENTS; i++) {
            if (buffer->packet_id_in_use[i] && buffer->index[i].buffer_position > end) {
                buffer->index[i].buffer_position -= num_frames_to_remove;
                buffer->index[i].final_position -= num_frames_to_remove;
            }
        }
        buffer->packet_id_in_use[packet_id] = false; // Mark ID as not in use
        buffer->index[packet_id].buffer_position = 0; // Reset the packet's buffer position
        buffer->index[packet_id].final_position = 0; // Reset the packet's final buffer position
        buffer->index[packet_id].in_nextsublayer = false; // Reset the packet's next sublayer index
    } else {
        fprintf(stderr, "Error: Invalid packet_id.\n");
    }
    buffer->completframes--; // Decrement the number of complete frames in the buffer
}

// Send to the next sublayer
SDUFrame* send_to_next_sublayer(IOBuffer *buffer, uint32_t packet_id, size_t *frames_count) {
    if (frames_count == NULL) {
        fprintf(stderr, "Error: frames_count must not be NULL.\n");
        return NULL;
    }
    *frames_count = 0;

    SDUFrame* frames_to_send = NULL; // Initialize pointer to frames to send
    if (packet_id >= NUM_MAX_SEGMENTS) {
        fprintf(stderr, "Error: Invalid packet_id.\n");
        return NULL;
    }

    size_t start = buffer->index[packet_id].buffer_position;
    size_t end = buffer->index[packet_id].final_position;
    if (end < start) {
        // nothing to send
        return NULL;
    }

    size_t num_frames_to_send = end - start + 1;
    frames_to_send = (SDUFrame*)malloc(num_frames_to_send * sizeof(SDUFrame)); // Allocate memory for frames to send
    if (frames_to_send == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for frames array.\n");
        return NULL;
    }

    // Initialize to zero to prevent failures
    for (size_t k = 0; k < num_frames_to_send; ++k) {
        memset(&frames_to_send[k], 0, sizeof(SDUFrame));
    }

    // Copy frames and duplicate SDU payloads safely
    for (size_t i = start; i <= end; i++) {
        size_t idx = i - start;
        SDUFrame *src = &buffer->frames[i];
        if (src->type == FRAME_UNFRAGMENTED) {
            frames_to_send[idx].type = FRAME_UNFRAGMENTED;
            frames_to_send[idx].data.unfragmented.header = src->data.unfragmented.header;
            size_t len = src->data.unfragmented.header.data_length_low;
            if (len > 0) {
                frames_to_send[idx].data.unfragmented.sdu = (uint8_t*)malloc(len);
                if (frames_to_send[idx].data.unfragmented.sdu == NULL) {
                    fprintf(stderr, "Error: Memory allocation failed for unfragmented SDU.\n");
                    // cleanup
                    for (size_t j = 0; j < idx; ++j) {
                        if (frames_to_send[j].type == FRAME_UNFRAGMENTED && frames_to_send[j].data.unfragmented.sdu) free(frames_to_send[j].data.unfragmented.sdu);
                        if (frames_to_send[j].type == FRAME_FRAGMENTED && frames_to_send[j].data.fragmented.sdu) free(frames_to_send[j].data.fragmented.sdu);
                    }
                    free(frames_to_send);
                    return NULL;
                }
                memcpy(frames_to_send[idx].data.unfragmented.sdu, src->data.unfragmented.sdu, len);
            } else {
                frames_to_send[idx].data.unfragmented.sdu = NULL;
            }
        } else if (src->type == FRAME_FRAGMENTED) {
            frames_to_send[idx].type = FRAME_FRAGMENTED;
            frames_to_send[idx].data.fragmented.pdu_header = src->data.fragmented.pdu_header;
            frames_to_send[idx].data.fragmented.seg_header = src->data.fragmented.seg_header;
            size_t seg_len = src->data.fragmented.pdu_header.data_length_low;
            if (seg_len > 0) {
                frames_to_send[idx].data.fragmented.sdu = (uint8_t*)malloc(seg_len);
                if (frames_to_send[idx].data.fragmented.sdu == NULL) {
                    fprintf(stderr, "Error: Memory allocation failed for fragmented SDU.\n");
                    // cleanup
                    for (size_t j = 0; j < idx; ++j) {
                        if (frames_to_send[j].type == FRAME_UNFRAGMENTED && frames_to_send[j].data.unfragmented.sdu) free(frames_to_send[j].data.unfragmented.sdu);
                        if (frames_to_send[j].type == FRAME_FRAGMENTED && frames_to_send[j].data.fragmented.sdu) free(frames_to_send[j].data.fragmented.sdu);
                    }
                    free(frames_to_send);
                    return NULL;
                }
                memcpy(frames_to_send[idx].data.fragmented.sdu, src->data.fragmented.sdu, seg_len);
            } else {
                frames_to_send[idx].data.fragmented.sdu = NULL;
            }
        } else {
            // Unknown type: leave zeroed entry
            frames_to_send[idx].type = src->type;
        }
    }

    // success: report count and mark buffer entry
    *frames_count = num_frames_to_send;
    buffer->index[packet_id].in_nextsublayer = true; // Mark the packet as sent to the next sublayer
    return frames_to_send; // Return the frames to send
}

uint32_t get_first_packet_id(IOBuffer *buffer) {
        for (uint32_t id = 0; id < NUM_MAX_SEGMENTS; id++) {
            if (buffer->packet_id_in_use[id] && buffer->index[id].buffer_position == 0) {
                return id; // Return the packet_id corresponding to the first element
            }
        }
        fprintf(stderr, "Error: No valid packet_id found for the first element.\n");
        return UINT32_MAX; // Indicate that no valid packet_id was found
}

// I/O sublayer Rx

bool need_more_seg(SDUFrame frame) {
    if (frame.type == FRAME_UNFRAGMENTED) {
        return false; // The frame is not fragmented
    }
    if (frame.data.fragmented.seg_header.SegFlag == LAST_SEGMENT || frame.data.fragmented.seg_header.SegFlag == NO_SEGMENT) {
        return false; // The frame is the last segment or not a segment
    }
    return true; // The frame is an intermediate or first segment   
}
 void serialize_to_obc(SDUFrame frame, SerializedData* bufferserialized) {
    if (frame.type == FRAME_UNFRAGMENTED) {
        // Copy only the data of the unfragmented SDU
        memcpy(bufferserialized->data, frame.data.unfragmented.sdu, frame.data.unfragmented.header.data_length_low);
        bufferserialized->length = frame.data.unfragmented.header.data_length_low;
    } else if (frame.type == FRAME_FRAGMENTED) {
        // Add the data of the fragmented SDU at the end of bufferserialized->data (without headers)
        size_t segment_size = frame.data.fragmented.pdu_header.data_length_low;
        memcpy(bufferserialized->data + bufferserialized->length, frame.data.fragmented.sdu, segment_size);
        bufferserialized->length += segment_size;
    }
}

bitsecuence create_bitsecuence(SerializedData* bufferserialized) {
    bitsecuence bits = {0};
    size_t bit_index = 0;

    if (bufferserialized == NULL) return bits;

    // Iterar solo sobre los bytes realmente usados
    for (size_t i = 0; i < bufferserialized->length; i++) {
        uint8_t byte = bufferserialized->data[i];
        for (int b = 7; b >= 0; b--) {
            if (bit_index >= MAX_BITSEQUENCE_SIZE) break;
            bits.bits[bit_index++] = (byte >> b) & 0x01;
        }
        if (bit_index >= MAX_BITSEQUENCE_SIZE) break;
    }

    bits.bit_length = bit_index;

    // Opcional: limpiar solo la parte usada o toda la capacidad
    // memset(bufferserialized->data, 0, bufferserialized->length); // limpia la porción usada
    memset(bufferserialized->data, 0, sizeof(bufferserialized->data)); // limpia toda la capacidad
    bufferserialized->length = 0;

    return bits;
}

