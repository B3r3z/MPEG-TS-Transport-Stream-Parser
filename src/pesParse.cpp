/**
 * @file pesParse.cpp
 * @brief Implementation of MPEG-2 Packetized Elementary Stream (PES) parsing classes
 * 
 * This file implements PES packet header parsing and PES packet assembly from multiple
 * Transport Stream packets according to ISO/IEC 13818-1 standard. It provides functionality
 * for extracting timing information (PTS/DTS), stream identification, and packet validation.
 * 
 * PES packets carry compressed audio/video data and are typically larger than single
 * TS packets, requiring assembly across multiple TS packets using continuity counters
 * and Payload Unit Start Indicator (PUSI) flags.
 * 
 * @author Bartosz Berezowski
 * @date 2025
 * @version 1.0
 */

#include "../include/pesParse.h"
#include <cstring>
#include <cstdio>

//=============================================================================================================================================================================
// xPES_PacketHeader Implementation
//=============================================================================================================================================================================

/**
 * @brief Resets all PES packet header fields to default/uninitialized state
 * 
 * Initializes all header components to zero/false, preparing the object for parsing
 * a new PES packet. This includes basic header fields, optional extension fields,
 * timing stamps, and internal state variables.
 * 
 * The PES packet structure consists of:
 * - 6-byte basic header (start code prefix + stream ID + packet length)
 * - Optional extended header (flags + timing + additional fields)
 * - Payload data (compressed audio/video/data)
 * 
 * @note After reset, m_headerLength is set to 6 (basic header size)
 * @note All timing stamps (PTS/DTS) are cleared to zero
 * @note Extension flag is set to false, indicating no extended header parsed
 */
void xPES_PacketHeader::Reset()
{
  // Basic PES header fields (always present)
  m_PacketStartCodePrefix = 0;  // Should be 0x000001 for valid PES packets
  m_StreamId = 0;               // Stream type identifier (audio/video/data)
  m_PacketLength = 0;           // PES packet length (excluding start code and length field)
  
  // Optional header extension fields
  m_HeaderMarkerBits = 0;           // Marker bits - should be '10' (binary)
  m_PES_scrambling_control = 0;     // Scrambling control (0=not scrambled)
  m_PES_priority = 0;               // PES priority flag
  m_data_alignment_indicator = 0;   // Data alignment indicator
  m_copyright = 0;                  // Copyright flag
  m_original_or_copy = 0;           // Original or copy flag
  m_PTS_DTS_flags = 0;              // PTS/DTS flags (00=none, 10=PTS only, 11=both)
  m_ESCR_flag = 0;                  // Elementary Stream Clock Reference flag
  m_ES_rate_flag = 0;               // Elementary Stream rate flag
  m_DSM_trick_mode_flag = 0;        // DSM trick mode flag
  m_additional_copy_info_flag = 0;  // Additional copy info flag
  m_PES_CRC_flag = 0;               // PES CRC flag
  m_PES_extension_flag = 0;         // PES extension flag
  m_PES_header_data_length = 0;     // Length of optional header data
  
  // Timing information (33-bit timestamps)
  m_PTS = 0;  // Presentation Time Stamp (when to present/display)
  m_DTS = 0;  // Decoding Time Stamp (when to decode)
  
  // Internal state management
  m_hasHeaderExtension = false;     // Indicates if extended header was parsed
  m_headerLength = 6;               // Total header length (6 bytes minimum)
}

/**
 * @brief Parses MPEG-2 PES packet header according to ISO/IEC 13818-1 specification
 * 
 * Extracts and validates all fields from PES packet header, including basic header
 * and optional extended header with timing information. The PES header structure:
 * 
 * Basic Header (6 bytes):
 * - packet_start_code_prefix (24 bits) = 0x000001
 * - stream_id (8 bits) - identifies stream type
 * - PES_packet_length (16 bits) - length excluding basic header
 * 
 * Extended Header (variable length, present for audio/video streams):
 * - Marker bits '10' + scrambling/priority/alignment flags (8 bits)
 * - PTS/DTS flags + other optional field flags (8 bits)  
 * - PES_header_data_length (8 bits)
 * - Optional fields: PTS (5 bytes), DTS (5 bytes), other extensions
 * 
 * @param Input Pointer to buffer containing PES packet data
 * @return Total header length in bytes on success, NOT_VALID on failure
 * 
 * @retval >=6 Successfully parsed header, returns total header length
 * @retval NOT_VALID Invalid input pointer or malformed PES header
 * 
 * @note PTS/DTS are 33-bit timestamps with special 5-byte encoding format
 * @note Extended header only present for certain stream IDs (audio/video streams)
 * @note Stream IDs 0xC0-0xDF=audio, 0xE0-0xEF=video, others=special purpose
 */
int32_t xPES_PacketHeader::Parse(const uint8_t* Input)
{
  // Validate input parameter
  if (!Input) return NOT_VALID;

  // Parse and validate packet start code prefix (must be 0x000001)
  m_PacketStartCodePrefix = (Input[0] << 16) | (Input[1] << 8) | Input[2];
  if (m_PacketStartCodePrefix != 0x000001) return NOT_VALID;

  // Extract stream ID and packet length from basic header
  m_StreamId = Input[3];                          // Stream type identifier
  m_PacketLength = (Input[4] << 8) | Input[5];    // PES packet length field
  
  // Initialize header length to basic header size
  m_headerLength = xTS::PES_HeaderLength; // 6 bytes for basic header
  
  // Determine if extended header is present based on stream ID
  // Extended headers are used for elementary streams (audio/video) but not for
  // special purpose streams like padding, private streams, etc.
  if (m_StreamId != eStreamId_program_stream_map && 
      m_StreamId != eStreamId_padding_stream && 
      m_StreamId != eStreamId_private_stream_2 && 
      m_StreamId != eStreamId_ECM && 
      m_StreamId != eStreamId_EMM && 
      m_StreamId != eStreamId_program_stream_directory &&
      m_StreamId != eStreamId_DSMCC_stream && 
      m_StreamId != eStreamId_ITUT_H222_1_type_E &&
      // Verify stream ID is in valid range for audio/video elementary streams
      ((m_StreamId >= 0xC0 && m_StreamId <= 0xDF) || // Audio streams (MPEG/AC-3/etc)
       (m_StreamId >= 0xE0 && m_StreamId <= 0xEF)))   // Video streams (MPEG/H.264/etc)
  {
    m_hasHeaderExtension = true;
    
    // Validate minimum packet length for extended header (requires 3+ additional bytes)
    if (m_PacketLength < 3) return m_headerLength; 
    
    // Parse and validate marker bits from first extension byte
    uint8_t markerBits = (Input[6] & 0xC0) >> 6;
    if (markerBits != 0x2) {
      // Invalid marker bits - log warning but continue parsing
      printf("Warning: Invalid PES header marker bits: 0x%02X (expected 0x2)\n", markerBits);
    }
    
    // Parse extension header byte 1 (byte 6): marker + scrambling + priority flags
    m_HeaderMarkerBits = (Input[6] & 0xC0) >> 6;       // Bits 7-6: should be '10'
    m_PES_scrambling_control = (Input[6] & 0x30) >> 4; // Bits 5-4: scrambling control
    m_PES_priority = (Input[6] & 0x08) >> 3;           // Bit 3: PES priority
    m_data_alignment_indicator = (Input[6] & 0x04) >> 2; // Bit 2: data alignment
    m_copyright = (Input[6] & 0x02) >> 1;               // Bit 1: copyright
    m_original_or_copy = Input[6] & 0x01;               // Bit 0: original/copy
    
    // Parse extension header byte 2 (byte 7): PTS/DTS flags + other optional flags
    m_PTS_DTS_flags = (Input[7] & 0xC0) >> 6;           // Bits 7-6: PTS/DTS presence
    m_ESCR_flag = (Input[7] & 0x20) >> 5;               // Bit 5: ESCR flag
    m_ES_rate_flag = (Input[7] & 0x10) >> 4;            // Bit 4: ES rate flag
    m_DSM_trick_mode_flag = (Input[7] & 0x08) >> 3;     // Bit 3: DSM trick mode
    m_additional_copy_info_flag = (Input[7] & 0x04) >> 2; // Bit 2: additional copy info
    m_PES_CRC_flag = (Input[7] & 0x02) >> 1;             // Bit 1: PES CRC flag
    m_PES_extension_flag = Input[7] & 0x01;               // Bit 0: PES extension flag
    
    // Parse extension header byte 3 (byte 8): length of optional header data
    m_PES_header_data_length = Input[8];
    
    // Calculate total PES header length: basic(6) + extension header(3) + optional data(N)
    m_headerLength = xTS::PES_HeaderLength + 3 + m_PES_header_data_length;
    
    // Parse optional timing stamps (PTS/DTS) if present
    int offset = 9; // Start of optional header data
    
    // Parse Presentation Time Stamp (PTS) - 33-bit timestamp
    if (m_PTS_DTS_flags & 0x2) { // Check if PTS flag is set (bit 1)
      // PTS encoding format (5 bytes total):
      // Byte 0: '0010' (4 bits) + PTS[32:30] (3 bits) + marker_bit (1 bit)
      // Byte 1: PTS[29:22] (8 bits) 
      // Byte 2: PTS[21:15] (7 bits) + marker_bit (1 bit)
      // Byte 3: PTS[14:7] (8 bits)
      // Byte 4: PTS[6:0] (7 bits) + marker_bit (1 bit)
      
      uint64_t pts_32_30 = (Input[offset] & 0x0E) >> 1;      // Upper 3 bits of PTS
      uint64_t pts_29_22 = Input[offset + 1];                // Next 8 bits of PTS
      uint64_t pts_21_15 = (Input[offset + 2] & 0xFE) >> 1;  // Next 7 bits of PTS
      uint64_t pts_14_7  = Input[offset + 3];                // Next 8 bits of PTS
      uint64_t pts_6_0   = (Input[offset + 4] & 0xFE) >> 1;  // Lower 7 bits of PTS
      
      // Combine all PTS components into 33-bit timestamp
      m_PTS = (pts_32_30 << 30) | (pts_29_22 << 22) | (pts_21_15 << 15) | 
              (pts_14_7 << 7) | pts_6_0;
      offset += 5; // Advance past PTS data
    }
    
    // Parse Decoding Time Stamp (DTS) - only present when both PTS and DTS flags set
    if (m_PTS_DTS_flags == 0x3) { // Both PTS and DTS present (flags = '11')
      // DTS uses same encoding format as PTS
      uint64_t dts_32_30 = (Input[offset] & 0x0E) >> 1;      // Upper 3 bits of DTS
      uint64_t dts_29_22 = Input[offset + 1];                // Next 8 bits of DTS  
      uint64_t dts_21_15 = (Input[offset + 2] & 0xFE) >> 1;  // Next 7 bits of DTS
      uint64_t dts_14_7  = Input[offset + 3];                // Next 8 bits of DTS
      uint64_t dts_6_0   = (Input[offset + 4] & 0xFE) >> 1;  // Lower 7 bits of DTS
      
      // Combine all DTS components into 33-bit timestamp
      m_DTS = (dts_32_30 << 30) | (dts_29_22 << 22) | (dts_21_15 << 15) | 
              (dts_14_7 << 7) | dts_6_0;
      offset += 5; // Advance past DTS data
    }
    
    // Note: Additional optional fields (ESCR, ES_rate, DSM_trick_mode, etc.) 
    // could be parsed here based on their respective flags, but PTS/DTS are
    // the most commonly used timing references for basic stream processing
  }

  return m_headerLength; // Return total header length consumed
}

/**
 * @brief Prints PES packet header information in formatted output for debugging
 * 
 * Outputs essential PES header fields in standardized format for analysis and debugging.
 * Includes packet start code prefix, stream ID, packet length, and timing information
 * when available.
 * 
 * Basic output format: "PES: PSCP=0xXXXXXX SID=0xXX PLEN=XXXX"
 * Extended output adds: "HeaderLen=XX PTS=XXXXXXXX DTS=XXXXXXXX" (when present)
 * 
 * Where:
 * - PSCP: Packet Start Code Prefix (should be 0x000001)
 * - SID: Stream ID (0xC0-0xDF=audio, 0xE0-0xEF=video, others=special)
 * - PLEN: PES Packet Length in bytes (0=unbounded/unknown length)
 * - HeaderLen: Total header length including extensions
 * - PTS: Presentation Time Stamp (33-bit, 90kHz units)
 * - DTS: Decoding Time Stamp (33-bit, 90kHz units)
 * 
 * @note PTS represents when content should be presented/displayed
 * @note DTS represents when content should be decoded (always <= PTS)
 * @note Timing stamps are in 90kHz clock units (MPEG-2 system time base)
 */
void xPES_PacketHeader::Print() const
{
  printf("PES: PSCP=0x%06X SID=0x%02X PLEN=%d", 
         m_PacketStartCodePrefix, m_StreamId, m_PacketLength);
  
  // Display extended header information if present
  if (m_hasHeaderExtension) {
    printf(" HeaderLen=%d", m_headerLength);
    
    // Display timing information when available
    if (hasPTS()) {
      printf(" PTS=%" PRIu64, m_PTS);
    }
    if (hasDTS()) {
      printf(" DTS=%" PRIu64, m_DTS);
    }
  }
}

//=============================================================================================================================================================================
// xPES_Assembler Implementation  
//=============================================================================================================================================================================

/**
 * @brief Default constructor - initializes PES assembler with default values
 * 
 * Creates a new PES assembler instance with all internal state variables set to
 * safe default values. The assembler is ready for initialization with a specific
 * PID via the Init() method.
 * 
 * @note Buffer is initially null and will be allocated dynamically as needed
 * @note Assembler is not started until first packet with PUSI flag is received
 */
xPES_Assembler::xPES_Assembler()
  : m_PID(0)                    // Target PID (will be set by Init())
  , m_Buffer(nullptr)           // Dynamic buffer for PES packet assembly
  , m_BufferSize(0)             // Current allocated buffer size
  , m_DataOffset(0)             // Current write position in buffer
  , m_LastContinuityCounter(0)  // Last seen continuity counter value
  , m_Started(false)            // Assembly state flag
  , m_TotalStuffingBytes(0)     // Accumulated stuffing bytes count
{
}

/**
 * @brief Destructor - cleans up dynamically allocated memory
 * 
 * Ensures proper cleanup of the internal buffer to prevent memory leaks.
 * Called automatically when assembler object goes out of scope.
 */
xPES_Assembler::~xPES_Assembler()
{
  if (m_Buffer)
  {
    delete[] m_Buffer;
    m_Buffer = nullptr;
  }
}

/**
 * @brief Initializes the assembler for a specific PID
 * 
 * Prepares the assembler to process TS packets belonging to the specified PID.
 * Resets all internal state variables and prepares for new PES packet assembly.
 * 
 * @param PID Packet Identifier (0-8191) to monitor for PES packets
 * 
 * @note PID 136 is commonly used for audio streams in DVB broadcasts
 * @note Call this method before processing any TS packets
 * @note Previous assembly state and buffers are completely reset
 */
void xPES_Assembler::Init(int32_t PID)
{
  m_PID = PID;                  // Set target PID for packet filtering
  m_Started = false;            // Reset assembly state
  m_LastContinuityCounter = 0;  // Reset continuity tracking
  m_TotalStuffingBytes = 0;     // Reset stuffing byte accumulator
  
  // Reset internal buffers and PES header state
  xBufferReset();
  m_PESH.Reset();
}

/**
 * @brief Processes a Transport Stream packet and extracts PES packet data
 * 
 * Core method for assembling PES packets from multiple TS packets. Handles packet
 * validation, continuity checking, PUSI flag processing, and incremental data assembly.
 * PES packets are typically larger than 188-byte TS packets and require assembly
 * across multiple TS packets using continuity counters.
 * 
 * Processing flow:
 * 1. Validate PID matches target PID
 * 2. Check continuity counter sequence  
 * 3. Handle PUSI (new PES packet start) or continuation
 * 4. Extract payload data and append to buffer
 * 5. Check for PES packet completion
 * 
 * @param TransportStreamPacket Pointer to complete 188-byte TS packet
 * @param PacketHeader Parsed TS packet header containing PID, flags, etc.
 * @param AdaptationField Parsed adaptation field (may be null if not present)
 * @return Assembly result indicating current state or completion
 * 
 * @retval AssemblingStarted New PES packet started, header parsed successfully
 * @retval AssemblingContinue Continuing assembly of current PES packet
 * @retval AssemblingFinished PES packet completed and ready for processing
 * @retval UnexpectedPID Packet PID doesn't match target PID
 * @retval StreamPackedLost Continuity error detected or invalid packet structure
 * 
 * @note PUSI flag indicates start of new PES packet (always check this first)
 * @note Continuity counter must increment by 1 for each packet with payload
 * @note Stuffing bytes in adaptation field don't affect PES packet length
 */
xPES_Assembler::eResult xPES_Assembler::AbsorbPacket(const uint8_t* TransportStreamPacket, 
                                                     const xTS_PacketHeader* PacketHeader, 
                                                     const xTS_AdaptationField* AdaptationField)
{
  // Validate that packet belongs to our target PID
  if (PacketHeader->getPID() != m_PID)
  {
    return eResult::UnexpectedPID;
  }

  // Perform continuity counter checking for ongoing assembly
  if (m_Started)
  {
    // Calculate expected continuity counter value
    // Note: Continuity counter only increments for packets containing payload data
    uint8_t expectedCC;
    if (PacketHeader->getAdaptationFieldControl() == 0 || 
        PacketHeader->getAdaptationFieldControl() == 2) {
      // Packets with adaptation field only (no payload) don't increment counter
      expectedCC = m_LastContinuityCounter;
    } else {
      // Packets with payload increment counter (modulo 16)
      expectedCC = (m_LastContinuityCounter + 1) & 0x0F;
    }
    
    // Check for continuity errors (missing packets)
    if (PacketHeader->getContinuityCounter() != expectedCC)
    {
      // Detected packet loss - abort current assembly
      m_Started = false;
      return eResult::StreamPackedLost;
    }
  }
  
  // Update continuity counter for packets containing payload data
  if (PacketHeader->getAdaptationFieldControl() == 1 || 
      PacketHeader->getAdaptationFieldControl() == 3) {
    m_LastContinuityCounter = PacketHeader->getContinuityCounter();
  }

  // Calculate payload offset within TS packet
  uint32_t payloadOffset = xTS::TS_HeaderLength; // Start after 4-byte TS header
  
  // Adjust offset if adaptation field is present
  if (PacketHeader->getAdaptationFieldControl() & 0x2)
  {
    // Accumulate stuffing bytes for accurate length verification
    m_TotalStuffingBytes += AdaptationField->getStuffingBytes();
    
    // Skip adaptation field: length byte (1) + adaptation field content
    payloadOffset += AdaptationField->getAdaptationFieldLength() + 1;
  }
  
  // Validate that packet contains payload data
  if (payloadOffset >= xTS::TS_PacketLength || 
      PacketHeader->getAdaptationFieldControl() == 0 || 
      PacketHeader->getAdaptationFieldControl() == 2)
  {
    // No payload present - return appropriate state
    return m_Started ? eResult::AssemblingContinue : eResult::StreamPackedLost;
  }
  
  // Calculate actual payload size within this TS packet
  uint32_t payloadSize = xTS::TS_PacketLength - payloadOffset;
  
  // Get pointer to payload data within TS packet
  const uint8_t* payload = TransportStreamPacket + payloadOffset;

  // Handle Payload Unit Start Indicator (PUSI) - indicates new PES packet start
  if (PacketHeader->getPayloadUnitStartIndicator())
  {
    // Start new PES packet assembly - reset all state
    m_Started = true;
    xBufferReset();    // Clear any previous data
    m_PESH.Reset();    // Reset PES header parser
    
    // Parse PES packet header from payload start
    int32_t pesHeaderLength = m_PESH.Parse(payload);
    if (pesHeaderLength < 0)
    {
      // Invalid PES header - abort assembly
      m_Started = false;
      return eResult::UnexpectedPID;
    }
    
    // Store complete payload data (including PES header) in assembly buffer
    xBufferAppend(payload, payloadSize);
    
    // Check if this PES packet is complete within single TS packet
    if (m_PESH.getPacketLength() > 0 && 
        (6 + static_cast<uint32_t>(m_PESH.getPacketLength())) <= payloadSize)
    {
      // Complete PES packet contained in single TS packet
      return eResult::AssemblingFinished;
    }
    
    return eResult::AssemblingStarted;
  }
  else if (m_Started)
  {
    // Continue assembling existing PES packet (no PUSI flag)
    xBufferAppend(payload, payloadSize);
    
    // Check if PES packet assembly is now complete
    if (m_PESH.getPacketLength() > 0)
    {
      // Calculate expected total PES packet size:
      // 6 bytes (basic header) + PacketLength field value
      uint32_t expectedSize = 6 + m_PESH.getPacketLength();
      
      if (m_DataOffset >= expectedSize)
      {
        // PES packet assembly completed
        return eResult::AssemblingFinished;
      }
    }
    
    return eResult::AssemblingContinue;
  }
  
  // Received continuation packet without having started assembly
  // This indicates either stream start or packet loss
  return eResult::StreamPackedLost;
}

/**
 * @brief Resets internal assembly buffer and deallocates memory
 * 
 * Clears the internal buffer used for PES packet assembly and resets all
 * buffer management variables to initial state. This prepares the assembler
 * for a new PES packet assembly session.
 * 
 * @note Deallocates buffer memory to prevent memory leaks
 * @note Called automatically when starting new PES packet assembly
 * @note Safe to call multiple times - checks for null pointer
 */
void xPES_Assembler::xBufferReset()
{
  // Deallocate existing buffer if present
  if (m_Buffer)
  {
    delete[] m_Buffer;
    m_Buffer = nullptr;
  }
  
  // Reset buffer management variables
  m_BufferSize = 0;   // No allocated space
  m_DataOffset = 0;   // No data stored
}

/**
 * @brief Appends data to internal assembly buffer with automatic resizing
 * 
 * Dynamically expands the internal buffer as needed to accommodate new data.
 * Uses exponential growth strategy to minimize memory reallocations while
 * assembling large PES packets from multiple TS packet payloads.
 * 
 * Buffer expansion strategy:
 * 1. Double current buffer size, or
 * 2. Current data + new data size, whichever is larger
 * 3. Minimum buffer size of 1024 bytes for efficiency
 * 
 * @param Data Pointer to new data to append to buffer
 * @param Size Number of bytes to append from Data
 * 
 * @note Handles memory allocation failures gracefully with error logging
 * @note Preserves existing buffer content during expansion
 * @note Thread-safe within single assembler instance
 * 
 * @warning Does nothing if Data is null or Size is <= 0
 */
void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size)
{
  // Validate input parameters
  if (Size <= 0 || !Data) return;
  
  // Check if buffer expansion is needed
  if (m_DataOffset + Size > m_BufferSize)
  {
    // Calculate new buffer size using exponential growth strategy
    uint32_t newSize = std::max(m_BufferSize * 2, m_DataOffset + Size);
    
    // Ensure minimum buffer size for efficiency
    newSize = std::max(newSize, static_cast<uint32_t>(1024));
    
    uint8_t* newBuffer = nullptr;
    try {
      // Allocate new buffer
      newBuffer = new uint8_t[newSize];
      
      // Copy existing data if present
      if (m_Buffer && m_DataOffset > 0)
      {
        memcpy(newBuffer, m_Buffer, m_DataOffset);
      }
      
      // Replace old buffer with new expanded buffer
      if (m_Buffer)
      {
        delete[] m_Buffer;
      }
      
      m_Buffer = newBuffer;
      m_BufferSize = newSize;
    }
    catch (const std::bad_alloc& e) {
      // Handle memory allocation failure
      if (newBuffer) delete[] newBuffer;
      printf("Error: Memory allocation failed in xBufferAppend: %s\n", e.what());
      return;
    }
  }
  
  // Append new data to buffer
  memcpy(m_Buffer + m_DataOffset, Data, Size);
  m_DataOffset += Size;
}
