#include "../include/tsCommon.h"
#include "../include/tsTransportStream.h"
#include <fstream>
#include <iomanip>

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

  xTS_PacketHeader TS_PacketHeader;
  xTS_AdaptationField TS_AdaptationField;
  uint8_t TS_PacketBuffer[xTS::TS_PacketLength];
  int32_t TS_PacketId = 0;

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
