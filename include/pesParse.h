#pragma once
#include "tsCommon.h"
#include "tsTransportStream.h"

class xPES_PacketHeader
{
public:
  enum eStreamId : uint8_t
  {
    eStreamId_program_stream_map      = 0xBC,
    eStreamId_padding_stream          = 0xBE,
    eStreamId_private_stream_2        = 0xBF,
    eStreamId_ECM                     = 0xF0,
    eStreamId_EMM                     = 0xF1,
    eStreamId_program_stream_directory = 0xFF,
    eStreamId_DSMCC_stream            = 0xF2,
    eStreamId_ITUT_H222_1_type_E      = 0xF8
  };

protected:
  //PES packet header - obowiązkowe pola
  uint32_t m_PacketStartCodePrefix;
  uint8_t  m_StreamId;
  uint16_t m_PacketLength;
  
  //PES packet header - opcjonalne pola
  uint8_t  m_HeaderMarkerBits;    // '10' dla poprawnego nagłówka (2 bity)
  uint8_t  m_PES_scrambling_control; // Scrambling control (2 bity)
  uint8_t  m_PES_priority;        // Priorytet (1 bit)
  uint8_t  m_data_alignment_indicator; // Wskaźnik wyrównania danych (1 bit)
  uint8_t  m_copyright;           // Copyright (1 bit)
  uint8_t  m_original_or_copy;    // Oryginał/kopia (1 bit)
  uint8_t  m_PTS_DTS_flags;       // Flagi PTS/DTS (2 bity)
  uint8_t  m_ESCR_flag;           // Flaga ESCR (1 bit)
  uint8_t  m_ES_rate_flag;        // Flaga ES rate (1 bit)
  uint8_t  m_DSM_trick_mode_flag; // Flaga DSM trick mode (1 bit)
  uint8_t  m_additional_copy_info_flag; // Flaga dodatkowych informacji o kopii (1 bit)
  uint8_t  m_PES_CRC_flag;        // Flaga CRC (1 bit)
  uint8_t  m_PES_extension_flag;  // Flaga rozszerzenia (1 bit)
  uint8_t  m_PES_header_data_length; // Długość danych nagłówka (8 bitów)
  
  // Timestampy PTS i DTS (33 bity każdy)
  uint64_t m_PTS;                 // Presentation Time Stamp
  uint64_t m_DTS;                 // Decoding Time Stamp
  
  // Dodatkowe zmienne pomocnicze
  bool m_hasHeaderExtension;      // Czy pakiet ma rozszerzony nagłówek
  int32_t m_headerLength;         // Całkowita długość nagłówka (6 + rozszerzony)

public:
  void      Reset();
  int32_t   Parse(const uint8_t* Input);
  void      Print() const;

public:
  //PES packet header - podstawowe pola
  uint32_t  getPacketStartCodePrefix() const { return m_PacketStartCodePrefix; }
  uint8_t   getStreamId() const { return m_StreamId; }
  uint16_t  getPacketLength() const { return m_PacketLength; }
  int32_t   getHeaderLength() const { return m_headerLength; }
  
  //PES packet header - opcjonalne pola
  bool      hasOptionalHeader() const { return m_hasHeaderExtension; }
  uint8_t   getPTSDTSFlags() const { return m_PTS_DTS_flags; }
  bool      hasPTS() const { return (m_PTS_DTS_flags & 0x2) == 0x2; }
  bool      hasDTS() const { return (m_PTS_DTS_flags & 0x3) == 0x3; }
  uint64_t  getPTS() const { return m_PTS; }
  uint64_t  getDTS() const { return m_DTS; }
  uint8_t   getPESHeaderDataLength() const { return m_PES_header_data_length; }
};

class xPES_Assembler
{
public:
  enum class eResult : int32_t
  {
    UnexpectedPID       = 1,
    StreamPackedLost    ,
    AssemblingStarted   ,
    AssemblingContinue  ,
    AssemblingFinished  ,
  };

public:
  // Uczynienie pól publicznymi dla łatwego dostępu (lub można dodać metody dostępowe)
  xPES_PacketHeader m_PESH;
  
protected:
  //setup
  int32_t m_PID;
  //buffer
  uint8_t* m_Buffer;
  uint32_t m_BufferSize;
  uint32_t m_DataOffset;
  //operation
  uint8_t   m_LastContinuityCounter;
  bool      m_Started;

public:
  xPES_Assembler();
  ~xPES_Assembler();

  void    Init(int32_t PID);
  eResult AbsorbPacket(const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField);

  void     PrintPESH() const { m_PESH.Print(); }
  uint8_t* getPacket() const { return m_Buffer; }
  int32_t  getNumPacketBytes() const { return m_DataOffset; }

protected:
  void xBufferReset();
  void xBufferAppend(const uint8_t* Data, int32_t Size);
};
