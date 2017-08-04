/*
 * anim1b:
 * Image to ssd1306 image converter.
 * 2017 - Tara Keeling
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <FreeImage.h>

#define BIT( n ) ( 1 << n )

#define Display_Width 128
#define Display_Height 64
#define Display_Ratio ( ( float ) ( ( float ) Display_Width ) / ( ( float ) Display_Height ) )
#define Display_Size ( ( Display_Width * Display_Height ) / 8 )

FREE_IMAGE_DITHER CMDLine_GetDitherAlgorithm( void );
int CMDLine_DitherEnabled( void );
int CMDLine_GetColorThreshold( void );
const char** CMDLine_GetInputFilenames( void );
const char* CMDLine_GetOutputFilename( void );
int CMDLine_GetInputCount( void );
int CMDLine_GetInvertFlag( void );
int CMDLine_Handler( int Argc, char** Argv );
void CMDLine_Free( void );

int ShouldInvertOutput = 0;

void ErrorHandler( FREE_IMAGE_FORMAT Fmt, const char* Message ) {
    printf( "%s: %s\n", __FUNCTION__, Message );
}

/*
 * Make1BPP:
 * Converts the input to a 1 bit per pixel monochrome image.
 *
 * Returns: 1 Bit per pixel image.
 */
FIBITMAP* Make1BPP( FIBITMAP* Input ) {
    FIBITMAP* Output = NULL;

    if ( CMDLine_DitherEnabled( ) ) {
        Output = FreeImage_Dither( Input, CMDLine_GetDitherAlgorithm( ) );
    } else {
        Output = FreeImage_Threshold( Input, CMDLine_GetColorThreshold( ) );
    }

    return Output;
}

/*
 * VerifyAndLoadInput:
 *
 * Loads the given image file name and verifies that it's the correct dimensions.
 *
 * Returns: FIBITMAP Handle on success, NULL if failed.
 */
FIBITMAP* VerifyAndLoadInput( const char* ImagePath ) {
    FREE_IMAGE_FORMAT InputFormat = FIF_UNKNOWN;
    FIBITMAP* Output = NULL;
    int Height = 0;
    int Width = 0;

    if ( ( InputFormat = FreeImage_GetFIFFromFilename( ImagePath ) ) != FIF_UNKNOWN ) {
        if ( ( Output = FreeImage_Load( InputFormat, ImagePath, 0 ) ) != NULL ) {
            Height = FreeImage_GetHeight( Output );
            Width = FreeImage_GetWidth( Output );

            if ( Width == Display_Width && Height == Display_Height )
                return Output;

            printf( "[Error] %s: Image size is %dx%d, expected %dx%d\n", __FUNCTION__, Width, Height, Display_Width, Display_Height );
            FreeImage_Unload( Output );
        }
    }

    return NULL;
}

/*
 * GetPixelIndex:
 *
 * A simple wrapper around FreeImage_GetPixelIndex that just
 * returns the pixel index rather than needing to pass a pointer.
 *
 * Returns: Colour index of pixel at (x), (y)
 */
uint8_t PixelIndex( FIBITMAP* Input, int x, int y ) {
    uint8_t Index = 0;

    FreeImage_GetPixelIndex( Input, x, y, &Index );
    return ShouldInvertOutput ? ! Index : Index;
}

int SSD1306_Format( FIBITMAP* Input, uint8_t* OutBuffer ) {
    FIBITMAP* Tile = NULL;
    uint8_t Temp = 0;
    int x2 = 0;
    int x = 0;
    int y = 0;
    int i = 0;

    memset( OutBuffer, 0, Display_Size );

    if ( Input != NULL ) {
        /* Slice the image up into 8x8 chunks */
        for ( y = 0; y < Display_Height; y+= 8 ) {
            for ( x = 0; x < Display_Width; x+= 8 ) {
                if ( ( Tile = FreeImage_Copy( Input, x, y, x + 8, y + 8 ) ) != NULL ) {
                    for ( i = 0; i < 8; i++ ) {
                        /* Here be dragons */
                        Temp = PixelIndex( Tile, i, 7 ) & BIT( 0 ) ? BIT( 0 ) : 0;
                        Temp |= PixelIndex( Tile, i, 6 ) & BIT( 0 ) ? BIT( 1 ) : 0;
                        Temp |= PixelIndex( Tile, i, 5 ) & BIT( 0 ) ? BIT( 2 ) : 0;
                        Temp |= PixelIndex( Tile, i, 4 ) & BIT( 0 ) ? BIT( 3 ) : 0;
                        Temp |= PixelIndex( Tile, i, 3 ) & BIT( 0 ) ? BIT( 4 ) : 0;
                        Temp |= PixelIndex( Tile, i, 2 ) & BIT( 0 ) ? BIT( 5 ) : 0;
                        Temp |= PixelIndex( Tile, i, 1 ) & BIT( 0 ) ? BIT( 6 ) : 0;
                        Temp |= PixelIndex( Tile, i, 0 ) & BIT( 0 ) ? BIT( 7 ) : 0;

                        *OutBuffer++ = Temp;
                    }

                    FreeImage_Unload( Tile );
                }
            }
        }
    }

    return 1;
}

FILE* OutputFile = NULL;

int OpenOutput( void ) {
    const char* Filename = NULL;
    struct stat st;

    if ( ( Filename = CMDLine_GetOutputFilename( ) ) != NULL ) {
        if ( access( Filename, F_OK ) == 0 ) {
            printf( "Output file \"%s\" already exists, overwrite? (Y/N) ", Filename );

            if ( tolower( getchar( ) ) == 'n' )
                return 0;
        }

        OutputFile = fopen( Filename, "wb+" );
    }

    return OutputFile ? 1 : 0;
}

void CloseOutput( void ) {
    if ( OutputFile ) {
        fclose( OutputFile );
        OutputFile = NULL;
    }
}

void WriteFBToDisk( uint8_t* FB ) {
    if ( OutputFile && FB )
        fwrite( FB, 1, Display_Size, OutputFile );
}

int main( int Argc, char** Argv ) {
    static uint8_t FramebufferOut[ Display_Size ];
    const char** Filenames = NULL;
    FIBITMAP* Input = NULL;
    FIBITMAP* BW = NULL;
    int Frames = 0;
    int i = 0;

    FreeImage_Initialise( FALSE );
    FreeImage_SetOutputMessage( ErrorHandler );

    if ( CMDLine_Handler( Argc, Argv ) == 0 ) {
        ShouldInvertOutput = CMDLine_GetInvertFlag( );

        if ( ( Filenames = CMDLine_GetInputFilenames( ) ) != NULL ) {
            if ( OpenOutput( ) ) {
                for ( i = 0; i < CMDLine_GetInputCount( ); i++ ) {
                    if ( ( Input = VerifyAndLoadInput( Filenames[ i ] ) ) != NULL ) {
                        if ( ( BW = Make1BPP( Input ) ) != NULL ) {
                            SSD1306_Format( BW, FramebufferOut );
                            WriteFBToDisk( FramebufferOut );

                            FreeImage_Unload( BW );
                            Frames++;
                        }

                        FreeImage_Unload( Input );
                    }
                }

                CloseOutput( );
                printf( "Wrote %d frames into \"%s\"\n", Frames, CMDLine_GetOutputFilename( ) );
            }
        }

        CMDLine_Free( );
    }

    FreeImage_DeInitialise( );
    return 0;
}
