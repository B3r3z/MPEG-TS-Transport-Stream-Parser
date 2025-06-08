#include "../include/tsTransportStream.h"

//=============================================================================================================================================================================
// xTS_PacketHeader
//=============================================================================================================================================================================


/// @brief Reset - reset all TS packet header fields
void xTS_PacketHeader::Reset()
{
  m_SB = 0;
  m_E = 0;
  m_S = 0;
  m_T = 0;
  m_PID = 0;
  m_TSC = 0;
  m_AFC = 0;
  m_CC = 0;
}

/**
  @brief Parse all TS packet header fields
  @param Input is pointer to buffer containing TS packet
  @return Number of parsed bytes (4 on success, -1 on failure) 
 */
int32_t xTS_PacketHeader::Parse(const uint8_t* Input)
{
  if (!Input) return NOT_VALID;

  m_SB = Input[0];
  if (m_SB != 0x47) return NOT_VALID; // Sync byte must be 0x47

  m_E = (Input[1] & 0x80) >> 7;
  m_S = (Input[1] & 0x40) >> 6;
  m_T = (Input[1] & 0x20) >> 5;
  m_PID = ((Input[1] & 0x1F) << 8) | Input[2];
  m_TSC = (Input[3] & 0xC0) >> 6;
  m_AFC = (Input[3] & 0x30) >> 4;
  m_CC = Input[3] & 0x0F;

  return xTS::TS_HeaderLength;
}

/// @brief Print all TS packet header fields
void xTS_PacketHeader::Print() const
{
  printf("SB=%02X E=%d S=%d P=%d PID=%4d TSC=%d AF=%d CC=%2d",
         m_SB, m_E, m_S, m_T, m_PID, m_TSC, m_AFC, m_CC);
}

//=============================================================================================================================================================================
// xTS_AdaptationField
//=============================================================================================================================================================================

/// @brief Reset - reset all TS adaptation field fields
void  xTS_AdaptationField::Reset(){
  m_AFC = 0;
  m_Len = 0;
  //flag resetet
  m_DC = 0;
  m_RA = 0;
  m_SP = 0;
  m_PR = 0;
  m_OR = 0;
  m_SF = 0;
  m_TP = 0;
  m_EX = 0;
  //pcr and opcr
  m_PCR_base = 0;
  m_PCR_extension = 0;
  m_OPCR_base = 0;
  m_OPCR_extension = 0;
}

/**
  @brief Parse all TS adaptation field fields
  @param PacketBuffer is pointer to buffer containing TS packet
  @param AdaptationFieldControl is the Adaptation Field Control value from header
  @return Number of parsed bytes (length+1 on success, -1 on failure) 
 */
int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer, uint8_t AdaptationFieldControl){
  if(PacketBuffer == nullptr) return NOT_VALID;
  
  // Zapisz wskaźnik do bufora dla późniejszej analizy
  m_Buffer = PacketBuffer;
  m_AFC = AdaptationFieldControl;
  
  if(m_AFC != 2 && m_AFC != 3){
    // No adaptation field present
    m_Len = 0;
    return 0;
  }
  
  m_Len = PacketBuffer[0];
  if((m_Len > 183 && m_AFC == 3) || (m_Len > 184 && m_AFC == 2)) return NOT_VALID; // Invalid length for adaptation field
  
  // Zresetuj wartości pól opcjonalnych
  m_PrivateDataLength = 0;
  m_ExtensionLength = 0;
  m_SplicingPointOffset = 0;
  
  if (m_Len >= 1) {
      // Parsowanie flagi bajtu sterującego
      m_DC = (PacketBuffer[1] & 0x80) >> 7;  // Discontinuity indicator (bit 7)
      m_RA = (PacketBuffer[1] & 0x40) >> 6;  // Random access indicator (bit 6)
      m_SP = (PacketBuffer[1] & 0x20) >> 5;  // Elementary stream priority indicator (bit 5)
      m_PR = (PacketBuffer[1] & 0x10) >> 4;  // PCR flag (bit 4)
      m_OR = (PacketBuffer[1] & 0x08) >> 3;  // OPCR flag (bit 3)
      m_SF = (PacketBuffer[1] & 0x04) >> 2;  // Splicing point flag (bit 2)
      m_TP = (PacketBuffer[1] & 0x02) >> 1;  // Transport private data flag (bit 1)
      m_EX = (PacketBuffer[1] & 0x01);       // Extension flag (bit 0)
      
      int currentOffset = 2; // Początkowy offset po bajcie flagi
      
      // Parsowanie PCR (Program Clock Reference)
      if (m_PR && m_Len >= currentOffset + 6) {
          // PCR_base (33 bity) = bajt 2 (8 bitów) + bajt 3 (8 bitów) + bajt 4 (8 bitów) + bajt 5 (8 bitów) + pierwsze 7 bitów z bajtu 6
          m_PCR_base = ((uint64_t)PacketBuffer[currentOffset] << 25) |
                       ((uint64_t)PacketBuffer[currentOffset + 1] << 17) |
                       ((uint64_t)PacketBuffer[currentOffset + 2] << 9) |
                       ((uint64_t)PacketBuffer[currentOffset + 3] << 1) |
                       ((uint64_t)PacketBuffer[currentOffset + 4] >> 7);
          
          // PCR_extension (9 bitów) = ostatni bit z bajtu 6 + bajt 7 (8 bitów)
          m_PCR_extension = ((uint16_t)(PacketBuffer[currentOffset + 4] & 0x01) << 8) |
                           (uint16_t)PacketBuffer[currentOffset + 5];
          
          currentOffset += 6; // Przesuń wskaźnik po PCR
      }
      
      // Parsowanie OPCR (Original Program Clock Reference)
      if (m_OR && m_Len >= currentOffset + 6) {
          // OPCR_base (33 bity) = analogicznie jak dla PCR_base
          m_OPCR_base = ((uint64_t)PacketBuffer[currentOffset] << 25) |
                        ((uint64_t)PacketBuffer[currentOffset + 1] << 17) |
                        ((uint64_t)PacketBuffer[currentOffset + 2] << 9) |
                        ((uint64_t)PacketBuffer[currentOffset + 3] << 1) |
                        ((uint64_t)PacketBuffer[currentOffset + 4] >> 7);
          
          // OPCR_extension (9 bitów) = analogicznie jak dla PCR_extension
          m_OPCR_extension = ((uint16_t)(PacketBuffer[currentOffset + 4] & 0x01) << 8) |
                            (uint16_t)PacketBuffer[currentOffset + 5];
          
          currentOffset += 6; // Przesuń wskaźnik po OPCR
      }
      
      // Parsowanie Splice countdown
      if (m_SF && m_Len >= currentOffset + 1) {
          m_SplicingPointOffset = PacketBuffer[currentOffset];
          currentOffset += 1;
      }
      
      // Parsowanie Transport private data
      if (m_TP && m_Len >= currentOffset + 1) {
          m_PrivateDataLength = PacketBuffer[currentOffset];
          currentOffset += 1; // Przejście do pola długości private data
          
          // Przesunięcie offsetu o długość danych prywatnych, jeśli są dostępne
          if (m_Len >= currentOffset + m_PrivateDataLength) {
              currentOffset += m_PrivateDataLength;
          }
      }
      
      // Parsowanie Extension data
      if (m_EX && m_Len >= currentOffset + 1) {
          m_ExtensionLength = PacketBuffer[currentOffset];
          currentOffset += 1 + m_ExtensionLength; // Przejście za dane rozszerzenia
      }
  }
    return m_Len + 1; // +1 for the length byte itself
}


void xTS_AdaptationField::Print() const{
  printf("AF: L=%3d DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
         m_Len, m_DC, m_RA, m_SP, m_PR, m_OR, m_SF, m_TP, m_EX);
}

int32_t xTS_AdaptationField::getStuffingBytes() const{
    // Jeśli nie ma pola adaptacyjnego lub jego długość wynosi 0, to nie ma bajtów wypełniających
    if (m_AFC != 2 && m_AFC != 3 || m_Len == 0) {
        return 0;
    }
    
    // Długość obowiązkowego bajtu flag
    int32_t usedBytes = 1;
    
    // Dodaj długość PCR (6 bajtów), jeśli występuje
    if (m_PR) {
        usedBytes += 6;
    }
    
    // Dodaj długość OPCR (6 bajtów), jeśli występuje
    if (m_OR) {
        usedBytes += 6;
    }
    
    // Dodaj długość Splice countdown (1 bajt), jeśli występuje
    if (m_SF) {
        usedBytes += 1;
    }
    
    // Dodaj długość Transport private data, jeśli występuje
    if (m_TP && m_Buffer) {
        int privateDataOffset = 2; // Start po bajcie flag
        if (m_PR) privateDataOffset += 6;
        if (m_OR) privateDataOffset += 6;
        if (m_SF) privateDataOffset += 1;
        
        // Sprawdź czy mamy dostęp do bajtu długości
        if (m_Len >= privateDataOffset) {
            uint8_t privateDataLength = m_Buffer[privateDataOffset];
            usedBytes += 1 + privateDataLength; // 1 bajt długości + dane
        }
    }
    
    // Dodaj długość Extension, jeśli występuje
    if (m_EX && m_Buffer) {
        int extensionOffset = 2; // Start po bajcie flag
        if (m_PR) extensionOffset += 6;
        if (m_OR) extensionOffset += 6;
        if (m_SF) extensionOffset += 1;
        if (m_TP) extensionOffset += 1 + m_PrivateDataLength;
        
        // Sprawdź czy mamy dostęp do bajtu długości
        if (m_Len >= extensionOffset) {
            uint8_t extensionLength = m_Buffer[extensionOffset];
            usedBytes += 1 + extensionLength; // 1 bajt długości + dane
        }
    }
    
    // Liczba bajtów wypełniających to różnica między całkowitą długością a użytymi bajtami
    int32_t stuffingBytes = m_Len - usedBytes;
    return (stuffingBytes > 0) ? stuffingBytes : 0;
}