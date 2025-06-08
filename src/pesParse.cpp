#include "../include/pesParse.h"
#include <cstring>
#include <cstdio>

//=============================================================================================================================================================================
// xPES_PacketHeader
//=============================================================================================================================================================================

/// @brief Reset - reset all PES packet header fields
void xPES_PacketHeader::Reset()
{
  // Podstawowe pola
  m_PacketStartCodePrefix = 0;
  m_StreamId = 0;
  m_PacketLength = 0;
  
  // Opcjonalne pola
  m_HeaderMarkerBits = 0;
  m_PES_scrambling_control = 0;
  m_PES_priority = 0;
  m_data_alignment_indicator = 0;
  m_copyright = 0;
  m_original_or_copy = 0;
  m_PTS_DTS_flags = 0;
  m_ESCR_flag = 0;
  m_ES_rate_flag = 0;
  m_DSM_trick_mode_flag = 0;
  m_additional_copy_info_flag = 0;
  m_PES_CRC_flag = 0;
  m_PES_extension_flag = 0;
  m_PES_header_data_length = 0;
  
  // Timestampy
  m_PTS = 0;
  m_DTS = 0;
  
  // Stan
  m_hasHeaderExtension = false;
  m_headerLength = 6; // Podstawowy nagłówek ma 6 bajtów
}

/**
  @brief Parse all PES packet header fields
  @param Input is pointer to buffer containing PES packet
  @return Number of parsed bytes (header length on success, -1 on failure) 
 */
int32_t xPES_PacketHeader::Parse(const uint8_t* Input)
{
  if (!Input) return NOT_VALID;

  // Sprawdzenie Packet Start Code Prefix - powinien mieć wartość 0x000001
  m_PacketStartCodePrefix = (Input[0] << 16) | (Input[1] << 8) | Input[2];
  if (m_PacketStartCodePrefix != 0x000001) return NOT_VALID;

  // Stream ID
  m_StreamId = Input[3];
  
  // Packet Length
  m_PacketLength = (Input[4] << 8) | Input[5];
  
  // Domyślna długość nagłówka
  m_headerLength = xTS::PES_HeaderLength; // 6 bajtów standardowego nagłówka
  
  // Sprawdź czy mamy do czynienia z rozszerzonym nagłówkiem
  // Rozszerzony nagłówek występuje tylko dla niektórych Stream ID (nie dla padding_stream, private_stream_2 itp.)
  if (m_StreamId != eStreamId_program_stream_map && 
      m_StreamId != eStreamId_padding_stream && 
      m_StreamId != eStreamId_private_stream_2 && 
      m_StreamId != eStreamId_ECM && 
      m_StreamId != eStreamId_EMM && 
      m_StreamId != eStreamId_program_stream_directory &&
      m_StreamId != eStreamId_DSMCC_stream && 
      m_StreamId != eStreamId_ITUT_H222_1_type_E) 
  {
    m_hasHeaderExtension = true;
    
    // Sprawdź czy mamy wystarczająco danych
    if (m_PacketLength < 3) return m_headerLength; // Rozszerzony nagłówek wymaga co najmniej 3 dodatkowych bajtów
    
    // Bite 8-10: '10' followed by PES_scrambling_control (2 bits)
    m_HeaderMarkerBits = (Input[6] & 0xC0) >> 6;       // Bity 6-7
    m_PES_scrambling_control = (Input[6] & 0x30) >> 4; // Bity 4-5
    
    // Bite 11-15: Flags
    m_PES_priority = (Input[6] & 0x08) >> 3;           // Bit 3
    m_data_alignment_indicator = (Input[6] & 0x04) >> 2; // Bit 2
    m_copyright = (Input[6] & 0x02) >> 1;               // Bit 1
    m_original_or_copy = Input[6] & 0x01;               // Bit 0
    
    // Bite 16-23: More flags and PES_header_data_length
    m_PTS_DTS_flags = (Input[7] & 0xC0) >> 6;           // Bity 6-7
    m_ESCR_flag = (Input[7] & 0x20) >> 5;               // Bit 5
    m_ES_rate_flag = (Input[7] & 0x10) >> 4;            // Bit 4
    m_DSM_trick_mode_flag = (Input[7] & 0x08) >> 3;     // Bit 3
    m_additional_copy_info_flag = (Input[7] & 0x04) >> 2; // Bit 2
    m_PES_CRC_flag = (Input[7] & 0x02) >> 1;             // Bit 1
    m_PES_extension_flag = Input[7] & 0x01;               // Bit 0
    
    m_PES_header_data_length = Input[8];                  // Długość dodatkowych danych nagłówka
    
    // Aktualizujemy całkowitą długość nagłówka PES
    m_headerLength = xTS::PES_HeaderLength + 3 + m_PES_header_data_length; // 6 + 3 + N
    
    // Parsowanie PTS (33 bity) - jeśli flaga jest ustawiona
    int offset = 9; // Start PTS/DTS data
    if (m_PTS_DTS_flags & 0x2) { // Jeśli flaga PTS jest ustawiona (bit 1)
      // Format PTS: '0010' (4 bity) i pierwszy bit PTS (1 bit) w pierwszym bajcie
      // Potem 7 bitów PTS, marker bit, 8 bitów PTS, marker bit, 7 bitów PTS i ostatni marker bit
      uint64_t pts_32_30 = (Input[offset] & 0x0E) >> 1;  // 3 bity (z 33) - przesunięte o 30 pozycji
      uint64_t pts_29_22 = Input[offset + 1];             // 8 bitów (z 33) - przesunięte o 22 pozycje
      uint64_t pts_21_15 = (Input[offset + 2] & 0xFE) >> 1; // 7 bitów (z 33) - przesunięte o 15 pozycji
      uint64_t pts_14_7  = Input[offset + 3];             // 8 bitów (z 33) - przesunięte o 7 pozycji
      uint64_t pts_6_0   = (Input[offset + 4] & 0xFE) >> 1; // 7 bitów (z 33) - najniższe bity
      
      m_PTS = (pts_32_30 << 30) | (pts_29_22 << 22) | (pts_21_15 << 15) | (pts_14_7 << 7) | pts_6_0;
      offset += 5;
    }
    
    // Parsowanie DTS (33 bity) - jeśli flaga jest ustawiona (tylko gdy PTS też jest dostępny)
    if (m_PTS_DTS_flags == 0x3) { // Jeśli flagi PTS i DTS są ustawione
      // Format DTS: taki sam jak PTS
      uint64_t dts_32_30 = (Input[offset] & 0x0E) >> 1;
      uint64_t dts_29_22 = Input[offset + 1];
      uint64_t dts_21_15 = (Input[offset + 2] & 0xFE) >> 1;
      uint64_t dts_14_7  = Input[offset + 3];
      uint64_t dts_6_0   = (Input[offset + 4] & 0xFE) >> 1;
      
      m_DTS = (dts_32_30 << 30) | (dts_29_22 << 22) | (dts_21_15 << 15) | (dts_14_7 << 7) | dts_6_0;
      offset += 5;
    }
    
    // Uwaga: Inne pola opcjonalne jak ESCR, ES rate, itd. mogą być dodane tutaj
    // ale dla podstawowego parsera PTS i DTS są najważniejsze
  }

  return m_headerLength;
}

/// @brief Print all PES packet header fields
void xPES_PacketHeader::Print() const
{
  printf("PES: PSCP=0x%06X SID=0x%02X PLEN=%d", 
         m_PacketStartCodePrefix, m_StreamId, m_PacketLength);
  
  if (m_hasHeaderExtension) {
    printf(" HeaderLen=%d", m_headerLength);
    
    // Wyświetl flagi
    if (hasPTS()) {
      printf(" PTS=%" PRIu64, m_PTS);
    }
    if (hasDTS()) {
      printf(" DTS=%" PRIu64, m_DTS);
    }
  }
}

//=============================================================================================================================================================================
// xPES_Assembler
//=============================================================================================================================================================================

/// @brief Constructor - initialize with default values
xPES_Assembler::xPES_Assembler()
  : m_PID(0)
  , m_Buffer(nullptr)
  , m_BufferSize(0)
  , m_DataOffset(0)
  , m_LastContinuityCounter(0)
  , m_Started(false)
{
}

/// @brief Destructor - free allocated memory
xPES_Assembler::~xPES_Assembler()
{
  if (m_Buffer)
  {
    delete[] m_Buffer;
    m_Buffer = nullptr;
  }
}

/// @brief Initialize assembler with specific PID
void xPES_Assembler::Init(int32_t PID)
{
  m_PID = PID;
  m_Started = false;
  m_LastContinuityCounter = 0;
  
  // Reset buffers
  xBufferReset();
  m_PESH.Reset();
}

/// @brief Process a TS packet and extract PES data
xPES_Assembler::eResult xPES_Assembler::AbsorbPacket(const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField)
{
  // Check if this is the expected PID
  if (PacketHeader->getPID() != m_PID)
  {
    return eResult::UnexpectedPID;
  }

  // Check for continuity counter
  if (m_Started)
  {
    // Expected continuity counter value (previous + 1) mod 16
    // Nie zwiększamy licznika dla pakietów z samym polem adaptacyjnym (bez danych)
    uint8_t expectedCC;
    if (PacketHeader->getAdaptationFieldControl() == 0 || PacketHeader->getAdaptationFieldControl() == 2) {
      // Pakiety bez payloadu nie zwiększają licznika
      expectedCC = m_LastContinuityCounter;
    } else {
      // Pakiety z payloadem zwiększają licznik
      expectedCC = (m_LastContinuityCounter + 1) & 0x0F;
    }
    
    // Sprawdź ciągłość
    if (PacketHeader->getContinuityCounter() != expectedCC)
    {
      // Wykryto utratę pakietu
      m_Started = false;
      return eResult::StreamPackedLost;
    }
  }
  
  // Aktualizuj licznik ciągłości
  if (PacketHeader->getAdaptationFieldControl() == 1 || PacketHeader->getAdaptationFieldControl() == 3) {
    // Aktualizuj licznik tylko gdy jest payload
    m_LastContinuityCounter = PacketHeader->getContinuityCounter();
  }

  // Oblicz offset początku danych
  uint32_t payloadOffset = xTS::TS_HeaderLength;
  
  // Jeśli jest pole adaptacyjne, dostosuj offset
  if (PacketHeader->getAdaptationFieldControl() & 0x2)
  {
    payloadOffset += AdaptationField->getAdaptationFieldLength() + 1; // +1 dla pola długości
  }
  
  // Jeśli pakiet nie ma danych, nie ma co przetwarzać
  if (payloadOffset >= xTS::TS_PacketLength || PacketHeader->getAdaptationFieldControl() == 0 || 
      PacketHeader->getAdaptationFieldControl() == 2)
  {
    return m_Started ? eResult::AssemblingContinue : eResult::StreamPackedLost;
  }
  
  // Oblicz rozmiar danych
  uint32_t payloadSize = xTS::TS_PacketLength - payloadOffset;
  
  // Pobierz wskaźnik do danych
  const uint8_t* payload = TransportStreamPacket + payloadOffset;

  // Jeśli pakiet ma flagę PUSI (Payload Unit Start Indicator)
  if (PacketHeader->getPayloadUnitStartIndicator())
  {
    // Jeśli już trwało składanie, zakończ poprzedni pakiet i zasygnalizuj to
    eResult result = eResult::AssemblingStarted;
    if (m_Started)
    {
      result = eResult::AssemblingFinished;
    }
    
    // Zawsze zaczynaj nowy pakiet, resetując bufor i nagłówek
    m_Started = true;
    xBufferReset();
    m_PESH.Reset();
    
    // Parsuj nagłówek PES
    int32_t pesHeaderLength = m_PESH.Parse(payload);
    if (pesHeaderLength < 0)
    {
      m_Started = false;
      return eResult::UnexpectedPID;
    }
    
    // Dodaj całe dane do bufora (włącznie z nagłówkiem PES)
    xBufferAppend(payload, payloadSize);
    
    // Jeśli pakiet PES ma określoną długość i mieści się w jednym pakiecie TS
    if (m_PESH.getPacketLength() > 0 && 
        6 + m_PESH.getPacketLength() <= payloadSize)
    {
      // Pakiet PES zakończony w jednym pakiecie TS
      return eResult::AssemblingFinished;
    }
    
    return eResult::AssemblingStarted;
  }
  else if (m_Started)
  {
    // Kontynuuj składanie istniejącego pakietu PES
    xBufferAppend(payload, payloadSize);
    
    // Sprawdź czy pakiet PES został zakończony
    if (m_PESH.getPacketLength() > 0)
    {
      // Całkowita oczekiwana długość pakietu PES = 6 bajtów podstawowego nagłówka + długość w polu PacketLength
      uint32_t expectedSize = 6 + m_PESH.getPacketLength();
      if (m_DataOffset >= expectedSize)
      {
        return eResult::AssemblingFinished;
      }
    }
    
    return eResult::AssemblingContinue;
  }
  
  // Jeśli dotarliśmy tutaj, to jest pakiet bez startu i nie jesteśmy w trakcie składania
  return eResult::StreamPackedLost;
}

/// @brief Reset internal buffer
void xPES_Assembler::xBufferReset()
{
  if (m_Buffer)
  {
    delete[] m_Buffer;
    m_Buffer = nullptr;
  }
  
  m_BufferSize = 0;
  m_DataOffset = 0;
}

/// @brief Append data to internal buffer
void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size)
{
  if (Size <= 0 || !Data) return;
  
  // Jeśli bieżący bufor nie jest wystarczająco duży
  if (m_DataOffset + Size > m_BufferSize)
  {
    // Oblicz nowy rozmiar bufora (podwój obecny rozmiar lub obecny + nowe dane, cokolwiek jest większe)
    uint32_t newSize = std::max(m_BufferSize * 2, m_DataOffset + Size);
    // Bezpieczniejsze podejście - ustaw minimalny rozmiar
    newSize = std::max(newSize, static_cast<uint32_t>(1024));
    
    uint8_t* newBuffer = nullptr;
    try {
      newBuffer = new uint8_t[newSize];
      
      // Kopiuj istniejące dane, jeśli istnieją
      if (m_Buffer && m_DataOffset > 0)
      {
        memcpy(newBuffer, m_Buffer, m_DataOffset);
      }
      
      // Usuń stary bufor i zaktualizuj wskaźniki
      if (m_Buffer)
      {
        delete[] m_Buffer;
      }
      
      m_Buffer = newBuffer;
      m_BufferSize = newSize;
    }
    catch (const std::bad_alloc& e) {
      // Obsługa błędu alokacji
      if (newBuffer) delete[] newBuffer;
      printf("Error: Memory allocation failed in xBufferAppend: %s\n", e.what());
      return;
    }
  }
  
  // Dodaj nowe dane
  memcpy(m_Buffer + m_DataOffset, Data, Size);
  m_DataOffset += Size;
}
