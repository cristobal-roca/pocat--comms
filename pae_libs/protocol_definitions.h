#ifndef PROTOCOL_DEFINITIONS_H
#define PROTOCOL_DEFINITIONS_H

#include <stdint.h>
#include <stdbool.h>

// Fundamental constants
#define MAX_UNFRAGMENTED_SDU_SIZE 250 //5pdu HEADER
#define MAX_FRAGMENTED_SDU_SIZE 249 //1 Fragamentation HEADER
#define MAX_TOTAL_FRAME_SIZE 255
#define VERSION_3 0b10 // Version 3 of Proximity-1
#define EXPEDITED 1 // Expedited QoS mode
#define SEQUENCED_CONTROLLEED 0 // Sequenced Controlled QoS mode
#define PRIMARYCANAL 1 // Primary Channel
#define BACKUPCANAL 0 // Backup Channel
#define DFC_PACKETS 0b00 // integer number of unsegmented packets (2 bits)
#define DFC_FRAGMENTED 0b01 // a complete or segmented packet (2 bits)
#define DFC_CCSDS 0b10 // Reserved for future CCSDS definitions (2 bits)
#define DFC_RESERVED 0b11 // Reserved for future user definitions (2 bits)
#define SIZE_PDU_HEADER 5 // PDU header size (5 bytes)
#define SIZE_SEGMENTATION_HEADER 1 // Segmentation header size (1 byte)
#define PDU_DATA 0x00 // Data PDU ID (1 byte)
#define PDU_COMMAND 0x01 // Command PDU ID (protocol/supervisory data) (1 byte)

// Definitions for Frame Sublayer
typedef enum {
    FRAME_UNFRAGMENTED = 0,
    FRAME_FRAGMENTED = 1
} FrameType;

// Structure for the PDU header (bitfields ordered for little-endian ARM memcpy compatibility)
typedef struct __attribute__((packed)) {
    // Byte 0: [bits 7-0] = [SC_ID₁₋₂|DFC_ID|PDU_ID|QoS|Ver]
    uint8_t VersionNum : 2;   // Proximity-1 Version [bits 0-1]
    uint8_t QoS : 1;          // Quality of Service [bit 2]
    uint8_t PDU_ID : 1;       // PDU ID [bit 3]
    uint8_t DFC_ID : 2;       // Data Field Construction ID [bits 4-5]
    uint8_t SC_ID_part1 : 2;  // High part of SC_ID [bits 6-7]
    
    // Byte 1: SC_ID low byte
    uint8_t SC_ID_part2 : 8;  // Low part of SC_ID [bits 0-7]
    
    // Byte 2: [bits 7-0] = [DataLen₉₋₁₁|SD_ID|PortID|PC_ID]
    uint8_t PC_ID : 1;        // Physical Channel ID [bit 0]
    uint8_t PortID : 3;       // Port ID [bits 1-3]
    uint8_t SD_ID : 1;        // Source Destination ID [bit 4]
    uint8_t data_length_high : 3; // High part of data_length [bits 5-7]
    
    // Byte 3: Data length low byte
    uint8_t data_length_low : 8;  // Low part of data_length [bits 0-7]
    
    // Byte 4: Frame sequence number
    uint8_t FSN : 8;          // Frame Sequence Number [bits 0-7]
} PDUHeader;

// Structure for the segmentation header (bitfields ordered for little-endian)
typedef struct __attribute__((packed)) {
    uint8_t SegFlag : 2;      // Segmentation Flag [bits 0-1]
    uint8_t PseudoPacketID : 6; // Pseudo Packet ID [bits 2-7]
} SegmentationHeader;

// Structure for the SDU
// Structure representing a Service Data Unit (SDU) Frame, which can be either unfragmented or fragmented
typedef struct SDUFrame {
    FrameType type; // Indicates if the frame is unfragmented or fragmented

    union {
        // Structure for unfragmented SDU frames
        struct {
            PDUHeader header; // PDU header for the frame
            uint8_t *sdu;     // Pointer to the SDU data
        } unfragmented;

        // Structure for fragmented SDU frames
        struct {
            PDUHeader pdu_header;        // PDU header for the frame
            SegmentationHeader seg_header; // Segmentation header for fragmented frames
            uint8_t *sdu;                // Pointer to the SDU data fragment
        } fragmented;
    } data;

    struct SDUFrame *next; // Pointer to the next SDUFrame in a linked list (for chaining frames)
} SDUFrame;

typedef struct {
    uint8_t* data;    // Serialized bytes
    uint32_t length;  // Length in bytes
} SerializedData;

typedef struct {
    uint8_t* bits;        // Bit sequence stored compactly (each bit is data)
    uint32_t bit_length;  // Length in bits
} bitsecuence;
#endif // PROTOCOL_DEFINITIONS_H