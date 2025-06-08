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
  printf("SB: 0x%02X E: %d S: %d T: %d PID: 0x%04X TSC: %d AFC: %d CC: %d",
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
}

/**
  @brief Parse all TS adaptation field fields
  @param PocketBuffer is pointer to buffer containing TS packet
  @param AdaptationFieldBuffer is pointer to buffer where adaptation field will be stored
  @return Number of parsed bytes (0 on success, -1 on failure) 
 */
int32_t xTS_AdaptationField::Parse(const uint8_t* PocketBuffer, uint8_t xTS_AdaptationField){
  if(PocketBuffer == nullptr) return NOT_VALID;
  m_AFC = xTS_AdaptationField;
  if(m_AFC != 2 && m_AFC != 3){
    // No adaptation field present
    m_Len = 0;
    return 0;
  }
  m_Len = PocketBuffer[0];
  if((m_Len > 183 && m_AFC ==3) || (m_Len >184 && m_AFC == 2)) return NOT_VALID; // Invalid length for adaptation field
  if (m_Len >= 1) {
      m_DC = (PocketBuffer[1] & 0x80) >> 7;  // 0b10000000
      m_RA = (PocketBuffer[1] & 0x40) >> 6;  // 0b01000000
      m_SP = (PocketBuffer[1] & 0x20) >> 5;  // 0b00100000
      m_PR = (PocketBuffer[1] & 0x10) >> 4;  // 0b00010000
      m_OR = (PocketBuffer[1] & 0x08) >> 3;  // 0b00001000
      m_SF = (PocketBuffer[1] & 0x04) >> 2;  // 0b00000100
      m_TP = (PocketBuffer[1] & 0x02) >> 1;  // 0b00000010
      m_EX = (PocketBuffer[1] & 0x01);       // 0b00000001
      
    // TODO: Jeśli PCR flag jest ustawiona (m_PR == 1), parsuj PCR
     if (m_PR) {
         // Parsowanie PCR z bajtów 2-7
     }
      
      // TODO: Podobnie dla innych flag (OPCR, splicing point, itp.)
  }
  
  // TODO: ANALLIZA PCR 
  return m_Len + 1; // +1 for the length byte itself
}


void xTS_AdaptationField::Print() const
{
  printf("AFC: %d Len: %d DC: %d RA: %d SP: %d PR: %d OR: %d SF: %d TP: %d EX: %d",
         m_AFC, m_Len, m_DC, m_RA, m_SP, m_PR, m_OR, m_SF, m_TP, m_EX);
}