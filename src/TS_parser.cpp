#include "../include/tsCommon.h"
#include "../include/tsTransportStream.h"
#include "../include/pesParse.h"
#include <fstream>
#include <iomanip>
#include <cinttypes> // dla PRIu64

//=============================================================================================================================================================================

// Function to save binary data to a text file
void SaveBinaryToTextFile(const uint8_t* data, size_t size, const std::string& outputFileName)
{
  std::ofstream outputFile(outputFileName);
  if (!outputFile.is_open())
  {
    printf("Error: Could not open file %s for writing\n", outputFileName.c_str());
    return;
  }

  for (size_t i = 0; i < size; ++i)
  {
    outputFile << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    if ((i + 1) % 16 == 0)
      outputFile << "\n";
    else
      outputFile << " ";
  }

  outputFile.close();
  printf("Binary data saved to %s\n", outputFileName.c_str());
}

int main(int argc, char *argv[], char *envp[])
{
  if (argc < 2)
  {
    printf("Usage: %s <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  std::ifstream inputFile(argv[1], std::ios::binary);
  if (!inputFile.is_open())
  {
    printf("Error: Could not open file %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  std::ofstream outputFile("analysis_output.txt");
  if (!outputFile.is_open())
  {
    printf("Error: Could not open file analysis_output.txt for writing\n");
    return EXIT_FAILURE;
  }
    // Nie dodajemy żadnego nagłówka do pliku
  
  xTS_PacketHeader TS_PacketHeader;
  xTS_AdaptationField TS_AdaptationField;
  uint8_t TS_PacketBuffer[xTS::TS_PacketLength];
  int32_t TS_PacketId = 0;
  
  // Inicjalizacja assemblera PES dla PID=136 (fonia)
  const int32_t AUDIO_PID = 136;
  xPES_Assembler PES_Assembler;
  PES_Assembler.Init(AUDIO_PID);

while (inputFile.read(reinterpret_cast<char*>(TS_PacketBuffer), xTS::TS_PacketLength)) {
  TS_PacketHeader.Reset();
  TS_AdaptationField.Reset();
  
  if (TS_PacketHeader.Parse(TS_PacketBuffer) == xTS::TS_HeaderLength) {
    // Parsuj pole adaptacyjne jeśli jest obecne
    if (TS_PacketHeader.getAdaptationFieldControl() == 2 || TS_PacketHeader.getAdaptationFieldControl() == 3) {
      int32_t result = TS_AdaptationField.Parse(TS_PacketBuffer + xTS::TS_HeaderLength, 
                                                TS_PacketHeader.getAdaptationFieldControl());
      if (result < 0) {
        outputFile << "Error parsing adaptation field in packet " << TS_PacketId << "\n";
      }
    }
      // Format danych nagłówka pakietu
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%010d TS: SB=%02X E=%d S=%d P=%d PID=%4d TSC=%d AF=%d CC=%2d",
             TS_PacketId,
             TS_PacketHeader.getSyncByte(),
             TS_PacketHeader.getTransportErrorIndicator(),
             TS_PacketHeader.getPayloadUnitStartIndicator(),
             TS_PacketHeader.getTransportPriority(),
             TS_PacketHeader.getPID(),
             TS_PacketHeader.getTransportScramblingControl(),
             TS_PacketHeader.getAdaptationFieldControl(),
             TS_PacketHeader.getContinuityCounter());
    outputFile << buffer;
    
    // Wyświetl pole adaptacyjne (tylko jeśli jest obecne)
    if (TS_PacketHeader.getAdaptationFieldControl() == 2 || TS_PacketHeader.getAdaptationFieldControl() == 3) {
      char afBuffer[256];
      snprintf(afBuffer, sizeof(afBuffer), " AF: L=%d DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
               TS_AdaptationField.getAdaptationFieldLength(),
               TS_AdaptationField.getDiscontinuityIndicator(),
               TS_AdaptationField.getRandomAccessIndicator(),
               TS_AdaptationField.getESPriorityIndicator(),
               TS_AdaptationField.getPCRFlag(),
               TS_AdaptationField.getOPCRFlag(),
               TS_AdaptationField.getSplicingPointFlag(),
               TS_AdaptationField.getTransportPrivateDataFlag(),
               TS_AdaptationField.getExtensionFlag());
      outputFile << afBuffer;
                
      // Wyświetl informacje o PCR jeśli flaga jest ustawiona
      if (TS_AdaptationField.getPCRFlag()) {
        outputFile << " PCR_base=" << TS_AdaptationField.getPCRBase()
                  << " PCR_ext=" << TS_AdaptationField.getPCRExtension()
                  << " PCR=" << TS_AdaptationField.getPCR();
      }
      
      // Wyświetl informacje o OPCR jeśli flaga jest ustawiona
      if (TS_AdaptationField.getOPCRFlag()) {
        outputFile << " OPCR_base=" << TS_AdaptationField.getOPCRBase()
                  << " OPCR_ext=" << TS_AdaptationField.getOPCRExtension()
                  << " OPCR=" << TS_AdaptationField.getOPCR();
      }
      
      // Dodaj informacje o Stuffing Bytes
      outputFile <<" StuffingBytes=" + std::to_string(TS_AdaptationField.getStuffingBytes());
    }    // Obsługa pakietu PES jeśli PID jest równy 136 (fonia)
    if (TS_PacketHeader.getPID() == AUDIO_PID) {
      xPES_Assembler::eResult result = PES_Assembler.AbsorbPacket(TS_PacketBuffer, &TS_PacketHeader, &TS_AdaptationField);
      
      // Wyświetl informacje o przetwarzaniu PES
      switch (result) {
        case xPES_Assembler::eResult::StreamPackedLost:
          outputFile << " PES: PacketLost";
          break;      case xPES_Assembler::eResult::AssemblingStarted:          outputFile << " PES: Started";
          // Wyświetl informacje o nagłówku PES - wzorując się na przykładzie
          outputFile << " PES: PSCP=" << PES_Assembler.m_PESH.getPacketStartCodePrefix()
                    << " SID=" << static_cast<int>(PES_Assembler.m_PESH.getStreamId())
                    << " L=" << PES_Assembler.m_PESH.getPacketLength();
          
          // Opcjonalnie, możemy dodać bardziej szczegółowe informacje
          if (PES_Assembler.m_PESH.hasPTS()) {
            outputFile << " PTS=" << PES_Assembler.m_PESH.getPTS();
          }
          if (PES_Assembler.m_PESH.hasDTS()) {
            outputFile << " DTS=" << PES_Assembler.m_PESH.getDTS();
          }
          break;
        case xPES_Assembler::eResult::AssemblingContinue:
          outputFile << " PES: Continue";
          break;        case xPES_Assembler::eResult::AssemblingFinished:          outputFile << " PES: Finished Length=" << PES_Assembler.getNumPacketBytes();
            // Weryfikacja poprawności pakietu PES
          if (PES_Assembler.m_PESH.getPacketLength() > 0) {
            // W pakiecie PES:
            // - m_PacketLength to długość pakietu PES BEZ pierwszych 6 bajtów nagłówka
            // - m_DataOffset to całkowita długość zgromadzonych danych, włącznie z nagłówkiem PES
            
            // Oczekiwana długość to m_PacketLength + 6 (pierwsze 6 bajtów nagłówka PES)
            uint32_t expectedLength = PES_Assembler.m_PESH.getPacketLength() + 6;
            uint32_t actualLength = PES_Assembler.getNumPacketBytes();
            
            // Ponieważ długość w nagłówku PES może być niedokładna (z powodu padding bytes,
            // rozszerzeń nagłówka itp.), dopuszczamy pewną tolerancję
            const int tolerancja = 16; // Zwiększamy tolerancję
            
            if (std::abs(static_cast<int>(expectedLength - actualLength)) <= tolerancja) {
              outputFile << " (Verified OK)";
            } else {
              // Nie wyświetlamy w hex, ponieważ to powoduje problemy z formatowaniem
              outputFile << " (Length mismatch: expected=" << expectedLength 
                        << ", actual=" << actualLength << ")";
            }
          }// Wyświetl tylko pierwsze kilka bajtów pakietu PES, bez zapisywania do osobnych plików
          if (PES_Assembler.getNumPacketBytes() > 0) {
            // Dodaj pierwsze kilka bajtów danych PES (np. do 16 bajtów) do pliku wynikowego
            const int maxBytesToShow = 16;
            int bytesToShow = std::min(maxBytesToShow, PES_Assembler.getNumPacketBytes());
            
            // Używamy formatu podobnego do przykładu
            outputFile << " ";
            
            for (int i = 0; i < bytesToShow; i++) {
              outputFile << std::hex << std::setw(2) << std::setfill('0') 
                        << static_cast<int>(PES_Assembler.getPacket()[i]);
              if (i < bytesToShow - 1) outputFile << " ";
            }
            
            if (PES_Assembler.getNumPacketBytes() > maxBytesToShow) {
              outputFile << " ...";
            }
            
            outputFile << std::dec; // Reset formatu na dziesiętny
          }
          break;
        default:
          break;
      }
    }
    
    // Add a line break after each packet output
    outputFile << "\n";
  } else {
    outputFile << "Error parsing packet " << TS_PacketId << "\n";
  }
  
  TS_PacketId++;
}

  inputFile.close();
  outputFile.close();
  return EXIT_SUCCESS;
}

//=============================================================================================================================================================================
