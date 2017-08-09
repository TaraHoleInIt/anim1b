#ifndef _OUTPUT_H_
#define _OUTPUT_H_

struct ANMHeader {
    uint16_t Width;
    uint16_t Height;
    uint16_t Frames;
    uint8_t DelayBetweenFrames;
    uint8_t Reserved[ 5 ];
} __attribute__( ( packed ) );

int OpenOutput( void );
void CloseOutput( void );
void WriteFrame( void* Frame );
int AreWeWritingAGIF( void );

#endif
