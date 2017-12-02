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

typedef enum {
    AddressMode_Horizontal = 0,
    AddressMode_Vertical
} SSD1306_AddressMode;

bool IsOutputAGIF( void );
bool OpenGIFOutput( void );
void CloseGIFOutput( void );
bool AddGIFFrame( FIBITMAP* Input, uint32_t AnimationDelay );

bool OpenRawOutput( void );
void CloseRawOutput( void );
bool AddRawFrame( uint8_t* Data, int Width, int Height );

#endif
