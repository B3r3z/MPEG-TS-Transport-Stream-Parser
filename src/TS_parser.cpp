/**
 * @file TS_parser.cpp
 * @brief Main MPEG-2 Transport Stream parser application with PES packet assembly
 * 
 * This application parses MPEG-2 Transport Stream files and performs analysis of
 * packet headers, adaptation fields, and PES packet assembly for audio streams.
 * It provides comprehensive output including timing information, packet validation,
 * and detailed field-by-field analysis suitable for broadcast stream debugging.
 * 
 * Key features:
 * - Complete TS packet header parsing and validation
 * - Adaptation field analysis including PCR/OPCR timing references
 * - PES packet assembly for audio PID (136) with continuity checking
 * - Stuffing byte tracking and packet length verification
 * - Formatted output for analysis and debugging purposes
 * 
 * @author Bartosz Berezowski
 * @date 2025
 * @version 1.0
 */

#include "../include/tsCommon.h"
#include "../include/tsTransportStream.h"
#include "../include/pesParse.h"
#include <fstream>
#include <iomanip>
#include <cinttypes> // For PRIu64 printf format specifier

//=============================================================================================================================================================================

/**
 * @brief Saves binary data to a text file in hexadecimal format
 * 
 * Utility function for debugging purposes that converts binary data to readable
 * hexadecimal representation with proper formatting (16 bytes per line).
 * 
 * @param data Pointer to binary data buffer
 * @param size Number of bytes to save
 * @param outputFileName Name of output text file to create
 * 
 * @note Currently unused but available for debugging binary packet content
 * @note Output format: "XX XX XX ... XX" with newline every 16 bytes
 */
void SaveBinaryToTextFile(const uint8_t* data, size_t size, const std::string& outputFileName)
{
  std::ofstream outputFile(outputFileName);
  if (!outputFile.is_open())
  {
    printf("Error: Could not open file %s for writing\n", outputFileName.c_str());
    return;
  }

  // Write data in hexadecimal format with 16 bytes per line
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

/**
 * @brief Main application entry point for MPEG-2 Transport Stream analysis
 * 
 * Processes MPEG-2 Transport Stream files packet by packet, performing comprehensive
 * analysis including header parsing, adaptation field processing, and PES packet
 * assembly for audio streams. Results are written to "analysis_output.txt" with
 * detailed per-packet information.
 * 
 * Command line usage: ./TS-PARSER <input_file>
 * 
 * Processing workflow:
 * 1. Parse 188-byte TS packet headers
 * 2. Process adaptation fields when present (PCR/OPCR/stuffing bytes)
 * 3. Assemble PES packets for audio PID (136)
 * 4. Validate packet continuity and PES packet integrity
 * 5. Generate formatted analysis output
 * 
 * Output format per packet:
 * "XXXXXXXXXX TS: SB=XX E=X S=X P=X PID=XXXX TSC=X AF=X CC=XX [AF details] [PES status]"
 * 
 * @param argc Command line argument count
 * @param argv Command line arguments (argv[1] = input file path)
 * @param envp Environment variables (unused)
 * @return EXIT_SUCCESS on successful completion, EXIT_FAILURE on error
 * 
 * @note Requires input file to contain valid MPEG-2 TS packets (188 bytes each)
 * @note Creates "analysis_output.txt" in current directory with analysis results
 * @note Currently configured for audio PID 136 (commonly used in DVB broadcasts)
 */
int main(int argc, char *argv[], char *envp[])
{
  // Validate command line arguments
  if (argc < 2)
  {
    printf("Usage: %s <input_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Open input Transport Stream file in binary mode
  std::ifstream inputFile(argv[1], std::ios::binary);
  if (!inputFile.is_open())
  {
    printf("Error: Could not open file %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  // Create output file for analysis results
  std::ofstream outputFile("analysis_output.txt");
  if (!outputFile.is_open())
  {
    printf("Error: Could not open file analysis_output.txt for writing\n");
    return EXIT_FAILURE;
  }
  
  // Initialize parsing objects and variables
  xTS_PacketHeader TS_PacketHeader;      // TS packet header parser
  xTS_AdaptationField TS_AdaptationField; // Adaptation field parser
  uint8_t TS_PacketBuffer[xTS::TS_PacketLength]; // 188-byte packet buffer
  int32_t TS_PacketId = 0;               // Sequential packet counter
  
  // Initialize PES assembler for audio stream analysis
  const int32_t AUDIO_PID = 136;         // Target PID for audio stream (DVB standard)
  xPES_Assembler PES_Assembler;
  PES_Assembler.Init(AUDIO_PID);

// Main packet processing loop - read and analyze each 188-byte TS packet
while (inputFile.read(reinterpret_cast<char*>(TS_PacketBuffer), xTS::TS_PacketLength)) {
  // Reset parser objects for new packet
  TS_PacketHeader.Reset();
  TS_AdaptationField.Reset();
  
  // Parse Transport Stream packet header (4 bytes)
  if (TS_PacketHeader.Parse(TS_PacketBuffer) == xTS::TS_HeaderLength) {
    // Parse adaptation field if present (AFC = 2 or 3)
    if (TS_PacketHeader.getAdaptationFieldControl() == 2 || 
        TS_PacketHeader.getAdaptationFieldControl() == 3) {
      int32_t result = TS_AdaptationField.Parse(TS_PacketBuffer + xTS::TS_HeaderLength, 
                                                TS_PacketHeader.getAdaptationFieldControl());
      if (result < 0) {
        outputFile << "Error parsing adaptation field in packet " << TS_PacketId << "\n";
      }
    }
    
    // Format and output basic TS packet header information
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
    
    // Output adaptation field details if present
    if (TS_PacketHeader.getAdaptationFieldControl() == 2 || 
        TS_PacketHeader.getAdaptationFieldControl() == 3) {
      char afBuffer[256];
      snprintf(afBuffer, sizeof(afBuffer), " AF: L=%d DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
               TS_AdaptationField.getAdaptationFieldLength(),
               TS_AdaptationField.getDiscontinuityIndicator(),
               TS_AdaptationField.getRandomAccessIndicator(),
               TS_AdaptationField.getESPriorityIndicator(),
               TS_AdaptationField.getPCRFlag(),               TS_AdaptationField.getOPCRFlag(),
               TS_AdaptationField.getSplicingPointFlag(),
               TS_AdaptationField.getTransportPrivateDataFlag(),
               TS_AdaptationField.getExtensionFlag());
      outputFile << afBuffer;
      
      // Output Program Clock Reference (PCR) timing information if present
      if (TS_AdaptationField.getPCRFlag()) {
        outputFile << " PCR_base=" << TS_AdaptationField.getPCRBase()
                  << " PCR_ext=" << TS_AdaptationField.getPCRExtension()
                  << " PCR=" << TS_AdaptationField.getPCR();
      }
      
      // Output Original Program Clock Reference (OPCR) timing information if present
      if (TS_AdaptationField.getOPCRFlag()) {
        outputFile << " OPCR_base=" << TS_AdaptationField.getOPCRBase()
                  << " OPCR_ext=" << TS_AdaptationField.getOPCRExtension()
                  << " OPCR=" << TS_AdaptationField.getOPCR();
      }
      
      // Output stuffing byte count for padding analysis
      outputFile << " StuffingBytes=" << TS_AdaptationField.getStuffingBytes();
    }
    
    // Process PES packet assembly for target audio PID
    if (TS_PacketHeader.getPID() == AUDIO_PID) {
      xPES_Assembler::eResult result = PES_Assembler.AbsorbPacket(TS_PacketBuffer, 
                                                                  &TS_PacketHeader, 
                                                                  &TS_AdaptationField);
      
      // Output PES assembly status and information
      switch (result) {
        case xPES_Assembler::eResult::UnexpectedPID:
          // Should not occur due to PID filtering, but included for completeness
          break;
          
        case xPES_Assembler::eResult::StreamPackedLost:
          outputFile << " PES: PacketLost";
          break;
          
        case xPES_Assembler::eResult::AssemblingStarted:
          outputFile << " PES: Started";
          // Output PES header information for new packet
          outputFile << " PES: PSCP=" << PES_Assembler.m_PESH.getPacketStartCodePrefix()
                    << " SID=" << static_cast<int>(PES_Assembler.m_PESH.getStreamId())
                    << " L=" << PES_Assembler.m_PESH.getPacketLength();
          
          // Include timing information if available
          if (PES_Assembler.m_PESH.hasPTS()) {
            outputFile << " PTS=" << PES_Assembler.m_PESH.getPTS();
          }
          if (PES_Assembler.m_PESH.hasDTS()) {
            outputFile << " DTS=" << PES_Assembler.m_PESH.getDTS();
          }
          break;
          
        case xPES_Assembler::eResult::AssemblingContinue:
          outputFile << " PES: Continue";
          break;
          
        case xPES_Assembler::eResult::AssemblingFinished:
          outputFile << " PES: Finished Length=" << PES_Assembler.getNumPacketBytes();
          
          // Perform PES packet integrity verification
          if (PES_Assembler.m_PESH.getPacketLength() > 0) {
            // Calculate expected PES packet size: 6-byte header + PacketLength field
            uint32_t expectedLength = PES_Assembler.m_PESH.getPacketLength() + 6;
            uint32_t actualLength = PES_Assembler.getNumPacketBytes();
            uint32_t totalStuffing = PES_Assembler.getTotalStuffingBytes();
            
            outputFile << " StuffingBytes=" << totalStuffing;
            
            // Verify packet length integrity (stuffing bytes don't affect PES length)
            int32_t difference = std::abs(static_cast<int32_t>(expectedLength - actualLength));
            
            if (difference == 0) {
              outputFile << " (Verified OK - exact match)";
            } else if (difference <= 4) {
              outputFile << " (Verified OK with tolerance)";
            } else {
              outputFile << " (Length mismatch: expected=" << expectedLength 
                        << ", actual=" << actualLength
                        << ", diff=" << difference << ")";
            }
          }
          break;
          
        default:
          break;
      }
    }
    
    // Complete packet analysis line
    outputFile << "\n";
  } else {
    // TS packet header parsing failed
    outputFile << "Error parsing packet " << TS_PacketId << "\n";
  }
  
  // Increment packet counter for next iteration
  TS_PacketId++;
}

  // Cleanup and return success
  inputFile.close();
  outputFile.close();
  return EXIT_SUCCESS;
}

//=============================================================================================================================================================================
