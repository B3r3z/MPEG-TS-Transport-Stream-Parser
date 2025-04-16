#include "tsCommon.h"
#include "tsTransportStream.h"

//=============================================================================================================================================================================

int main(int argc, char *argv[ ], char *envp[ ])
{
  // TODO - open file
  // TODO - check if file if opened

  xTS_PacketHeader    TS_PacketHeader;

  int32_t TS_PacketId = 0;
  while(/*not eof*/ 0)
  {
    // TODO - read from file

    TS_PacketHeader.Reset();
    TS_PacketHeader.Parse(/*TS_PacketBuffer*/ nullptr);

    printf("%010d ", TS_PacketId);
    TS_PacketHeader.Print();
    printf("\n");

    TS_PacketId++;
  }
  
  // TODO - close file

  return EXIT_SUCCESS;
}

//=============================================================================================================================================================================
