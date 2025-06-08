/**
 * @file pesParse.h
 * @brief PES (Packetized Elementary Stream) packet parsing and assembly for MPEG-2 Transport Stream
 * 
 * This header file defines classes for parsing and assembling PES packets from MPEG-2 Transport Stream
 * packets according to ISO/IEC 13818-1 standard. It provides functionality for:
 * - Parsing PES packet headers with timing information (PTS/DTS)
 * - Assembling complete PES packets from multiple TS packets
 * - Handling continuity and packet validation
 * - Managing stuffing bytes and adaptation field data
 * 
 * The implementation supports audio and video elementary streams with proper timestamp extraction
 * and packet boundary detection using the Payload Unit Start Indicator (PUSI) flag.
 * 
 * @author Bartosz Berezowski
 * @date 2025
 * @version 1.0
 */

// filepath: /home/bartosz/Documents/PROJECT/MPEG-TS-Transport-Stream-Parser/include/pesParse.h
#pragma once
#include "tsCommon.h"
#include "tsTransportStream.h"

/**
 * @class xPES_PacketHeader
 * @brief Parser for PES (Packetized Elementary Stream) packet headers
 * 
 * This class implements parsing of PES packet headers according to ISO/IEC 13818-1 specification.
 * It extracts mandatory and optional header fields including:
 * - Packet start code prefix and stream identification
 * - Packet length and header extension information
 * - Presentation Time Stamp (PTS) and Decoding Time Stamp (DTS)
 * - Stream control flags and data alignment indicators
 * 
 * The parser handles both basic and extended PES headers, automatically detecting
 * the presence of optional fields based on header flags.
 */
class xPES_PacketHeader
{
public:
  /**
   * @enum eStreamId
   * @brief Standard PES stream identifiers according to ISO/IEC 13818-1
   * 
   * Defines the standard stream ID values used in PES packet headers to identify
   * the type of elementary stream data contained in the packet.
   */
  enum eStreamId : uint8_t
  {
    eStreamId_program_stream_map      = 0xBC,  ///< Program stream map
    eStreamId_padding_stream          = 0xBE,  ///< Padding stream (used for alignment)
    eStreamId_private_stream_2        = 0xBF,  ///< Private stream 2
    eStreamId_ECM                     = 0xF0,  ///< Entitlement Control Message
    eStreamId_EMM                     = 0xF1,  ///< Entitlement Management Message
    eStreamId_program_stream_directory = 0xFF, ///< Program stream directory
    eStreamId_DSMCC_stream            = 0xF2,  ///< DSM-CC stream
    eStreamId_ITUT_H222_1_type_E      = 0xF8   ///< ITU-T H.222.1 type E stream
  };

protected:
  // === Mandatory PES packet header fields ===
  uint32_t m_PacketStartCodePrefix;    ///< Packet start code prefix (0x000001)
  uint8_t  m_StreamId;                 ///< Stream identifier (audio/video/other)
  uint16_t m_PacketLength;             ///< PES packet length (0 = unbounded)
  
  // === Optional PES packet header fields ===
  uint8_t  m_HeaderMarkerBits;         ///< Header marker bits ('10' for valid header)
  uint8_t  m_PES_scrambling_control;   ///< Scrambling control (2 bits)
  uint8_t  m_PES_priority;             ///< PES priority flag (1 bit)
  uint8_t  m_data_alignment_indicator; ///< Data alignment indicator (1 bit)
  uint8_t  m_copyright;                ///< Copyright flag (1 bit)
  uint8_t  m_original_or_copy;         ///< Original/copy indicator (1 bit)
  uint8_t  m_PTS_DTS_flags;            ///< PTS/DTS presence flags (2 bits)
  uint8_t  m_ESCR_flag;                ///< ESCR flag (1 bit)
  uint8_t  m_ES_rate_flag;             ///< Elementary stream rate flag (1 bit)
  uint8_t  m_DSM_trick_mode_flag;      ///< DSM trick mode flag (1 bit)
  uint8_t  m_additional_copy_info_flag;///< Additional copy info flag (1 bit)
  uint8_t  m_PES_CRC_flag;             ///< PES CRC flag (1 bit)
  uint8_t  m_PES_extension_flag;       ///< PES extension flag (1 bit)
  uint8_t  m_PES_header_data_length;   ///< Header data length in bytes
  
  // === Timestamp fields ===
  uint64_t m_PTS;                      ///< Presentation Time Stamp (33 bits)
  uint64_t m_DTS;                      ///< Decoding Time Stamp (33 bits)
  
  // === Helper variables ===
  bool m_hasHeaderExtension;           ///< Flag indicating extended header presence
  int32_t m_headerLength;              ///< Total header length (6 + extended)

public:
  /**
   * @brief Reset all PES header fields to default values
   * 
   * Initializes all header fields to their default/invalid state,
   * preparing the object for parsing a new PES packet header.
   */
  void      Reset();
  
  /**
   * @brief Parse PES packet header from input buffer
   * 
   * Parses the PES packet header from the provided input buffer, extracting
   * all mandatory and optional fields according to the header flags.
   * 
   * @param Input Pointer to the input buffer containing PES header data
   * @return Length of the parsed header in bytes, or negative value on error
   * 
   * @note The input buffer must contain at least 6 bytes for the basic header
   * @note Extended header parsing is performed automatically based on flags
   */
  int32_t   Parse(const uint8_t* Input);
  
  /**
   * @brief Print PES header information to console
   * 
   * Outputs formatted PES header information including stream ID, packet length,
   * timestamp values (if present), and other relevant header fields.
   */
  void      Print() const;

public:
  // === Basic header field accessors ===
  
  /**
   * @brief Get packet start code prefix
   * @return Packet start code prefix (should be 0x000001)
   */
  uint32_t  getPacketStartCodePrefix() const { return m_PacketStartCodePrefix; }
  
  /**
   * @brief Get stream identifier
   * @return Stream ID indicating the type of elementary stream
   */
  uint8_t   getStreamId() const { return m_StreamId; }
  
  /**
   * @brief Get PES packet length
   * @return Packet length in bytes (0 indicates unbounded packet)
   */
  uint16_t  getPacketLength() const { return m_PacketLength; }
  
  /**
   * @brief Get total header length
   * @return Total header length including extensions in bytes
   */
  int32_t   getHeaderLength() const { return m_headerLength; }
  
  // === Optional header field accessors ===
  
  /**
   * @brief Check if packet has optional header fields
   * @return True if optional header fields are present
   */
  bool      hasOptionalHeader() const { return m_hasHeaderExtension; }
  
  /**
   * @brief Get PTS/DTS flags
   * @return Flags indicating presence of PTS/DTS timestamps
   */
  uint8_t   getPTSDTSFlags() const { return m_PTS_DTS_flags; }
  
  /**
   * @brief Check if PTS is present
   * @return True if Presentation Time Stamp is available
   */
  bool      hasPTS() const { return (m_PTS_DTS_flags & 0x2) == 0x2; }
  
  /**
   * @brief Check if DTS is present
   * @return True if Decoding Time Stamp is available
   */
  bool      hasDTS() const { return (m_PTS_DTS_flags & 0x3) == 0x3; }
  
  /**
   * @brief Get Presentation Time Stamp
   * @return PTS value in 90kHz clock units (33-bit value)
   */
  uint64_t  getPTS() const { return m_PTS; }
  
  /**
   * @brief Get Decoding Time Stamp
   * @return DTS value in 90kHz clock units (33-bit value)
   */
  uint64_t  getDTS() const { return m_DTS; }
  
  /**
   * @brief Get PES header data length
   * @return Length of optional header data in bytes
   */
  uint8_t   getPESHeaderDataLength() const { return m_PES_header_data_length; }
};

/**
 * @class xPES_Assembler
 * @brief Assembles complete PES packets from multiple Transport Stream packets
 * 
 * This class manages the assembly of PES packets that span multiple TS packets.
 * It handles:
 * - Packet continuity checking using continuity counters
 * - Buffer management for accumulating PES data
 * - Detection of packet boundaries using PUSI (Payload Unit Start Indicator)
 * - Stuffing byte accounting and validation
 * - Complete packet assembly and verification
 * 
 * The assembler maintains state between TS packets and provides status feedback
 * through the eResult enumeration to indicate assembly progress.
 */
class xPES_Assembler
{
public:
  /**
   * @enum eResult
   * @brief Assembly operation result codes
   * 
   * Indicates the result of packet absorption operations and current assembly state.
   */
  enum class eResult : int32_t
  {
    UnexpectedPID       = 1,  ///< Packet PID doesn't match expected assembler PID
    StreamPackedLost    ,     ///< Continuity counter indicates lost packet(s)
    AssemblingStarted   ,     ///< New PES packet assembly started (PUSI=1)
    AssemblingContinue  ,     ///< Continuing assembly of current PES packet
    AssemblingFinished  ,     ///< PES packet assembly completed successfully
  };

public:
  xPES_PacketHeader m_PESH;    ///< Parsed PES header for current packet
  
protected:
  // === Configuration ===
  int32_t m_PID;               ///< Target PID for packet assembly
  
  // === Buffer management ===
  uint8_t* m_Buffer;           ///< Assembly buffer for PES packet data
  uint32_t m_BufferSize;       ///< Total buffer size in bytes
  uint32_t m_DataOffset;       ///< Current data offset in buffer
  
  // === Assembly state ===
  uint8_t   m_LastContinuityCounter; ///< Last processed continuity counter
  bool      m_Started;               ///< Flag indicating assembly in progress
  
  // === Statistics ===
  uint32_t  m_TotalStuffingBytes;    ///< Total stuffing bytes encountered

public:
  /**
   * @brief Default constructor
   * 
   * Initializes the PES assembler with default values and allocates
   * the internal buffer for packet assembly.
   */
  xPES_Assembler();
  
  /**
   * @brief Destructor
   * 
   * Cleans up allocated resources, particularly the assembly buffer.
   */
  ~xPES_Assembler();

  /**
   * @brief Initialize assembler for specific PID
   * 
   * Configures the assembler to process packets for the specified PID
   * and resets all internal state variables.
   * 
   * @param PID Transport Stream Packet Identifier to monitor
   */
  void Init(int32_t PID);
  
  /**
   * @brief Process a Transport Stream packet
   * 
   * Absorbs a TS packet into the PES assembly process. Handles packet
   * continuity checking, payload extraction, and assembly state management.
   * 
   * @param TransportStreamPacket Pointer to complete 188-byte TS packet
   * @param PacketHeader Parsed TS packet header information
   * @param AdaptationField Parsed adaptation field (may be nullptr)
   * @return Assembly result indicating current state or error condition
   * 
   * @note Packets with unexpected PID are rejected
   * @note Continuity counter gaps trigger StreamPackedLost result
   * @note PUSI flag detection triggers new packet assembly start
   */
  eResult AbsorbPacket(const uint8_t* TransportStreamPacket, 
                      const xTS_PacketHeader* PacketHeader, 
                      const xTS_AdaptationField* AdaptationField);

  // === Information access methods ===
  
  /**
   * @brief Print current PES header information
   * 
   * Outputs the parsed PES header details to console for debugging/analysis.
   */
  void PrintPESH() const { m_PESH.Print(); }
  
  /**
   * @brief Get assembled packet buffer
   * @return Pointer to internal buffer containing assembled PES packet
   */
  uint8_t* getPacket() const { return m_Buffer; }
  
  /**
   * @brief Get number of bytes in assembled packet
   * @return Current number of valid bytes in assembly buffer
   */
  int32_t getNumPacketBytes() const { return m_DataOffset; }
  
  /**
   * @brief Get total stuffing bytes encountered
   * @return Cumulative count of stuffing bytes processed during assembly
   */
  uint32_t getTotalStuffingBytes() const { return m_TotalStuffingBytes; }

protected:
  /**
   * @brief Reset internal assembly buffer
   * 
   * Clears the assembly buffer and resets data offset for new packet assembly.
   */
  void xBufferReset();
  
  /**
   * @brief Append data to assembly buffer
   * 
   * Adds the specified data to the internal assembly buffer at the current offset.
   * 
   * @param Data Pointer to data to append
   * @param Size Number of bytes to append
   */
  void xBufferAppend(const uint8_t* Data, int32_t Size);
};
