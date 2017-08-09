/*
 * anim1b:
 * Image to ssd1306 image converter.
 * 2017 - Tara Keeling
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <FreeImage.h>
#include "cmdline.h"
#include "output.h"

#define BIT( n ) ( 1 << n )

int AreWeWritingAGIF( void );
uint8_t* AllocateFramebuffer( void );
int OpenOutput( void );
void CloseOutput( void );

static int Display_Width = 0;
static int Display_Height = 0;
static int Display_Size = 0;

static int ShouldWriteHeader = 0;
static int ShouldInvertOutput = 0;

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

    if ( FreeImage_GetBPP( Input ) > 1 ) {
        if ( CmdLine_DitherEnabled( ) ) {
            Output = FreeImage_Dither( Input, CmdLine_GetDitherAlgorithm( ) );
        } else {
            Output = FreeImage_Threshold( Input, CmdLine_GetColorThreshold( ) );
        }
    } else {
        /* Do not modify images that are already 1bpp. */
        Output = Input;
    }

    /* If requested on the command line, invert the output */
    if ( Output != NULL && ShouldInvertOutput )
        FreeImage_Invert( Output );

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
    return Index;
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

uint8_t* AllocateFramebuffer( void ) {
    uint8_t* Result = NULL;

    if ( ( Result = ( ( uint8_t* ) malloc( Display_Size ) ) ) != NULL ) {
        memset( Result, 0, Display_Size );
    }

    return Result;
}

void Go( void ) {
    const char** InputFiles = NULL;
    uint8_t* Framebuffer = NULL;
    FIBITMAP* ImageBW = NULL;
    FIBITMAP* Image = NULL;
    int FramesWritten = 0;
    int Result = 0;
    int Count = 0;
    int i = 0;

    if ( ( Framebuffer = AllocateFramebuffer( ) ) == NULL ) {
        printf( "Error allocating %d bytes for a framebuffer.\n", Display_Size );
        return;
    }

    Result = OpenOutput( );

    switch ( Result ) {
        case EEXIST: {
            printf( "Cancelled.\n" );
            return;
        }
        case 0: {
            printf( "Failed to open \"%s\" for write.\n", CmdLine_GetOutputFilename( ) );
            return;
        }
        default: break;
    };

    if ( ( InputFiles = CmdLine_GetInputFilenames( ) ) != NULL ) {
        Count = CmdLine_GetInputCount( );

        printf( "Converting %d input files...\n", Count );

        for ( i = 0; i < Count; i++ ) {
            /* Make sure the input image is the proper dimensions */
            if ( ( Image = VerifyAndLoadInput( InputFiles[ i ] ) ) == NULL ) {
                printf( "Error: VerifyAndLoadInput %s\n", InputFiles[ i ] );
                break;
            }

            /* Convert input image to 1 bit per pixel */
            if ( ( ImageBW = Make1BPP( Image ) ) != NULL ) {
                if ( AreWeWritingAGIF( ) == 0 ) {
                    SSD1306_Format( ImageBW, Framebuffer );
                    WriteFrame( ( void* ) Framebuffer );
                } else {
                    WriteFrame( ( void* ) ImageBW );
                }

                FreeImage_Unload( ImageBW );
                FramesWritten++;
            }

            FreeImage_Unload( Image );
        }
    }

    //printf( "Wrote %d frame(s) to \"%s\"\n", FramesWritten, CmdLine_GetOutputFilename( ) );

    CloseOutput( );
    free( Framebuffer );
}

int main( int Argc, char** Argv ) {
    FreeImage_Initialise( FALSE );
    FreeImage_SetOutputMessage( ErrorHandler );

    if ( CmdLine_Handler( Argc, Argv ) == 0 ) {
        Display_Width = CmdLine_GetOutputWidth( );
        Display_Height = CmdLine_GetOutputHeight( );
        Display_Size = ( Display_Width * Display_Height ) / 8;
        ShouldInvertOutput = CmdLine_GetInvertFlag( );

        Go( );
        CmdLine_Free( );
    }

    FreeImage_DeInitialise( );
    return 0;
}

