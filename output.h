#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#define VerboseMessage( Text, ... ) { \
    printf( "%s line %d: %s.\n", __FUNCTION__, __LINE__, Text ); \
}

#define NullCheck( ptr, retexpr ) { \
    if ( ptr == NULL ) { \
        printf( "%s line %d: %s == NULL\n", __FUNCTION__, __LINE__, #ptr ); \
        retexpr; \
    }; \
}

#define CheckExpr( expr, retexpr ) { \
    if ( expr ) { \
        printf( "%s line %d: %s\n", __FUNCTION__, __LINE__, #expr ); \
        retexpr; \
    } \
}

enum {
    Format_1306_Horizontal = 0,
    Format_1306_Vertical,
    Format_Linear
};

struct ANM0_Header {
    /* ANM Header word:
     * Always starts with ANM with the last
     * byte being the header version.
     * The first version being ANM0
     */
    uint32_t ANMId;

    /* How the image is layed out in memory */
    uint8_t AddressMode;

    /* Unused (for now) */
    uint8_t CompressionType;

    /* Number of frames in this file, 1 if a single image */
    uint16_t FrameCount;

    /* Delay in milliseconds to wait before showing the next frame */
    uint16_t DelayBetweenFrames;

    uint16_t Width;
    uint16_t Height;

    uint16_t Reserved;
};

bool IsOutputAGIF( void );
bool IsOutputANM( void );

void SetOutputParameters( int Width, int Height );
bool OpenOutputFile( void );
void CloseOutputFile( void );
bool WriteOutputFile( void* Data );

#endif
