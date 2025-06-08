#pragma once
#include "tsCommon.h"
#include <string>

/**
 * @file tsTransportStream.h
 * @brief MPEG-2 Transport Stream packet parsing and analysis implementation
 * 
 * This file contains classes and structures for parsing MPEG-2 Transport Stream (TS) packets
 * according to ISO/IEC 13818-1 standard. The implementation supports full TS packet header
 * parsing, adaptation field processing, and PCR/OPCR time reference extraction.
 * 
 * MPEG-TS packet structure (188 bytes total):
 * ```
 *        3                   2                   1                   0  
 *      1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0  
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *   0 |                             Header (4 bytes)                  | 
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *   4 |                  Adaptation field + Payload (184 bytes)       | 
 *     |                                                               | 
 * 184 |                                                               | 
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 * ```
 *
 * MPEG-TS packet header structure (4 bytes):
 * ```
 *        3                   2                   1                   0  
 *      1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0  
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 *   0 |       SB      |E|S|T|           PID           |TSC|AFC|   CC  | 
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
 * ```
 *
 * Field descriptions:
 * - Sync byte                    (SB ) :  8 bits - Always 0x47, packet synchronization marker
 * - Transport error indicator    (E  ) :  1 bit  - Indicates transmission errors in the packet
 * - Payload unit start indicator (S  ) :  1 bit  - Indicates start of new PES packet or PSI section
 * - Transport priority           (T  ) :  1 bit  - Packet priority for same PID streams
 * - Packet Identifier            (PID) : 13 bits - Stream identifier (0x0000-0x1FFF)
 * - Transport scrambling control (TSC) :  2 bits - Scrambling mode (00=not scrambled, others=scrambled)
 * - Adaptation field control     (AFC) :  2 bits - Payload/adaptation field presence indicator
 * - Continuity counter           (CC ) :  4 bits - Incremental counter for packet loss detection
 *
 * @author Bartosz Berezowski
 * @date 2025
 * @version 1.0
 */


/**
 * @class xTS
 * @brief Constants and definitions for MPEG-2 Transport Stream processing
 * 
 * This class contains essential constants used throughout the MPEG-TS parsing implementation.
 * All values are defined according to ISO/IEC 13818-1 standard specifications.
 * 
 * Key constants include:
 * - Packet and header length definitions
 * - Clock frequency specifications for PCR/OPCR processing
 * - Multiplier constants for time reference calculations
 */
class xTS
{
public:
  /** @brief Total length of a Transport Stream packet in bytes (fixed at 188 bytes per ISO/IEC 13818-1) */
  static constexpr uint32_t TS_PacketLength  = 188;
  
  /** @brief Length of Transport Stream packet header in bytes (fixed at 4 bytes) */
  static constexpr uint32_t TS_HeaderLength  = 4;
  
  /** @brief Length of basic PES packet header in bytes (fixed at 6 bytes) */
  static constexpr uint32_t PES_HeaderLength = 6;

  /** @brief Base clock frequency for PCR timestamps in Hz (90 kHz) */
  static constexpr uint32_t BaseClockFrequency_Hz         =    90000;
  
  /** @brief Extended clock frequency for PCR timestamps in Hz (27 MHz) */
  static constexpr uint32_t ExtendedClockFrequency_Hz     = 27000000;
  
  /** @brief Base clock frequency for PCR timestamps in kHz (90 kHz) */
  static constexpr uint32_t BaseClockFrequency_kHz        =       90;
  
  /** @brief Extended clock frequency for PCR timestamps in kHz (27 MHz) */
  static constexpr uint32_t ExtendedClockFrequency_kHz    =    27000;
  
  /** @brief Multiplier from base to extended clock frequency (300x) */
  static constexpr uint32_t BaseToExtendedClockMultiplier =      300;
};

//=============================================================================================================================================================================

/**
 * @class xTS_PacketHeader
 * @brief Transport Stream packet header parser and container
 * 
 * This class implements parsing and storage for MPEG-2 Transport Stream packet headers.
 * It extracts all fields from the 4-byte TS header according to ISO/IEC 13818-1 specification.
 * 
 * The class handles:
 * - Sync byte validation (must be 0x47)
 * - Bit field extraction for all header components
 * - PID classification and validation
 * - Error detection and reporting
 * 
 * Header structure breakdown:
 * - Byte 0: Sync byte (0x47)
 * - Byte 1: TEI (1 bit) + PUSI (1 bit) + Priority (1 bit) + PID upper 5 bits
 * - Byte 2: PID lower 8 bits
 * - Byte 3: TSC (2 bits) + AFC (2 bits) + CC (4 bits)
 */
class xTS_PacketHeader
{
public:
  /**
   * @enum ePID
   * @brief Standard Program Identifier (PID) values for MPEG-TS
   * 
   * These are well-known PID values defined by the MPEG-2 standard and DVB specifications.
   * PIDs are 13-bit values (0x0000-0x1FFF) that identify different streams within a TS.
   */
  enum class ePID : uint16_t
  {
    PAT  = 0x0000,  ///< Program Association Table - contains list of programs
    CAT  = 0x0001,  ///< Conditional Access Table - scrambling information
    TSDT = 0x0002,  ///< Transport Stream Description Table
    IPMT = 0x0003,  ///< Reserved for IPMP (Intellectual Property Management and Protection)
    NIT  = 0x0010,  ///< Network Information Table (DVB specific)
    SDT  = 0x0011,  ///< Service Description Table (DVB specific)
    NuLL = 0x1FFF,  ///< Null packets - used for stuffing/padding
  };

protected:
  /** @brief Synchronization byte - always 0x47 for valid TS packets */
  uint8_t  m_SB;
  
  /** @brief Transport Error Indicator - signals transmission errors when set */
  uint8_t  m_E;
  
  /** @brief Payload Unit Start Indicator - marks beginning of PES packet or PSI section */
  uint8_t  m_S;
  
  /** @brief Transport Priority - indicates packet priority within same PID */
  uint8_t  m_T;  
  /** @brief Packet Identifier - 13-bit stream identifier (0x0000-0x1FFF) */
  uint16_t m_PID;
  
  /** @brief Transport Scrambling Control - indicates encryption status (00=clear, others=scrambled) */
  uint8_t  m_TSC;
  
  /** @brief Adaptation Field Control - indicates presence of adaptation field and/or payload */
  uint8_t  m_AFC;
  
  /** @brief Continuity Counter - 4-bit wraparound counter for packet loss detection */
  uint8_t  m_CC;

public:
  /**
   * @brief Reset all header fields to default values
   * 
   * Initializes all member variables to zero. Should be called before parsing
   * a new packet to ensure clean state.
   */
  void     Reset();
  
  /**
   * @brief Parse Transport Stream packet header from raw data
   * 
   * Extracts all header fields from the first 4 bytes of a TS packet.
   * Validates sync byte and performs basic sanity checks.
   * 
   * @param Input Pointer to raw packet data (must be at least 4 bytes)
   * @return Number of bytes parsed (4 on success, -1 on error)
   * 
   * @retval 4 Success - header parsed correctly
   * @retval -1 Error - invalid input or sync byte mismatch
   */
  int32_t  Parse(const uint8_t* Input);
  
  /**
   * @brief Print header information to console
   * 
   * Outputs all parsed header fields in human-readable format.
   * Useful for debugging and analysis.
   */
  void     Print() const;

public:
  // Accessor methods for header fields
  
  /** @brief Get synchronization byte value */
  uint8_t  getSyncByte() const { return m_SB; }
  
  /** @brief Get transport error indicator flag */
  uint8_t  getTransportErrorIndicator() const { return m_E; }
  
  /** @brief Get payload unit start indicator flag */
  uint8_t  getPayloadUnitStartIndicator() const { return m_S; }
  
  /** @brief Get transport priority flag */
  uint8_t  getTransportPriority() const { return m_T; }
  
  /** @brief Get packet identifier (PID) */
  uint16_t getPID() const { return m_PID; }
  
  /** @brief Get transport scrambling control field */
  uint8_t  getTransportScramblingControl() const { return m_TSC; }
  
  /** @brief Get adaptation field control field */
  uint8_t  getAdaptationFieldControl() const { return m_AFC; }
  
  /** @brief Get continuity counter value */
  uint8_t  getContinuityCounter() const { return m_CC; }

  // Utility methods for common operations
  
  /**
   * @brief Check if packet contains adaptation field
   * @return True if adaptation field is present
   * @note AFC bits: 00=Reserved, 01=Payload only, 10=AF only, 11=AF+Payload
   */
  bool hasAdaptationField() const { return (m_AFC & 0x2) != 0; }
  
  /**
   * @brief Check if packet contains payload data
   * @return True if payload is present
   * @note AFC bits: 00=Reserved, 01=Payload only, 10=AF only, 11=AF+Payload
   */
  bool hasPayload() const { return (m_AFC & 0x1) != 0; }
  
  /**
   * @brief Get readable PID classification
   * @return String description of PID type/purpose
   */
  std::string getPIDDescription() const;
};

//=============================================================================================================================================================================

/**
 * @class xTS_AdaptationField
 * @brief MPEG-2 Transport Stream adaptation field parser and container
 * 
 * This class implements parsing and management of TS packet adaptation fields according to
 * ISO/IEC 13818-1 specification. Adaptation fields provide additional control information
 * including timing references, discontinuity markers, and stream synchronization data.
 * 
 * The adaptation field structure allows for:
 * - Program Clock Reference (PCR) and Original PCR (OPCR) timestamps
 * - Stream discontinuity and random access indicators  
 * - Elementary stream priority signaling
 * - Splicing point indication for program switching
 * - Transport private data and extensions
 * - Stuffing bytes for packet alignment
 * 
 * Adaptation field control (AFC) values:
 * - 00: Reserved (not used)
 * - 01: No adaptation field, payload only
 * - 10: Adaptation field only, no payload
 * - 11: Adaptation field followed by payload
 * 
 * PCR provides system time references at 27MHz resolution with 33-bit base (90kHz)
 * and 9-bit extension (300x multiplier) for precise stream synchronization.
 */
class xTS_AdaptationField
{
protected:
  // === Basic adaptation field structure ===
  uint8_t  m_AFC;                  ///< Adaptation field control from TS header
  uint8_t  m_Len;                  ///< Adaptation field length in bytes

  // === Adaptation field flags ===
  uint8_t  m_DC;                   ///< Discontinuity indicator flag
  uint8_t  m_RA;                   ///< Random access indicator flag
  uint8_t  m_SP;                   ///< Elementary stream priority indicator flag
  uint8_t  m_PR;                   ///< PCR flag - indicates PCR presence
  uint8_t  m_OR;                   ///< OPCR flag - indicates OPCR presence
  uint8_t  m_SF;                   ///< Splicing point flag
  uint8_t  m_TP;                   ///< Transport private data flag
  uint8_t  m_EX;                   ///< Extension flag

  // === Time reference fields ===
  uint64_t m_PCR_base;             ///< Program Clock Reference base (33 bits at 90kHz)
  uint16_t m_PCR_extension;        ///< Program Clock Reference extension (9 bits)
  uint64_t m_OPCR_base;            ///< Original Program Clock Reference base (33 bits)
  uint16_t m_OPCR_extension;       ///< Original Program Clock Reference extension (9 bits)
    
  // === Variable length field information ===
  uint8_t m_PrivateDataLength;     ///< Length of transport private data field
  uint8_t m_ExtensionLength;       ///< Length of adaptation field extension
  uint8_t m_SplicingPointOffset;   ///< Splice countdown value
      // === Buffer management ===
  const uint8_t* m_Buffer;         ///< Pointer to adaptation field data for analysis

public:
  /**
   * @brief Reset all adaptation field values to defaults
   * 
   * Initializes all member variables to zero/invalid state.
   * Should be called before parsing a new adaptation field.
   */
  void Reset();
  
  /**
   * @brief Parse adaptation field from TS packet data
   * 
   * Extracts adaptation field information from the TS packet buffer.
   * Processes flags, time references, and variable-length fields based
   * on the adaptation field length and control flags.
   * 
   * @param PacketBuffer Pointer to complete TS packet (188 bytes)
   * @param AdaptationFieldControl AFC value from TS header
   * @return Number of bytes parsed, or negative value on error
   * 
   * @note Automatically handles PCR/OPCR extraction when flags are set
   * @note Calculates stuffing byte count for alignment purposes
   */
  int32_t Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl);
  
  /**
   * @brief Print adaptation field information to console
   * 
   * Outputs all parsed adaptation field data including flags, time references,
   * and calculated values in human-readable format.
   */
  void Print() const;
  
  /**
   * @brief Get adaptation field control value
   * @return AFC value indicating field presence and payload status
   */
  uint8_t getAdaptationFieldIndicator() const { return m_AFC; }
  
  /**
   * @brief Get adaptation field length
   * @return Length of adaptation field in bytes (excluding length byte itself)
   */
  uint8_t getAdaptationFieldLength() const { return m_Len; }
  
  /**
   * @brief Set adaptation field control value
   * @param afc New AFC value to set
   */
  void setAdaptationFieldControl(uint8_t afc) { m_AFC = afc; }

  // === Flag accessors ===
  
  /** @brief Get discontinuity indicator flag */
  uint8_t getDiscontinuityIndicator() const { return m_DC; }
  
  /** @brief Get random access indicator flag */
  uint8_t getRandomAccessIndicator() const { return m_RA; }
  
  /** @brief Get elementary stream priority indicator flag */
  uint8_t getESPriorityIndicator() const { return m_SP; }
  
  /** @brief Get PCR flag (indicates PCR presence) */
  uint8_t getPCRFlag() const { return m_PR; }
  
  /** @brief Get OPCR flag (indicates OPCR presence) */
  uint8_t getOPCRFlag() const { return m_OR; }
  
  /** @brief Get splicing point flag */
  uint8_t getSplicingPointFlag() const { return m_SF; }
  
  /** @brief Get transport private data flag */
  uint8_t getTransportPrivateDataFlag() const { return m_TP; }
  
  /** @brief Get extension flag */
  uint8_t getExtensionFlag() const { return m_EX; }

  // === Time reference accessors ===
  
  /**
   * @brief Get PCR base value (33-bit, 90kHz clock)
   * @return PCR base timestamp value
   */
  uint64_t getPCRBase() const { return m_PCR_base; }
  
  /**
   * @brief Get PCR extension value (9-bit, 27MHz clock)
   * @return PCR extension timestamp value
   */
  uint16_t getPCRExtension() const { return m_PCR_extension; }
  
  /**
   * @brief Get complete PCR value in 27MHz units
   * @return Full PCR timestamp (base * 300 + extension)
   * @note Combines 33-bit base and 9-bit extension into single 27MHz timestamp
   */
  uint64_t getPCR() const { return (m_PCR_base * 300 + m_PCR_extension); }
  
  /**
   * @brief Get OPCR base value (33-bit, 90kHz clock)
   * @return OPCR base timestamp value
   */
  uint64_t getOPCRBase() const { return m_OPCR_base; }
  
  /**
   * @brief Get OPCR extension value (9-bit, 27MHz clock)
   * @return OPCR extension timestamp value
   */
  uint16_t getOPCRExtension() const { return m_OPCR_extension; }
  
  /**
   * @brief Get complete OPCR value in 27MHz units
   * @return Full OPCR timestamp (base * 300 + extension)
   * @note Combines 33-bit base and 9-bit extension into single 27MHz timestamp
   */
  uint64_t getOPCR() const { return (m_OPCR_base * 300 + m_OPCR_extension); }
  
  /**
   * @brief Calculate number of stuffing bytes in adaptation field
   * @return Number of stuffing bytes used for packet alignment
   * 
   * Stuffing bytes are used to pad the adaptation field to reach the required
   * packet length when there isn't enough data to fill the entire TS packet.
   */
  int32_t getStuffingBytes() const;
};