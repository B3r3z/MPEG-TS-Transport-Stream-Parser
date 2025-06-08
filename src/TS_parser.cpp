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
  uint8_t TS_PacketBuffer[xTS::TS_PacketLength];
  int32_t TS_PacketId = 0;

  while (inputFile.read(reinterpret_cast<char*>(TS_PacketBuffer), xTS::TS_PacketLength))
  {
    TS_PacketHeader.Reset();
    if (TS_PacketHeader.Parse(TS_PacketBuffer) == xTS::TS_HeaderLength)
    {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "%010d ", TS_PacketId);
      outputFile << buffer;

      // Redirect header analysis to the output file
      snprintf(buffer, sizeof(buffer), "SB: 0x%02X E: %d S: %d T: %d PID: 0x%04X TSC: %d AFC: %d CC: %d\n",
               TS_PacketHeader.getSyncByte(),
               TS_PacketHeader.getTransportErrorIndicator(),
               TS_PacketHeader.getPayloadUnitStartIndicator(),
               TS_PacketHeader.getTransportPriority(),
               TS_PacketHeader.getPID(),
               TS_PacketHeader.getTransportScramblingControl(),
               TS_PacketHeader.getAdaptationFieldControl(),
               TS_PacketHeader.getContinuityCounter());
      outputFile << buffer;
    }
    else
    {
      outputFile << "Error parsing packet " << TS_PacketId << "\n";
    }

    TS_PacketId++;
  }

  inputFile.close();
  outputFile.close();
  return EXIT_SUCCESS;
}

//=============================================================================================================================================================================
