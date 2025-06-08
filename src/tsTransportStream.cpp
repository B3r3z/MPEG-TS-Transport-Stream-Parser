/**
 * @file tsTransportStream.cpp
 * @brief Implementation of MPEG-2 Transport Stream packet header and adaptation field parsing classes
 * 
 * This file contains the implementation of classes for parsing MPEG-2 Transport Stream packets
 * according to the ISO/IEC 13818-1 standard. It provides functionality for extracting and
 * validating packet headers, adaptation fields, and associated timing information.
 * 
 * @author Bartosz Berezowski
 * @date 2025
 * @version 1.0
 */

#include "../include/tsTransportStream.h"

//=============================================================================================================================================================================
// xTS_PacketHeader Implementation
//=============================================================================================================================================================================

/**
 * @brief Resets all Transport Stream packet header fields to their default values
 * 
 * Initializes all header fields to zero, preparing the object for parsing a new packet.
 * This method should be called before parsing each new TS packet to ensure clean state.
 * 
 * @note This method sets all fields to 0, which represents invalid/uninitialized state
 *       for most fields according to MPEG-2 TS specification
 */
void xTS_PacketHeader::Reset()
{
  m_SB = 0;   // Sync byte - should be 0x47 for valid packets
  m_E = 0;    // Transport Error Indicator
  m_S = 0;    // Payload Unit Start Indicator  
  m_T = 0;    // Transport Priority
  m_PID = 0;  // Packet Identifier
  m_TSC = 0;  // Transport Scrambling Control
  m_AFC = 0;  // Adaptation Field Control
  m_CC = 0;   // Continuity Counter
}

/**
 * @brief Parses the 4-byte MPEG-2 Transport Stream packet header
 * 
 * Extracts and validates all fields from the standard 32-bit TS packet header according
 * to ISO/IEC 13818-1 specification. The header structure is:
 * - Byte 0: sync_byte (0x47)
 * - Byte 1: transport_error_indicator(1) + payload_unit_start_indicator(1) + 
 *           transport_priority(1) + PID[12:8](5)
 * - Byte 2: PID[7:0](8)  
 * - Byte 3: transport_scrambling_control(2) + adaptation_field_control(2) + 
 *           continuity_counter(4)
 * 
 * @param Input Pointer to buffer containing at least 4 bytes of TS packet data
 * @return Number of parsed bytes (4) on success, NOT_VALID (-1) on failure
 * 
 * @retval 4 Successfully parsed header, all fields extracted
 * @retval NOT_VALID Invalid input pointer or sync byte mismatch
 * 
 * @note The sync byte must be 0x47 for a valid MPEG-2 TS packet
 * @note PID values have special meanings: 0x0000=PAT, 0x0001=CAT, 0x1FFF=NULL packets
 */
int32_t xTS_PacketHeader::Parse(const uint8_t* Input)
{
  // Validate input pointer
  if (!Input) return NOT_VALID;

  // Extract sync byte - must be 0x47 for valid MPEG-2 TS packet
  m_SB = Input[0];
  if (m_SB != 0x47) return NOT_VALID; 

  // Parse byte 1: Error indicator, PUSI, Priority, and upper 5 bits of PID
  m_E = (Input[1] & 0x80) >> 7;  // Transport Error Indicator (bit 7)
  m_S = (Input[1] & 0x40) >> 6;  // Payload Unit Start Indicator (bit 6)
  m_T = (Input[1] & 0x20) >> 5;  // Transport Priority (bit 5)
  
  // Parse 13-bit Packet Identifier (PID) spanning bytes 1-2
  m_PID = ((Input[1] & 0x1F) << 8) | Input[2];  // Bits 4-0 of byte 1 + byte 2
  
  // Parse byte 3: Scrambling control, adaptation field control, continuity counter
  m_TSC = (Input[3] & 0xC0) >> 6;  // Transport Scrambling Control (bits 7-6)
  m_AFC = (Input[3] & 0x30) >> 4;  // Adaptation Field Control (bits 5-4)
  m_CC = Input[3] & 0x0F;          // Continuity Counter (bits 3-0)

  return xTS::TS_HeaderLength;  // Return number of bytes consumed (4)
}

/**
 * @brief Prints all Transport Stream packet header fields in formatted output
 * 
 * Outputs header information in a standardized format for debugging and analysis.
 * The format includes all critical fields with appropriate spacing and formatting
 * for easy reading and parsing by external tools.
 * 
 * Output format: "SB=XX E=X S=X P=X PID=XXXX TSC=X AF=X CC=XX"
 * Where:
 * - SB: Sync Byte (hexadecimal)
 * - E: Transport Error Indicator (0/1)
 * - S: Payload Unit Start Indicator (0/1) 
 * - P: Transport Priority (0/1)
 * - PID: Packet Identifier (decimal, 0-8191)
 * - TSC: Transport Scrambling Control (0-3)
 * - AF: Adaptation Field Control (0-3)
 * - CC: Continuity Counter (0-15)
 * 
 * @note This method uses printf for output to maintain compatibility with C-style formatting
 * @note The output does not include a newline character - caller must add if needed
 */
void xTS_PacketHeader::Print() const
{
  printf("SB=%02X E=%d S=%d P=%d PID=%4d TSC=%d AF=%d CC=%2d",
         m_SB, m_E, m_S, m_T, m_PID, m_TSC, m_AFC, m_CC);
}

//=============================================================================================================================================================================
// xTS_AdaptationField Implementation
//=============================================================================================================================================================================

/**
 * @brief Resets all Transport Stream adaptation field members to default values
 * 
 * Initializes all adaptation field components to zero/false state, preparing the object
 * for parsing a new adaptation field. This includes control flags, timing references,
 * and optional field lengths.
 * 
 * The adaptation field is an optional component of TS packets that can contain:
 * - Discontinuity indicators
 * - Program Clock References (PCR/OPCR)  
 * - Splicing point information
 * - Private data
 * - Extensions
 * 
 * @note After reset, all boolean flags are false and all numeric values are zero
 * @note This method should be called before parsing each new adaptation field
 */
void xTS_AdaptationField::Reset()
{
  // Basic adaptation field control
  m_AFC = 0;    // Adaptation Field Control value from TS header
  m_Len = 0;    // Adaptation field length
  
  // Control flags - all initially disabled
  m_DC = 0;     // Discontinuity Counter indicator
  m_RA = 0;     // Random Access indicator  
  m_SP = 0;     // Elementary Stream Priority indicator
  m_PR = 0;     // PCR (Program Clock Reference) flag
  m_OR = 0;     // OPCR (Original Program Clock Reference) flag
  m_SF = 0;     // Splicing Point flag
  m_TP = 0;     // Transport Private data flag
  m_EX = 0;     // Extension flag
  
  // Timing references - Program Clock Reference components
  m_PCR_base = 0;       // 33-bit PCR base value (90 kHz clock)
  m_PCR_extension = 0;  // 9-bit PCR extension (27 MHz clock)
  
  // Original Program Clock Reference components  
  m_OPCR_base = 0;      // 33-bit OPCR base value (90 kHz clock)
  m_OPCR_extension = 0; // 9-bit OPCR extension (27 MHz clock)
}

/**
 * @brief Parses MPEG-2 Transport Stream adaptation field according to ISO/IEC 13818-1
 * 
 * The adaptation field provides additional functionality beyond basic packet transport,
 * including timing synchronization, stream discontinuity signaling, and private data.
 * Structure varies based on Adaptation Field Control (AFC) value:
 * - AFC = 0: Reserved (invalid)
 * - AFC = 1: Payload only, no adaptation field
 * - AFC = 2: Adaptation field only, no payload  
 * - AFC = 3: Adaptation field followed by payload
 * 
 * Adaptation field format:
 * - adaptation_field_length (8 bits)
 * - flags byte (8 bits) - when length > 0
 * - Optional fields based on flags: PCR(48), OPCR(48), splice_countdown(8), 
 *   private_data(variable), extension(variable)
 * - Stuffing bytes to fill remaining space
 * 
 * @param PacketBuffer Pointer to TS packet buffer positioned after 4-byte header
 * @param AdaptationFieldControl AFC value (0-3) from TS packet header
 * @return Length of parsed adaptation field + 1 for length byte, or NOT_VALID on error
 * 
 * @retval >0 Successfully parsed adaptation field, returns (length + 1) bytes consumed
 * @retval 0 No adaptation field present (AFC = 1)
 * @retval NOT_VALID Invalid parameters or malformed adaptation field
 * 
 * @note PCR provides 42-bit timestamp: 33-bit base (90kHz) + 9-bit extension (27MHz)
 * @note Maximum adaptation field length is 184 bytes (AFC=2) or 183 bytes (AFC=3)
 */
int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl)
{
  // Validate input parameters
  if(PacketBuffer == nullptr) return NOT_VALID;
  
  // Store buffer pointer and control value for later analysis
  m_Buffer = PacketBuffer;
  m_AFC = AdaptationFieldControl;
  
  // Check if adaptation field is present based on AFC value
  if(m_AFC != 2 && m_AFC != 3){
    // AFC = 0 (reserved) or AFC = 1 (payload only) - no adaptation field
    m_Len = 0;
    return 0;
  }
  
  // Extract adaptation field length from first byte
  m_Len = PacketBuffer[0];
  
  // Validate adaptation field length constraints
  // AFC = 2: max 184 bytes (no payload), AFC = 3: max 183 bytes (with payload)
  if((m_Len > 183 && m_AFC == 3) || (m_Len > 184 && m_AFC == 2)) {
    return NOT_VALID; 
  }
  
  // Initialize optional field lengths to zero
  m_PrivateDataLength = 0;
  m_ExtensionLength = 0;
  m_SplicingPointOffset = 0;
  
  // Parse flags and optional fields only if adaptation field has content
  if (m_Len >= 1) {
      // Parse control flags from second byte (index 1)
      m_DC = (PacketBuffer[1] & 0x80) >> 7;  // Discontinuity indicator (bit 7)
      m_RA = (PacketBuffer[1] & 0x40) >> 6;  // Random access indicator (bit 6)
      m_SP = (PacketBuffer[1] & 0x20) >> 5;  // Elementary stream priority (bit 5)
      m_PR = (PacketBuffer[1] & 0x10) >> 4;  // PCR flag (bit 4)
      m_OR = (PacketBuffer[1] & 0x08) >> 3;  // OPCR flag (bit 3)
      m_SF = (PacketBuffer[1] & 0x04) >> 2;  // Splicing point flag (bit 2)
      m_TP = (PacketBuffer[1] & 0x02) >> 1;  // Transport private data flag (bit 1)
      m_EX = (PacketBuffer[1] & 0x01);       // Extension flag (bit 0)
      
      int currentOffset = 2; // Start parsing after flags byte
      
      // Parse Program Clock Reference (PCR) - 48 bits total
      if (m_PR && m_Len >= currentOffset + 6) {
          // PCR_base: 33 bits = bytes 2-5 + upper 7 bits of byte 6
          // Format: [byte2][byte3][byte4][byte5][byte6_bits7-1][reserved][byte6_bit0 + byte7]
          m_PCR_base = ((uint64_t)PacketBuffer[currentOffset] << 25) |
                       ((uint64_t)PacketBuffer[currentOffset + 1] << 17) |
                       ((uint64_t)PacketBuffer[currentOffset + 2] << 9) |
                       ((uint64_t)PacketBuffer[currentOffset + 3] << 1) |
                       ((uint64_t)PacketBuffer[currentOffset + 4] >> 7);
          
          // PCR_extension: 9 bits = bit 0 of byte 6 + all 8 bits of byte 7  
          m_PCR_extension = ((uint16_t)(PacketBuffer[currentOffset + 4] & 0x01) << 8) |
                           (uint16_t)PacketBuffer[currentOffset + 5];
          
          currentOffset += 6; // Advance past PCR data
      }
      
      // Parse Original Program Clock Reference (OPCR) - same format as PCR
      if (m_OR && m_Len >= currentOffset + 6) {
          // OPCR_base: 33 bits in same format as PCR_base
          m_OPCR_base = ((uint64_t)PacketBuffer[currentOffset] << 25) |
                        ((uint64_t)PacketBuffer[currentOffset + 1] << 17) |
                        ((uint64_t)PacketBuffer[currentOffset + 2] << 9) |
                        ((uint64_t)PacketBuffer[currentOffset + 3] << 1) |
                        ((uint64_t)PacketBuffer[currentOffset + 4] >> 7);
          
          // OPCR_extension: 9 bits in same format as PCR_extension
          m_OPCR_extension = ((uint16_t)(PacketBuffer[currentOffset + 4] & 0x01) << 8) |
                            (uint16_t)PacketBuffer[currentOffset + 5];
          
          currentOffset += 6; // Advance past OPCR data
      }
      
      // Parse splice countdown - 8-bit signed value indicating splice point proximity
      if (m_SF && m_Len >= currentOffset + 1) {          m_SplicingPointOffset = PacketBuffer[currentOffset];
          currentOffset += 1; // Advance past splice countdown byte
      }
      
      // Parse transport private data - broadcaster-specific information
      if (m_TP && m_Len >= currentOffset + 1) {
          // First byte indicates length of private data that follows
          m_PrivateDataLength = PacketBuffer[currentOffset];
          currentOffset += 1; // Move past length byte
          
          // Skip over private data content if present and buffer has sufficient space
          if (m_Len >= currentOffset + m_PrivateDataLength) {
              currentOffset += m_PrivateDataLength;
          }
      }
      
      // Parse adaptation field extension - additional optional fields
      if (m_EX && m_Len >= currentOffset + 1) {
          // First byte indicates length of extension data that follows
          m_ExtensionLength = PacketBuffer[currentOffset];
          currentOffset += 1 + m_ExtensionLength; // Skip length byte + extension data
      }
  }
  
  // Return total bytes consumed: adaptation field length + 1 byte for length field itself
  return m_Len + 1;
}


/**
 * @brief Prints adaptation field information in formatted output for debugging
 * 
 * Outputs key adaptation field parameters in standardized format for analysis.
 * Includes length and all control flags to provide complete overview of
 * adaptation field content and capabilities.
 * 
 * Output format: "AF: L=XXX DC=X RA=X SP=X PR=X OR=X SF=X TP=X EX=X"
 * Where:
 * - L: Adaptation field Length (0-184)
 * - DC: Discontinuity Counter indicator (0/1)
 * - RA: Random Access indicator (0/1)
 * - SP: Elementary Stream Priority indicator (0/1)
 * - PR: PCR flag - Program Clock Reference present (0/1)
 * - OR: OPCR flag - Original Program Clock Reference present (0/1)
 * - SF: Splicing point Flag (0/1)
 * - TP: Transport Private data flag (0/1)
 * - EX: Extension flag (0/1)
 * 
 * @note Output does not include newline - caller must add if needed
 * @note Actual PCR/OPCR values are not printed, only presence flags
 */
void xTS_AdaptationField::Print() const
{
  printf("AF: L=%3d DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
         m_Len, m_DC, m_RA, m_SP, m_PR, m_OR, m_SF, m_TP, m_EX);
}

/**
 * @brief Calculates the number of stuffing bytes in the adaptation field
 * 
 * Stuffing bytes are padding bytes (usually 0xFF) added to fill unused space
 * in the adaptation field. They serve to maintain constant packet length when
 * the actual adaptation field content is shorter than required space.
 * 
 * Calculation process:
 * 1. Start with total adaptation field length
 * 2. Subtract 1 byte for mandatory flags field (when length > 0)
 * 3. Subtract lengths of all present optional fields:
 *    - PCR: 6 bytes (48 bits)
 *    - OPCR: 6 bytes (48 bits)  
 *    - Splice countdown: 1 byte
 *    - Transport private data: 1 + N bytes (length + data)
 *    - Extension: 1 + N bytes (length + data)
 * 4. Remaining bytes are stuffing
 * 
 * @return Number of stuffing bytes (0 or positive integer)
 * @retval 0 No adaptation field present, or no stuffing bytes needed
 * @retval >0 Number of stuffing bytes used for padding
 * 
 * @note Stuffing bytes do not affect PES packet length calculations
 * @note Method requires access to original buffer for accurate private data lengths
 */
int32_t xTS_AdaptationField::getStuffingBytes() const
{
    // Return zero if no adaptation field present or zero length
    if (m_AFC != 2 && m_AFC != 3 || m_Len == 0) {
        return 0;
    }
    
    // Start with mandatory flags byte (always present when length > 0)
    int32_t usedBytes = 1;
    
    // Add PCR field size if present (6 bytes total)
    if (m_PR) {
        usedBytes += 6;
    }
    
    // Add OPCR field size if present (6 bytes total)
    if (m_OR) {
        usedBytes += 6;
    }
    
    // Add splice countdown field size if present (1 byte)
    if (m_SF) {
        usedBytes += 1;
    }
    
    // Add transport private data size if present (1 byte length + N bytes data)
    if (m_TP && m_Buffer) {
        // Calculate offset to private data length byte
        int privateDataOffset = 2; // Start after flags byte
        if (m_PR) privateDataOffset += 6;  // Skip PCR
        if (m_OR) privateDataOffset += 6;  // Skip OPCR  
        if (m_SF) privateDataOffset += 1;  // Skip splice countdown
        
        // Verify buffer access and read private data length
        if (m_Len >= privateDataOffset) {
            uint8_t privateDataLength = m_Buffer[privateDataOffset];
            usedBytes += 1 + privateDataLength; // Length byte + actual data
        }
    }
    
    // Add extension data size if present (1 byte length + N bytes data)
    if (m_EX && m_Buffer) {
        // Calculate offset to extension length byte
        int extensionOffset = 2; // Start after flags byte
        if (m_PR) extensionOffset += 6;                    // Skip PCR
        if (m_OR) extensionOffset += 6;                    // Skip OPCR
        if (m_SF) extensionOffset += 1;                    // Skip splice countdown
        if (m_TP) extensionOffset += 1 + m_PrivateDataLength; // Skip private data
        
        // Verify buffer access and read extension length
        if (m_Len >= extensionOffset) {
            uint8_t extensionLength = m_Buffer[extensionOffset];
            usedBytes += 1 + extensionLength; // Length byte + actual data
        }
    }
    
    // Calculate stuffing bytes: total length minus used bytes
    int32_t stuffingBytes = m_Len - usedBytes;
    return (stuffingBytes > 0) ? stuffingBytes : 0;
}