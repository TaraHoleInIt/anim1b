/**
 * Copyright (c) 2017 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <FreeImage.h>
#include "cmdline.h"
#include "output.h"

#define BIT( n ) ( 1 << n )

void ErrorHandler( FREE_IMAGE_FORMAT Fmt, const char* Message ) {
    printf( "%s: %s\n", __FUNCTION__, Message );
}

/*
 * Opens the given input file (Filename) as an FIBITMAP handle.
 * OutImageWidth and OutImageHeight MUST point to valid integers to receive
 * the image width and height.
 * Width and height of the input image must be divisible by 8.
 */
FIBITMAP* OpenInputImage( const char* Filename, int* OutImageWidth, int* OutImageHeight ) {
    FREE_IMAGE_FORMAT InputFormat = FIF_UNKNOWN;
    FIBITMAP* Input = NULL;
    int Width = 0;
    int Height = 0;

    NullCheck( Filename, return NULL );
    NullCheck( OutImageWidth, return NULL );
    NullCheck( OutImageHeight, return NULL );

    InputFormat = FreeImage_GetFIFFromFilename( Filename );

    if ( InputFormat != FIF_UNKNOWN ) {
        Input = FreeImage_Load( InputFormat, Filename, 0 );
        
        if ( Input != NULL ) {
            *OutImageWidth = FreeImage_GetWidth( Input );
            *OutImageHeight = FreeImage_GetHeight( Input );

            if ( *OutImageWidth % 8 == 0 && *OutImageHeight % 8 == 0 )
                return Input;

            printf( "Error: Input image width and height must be divisible by 8.\n" );

            FreeImage_Unload( Input );
            Input = NULL;
        } else {
            printf( "Error Could not load input file [%s]\n", Filename );
        }
    }

    return Input;
}

/*
 * Checks to see if the input image is already in the correct (1BPP) format
 * and if it isn't we perform the conversion ourselves.
 */
FIBITMAP* PreprocessInput( FIBITMAP* Input ) {
    FREE_IMAGE_DITHER Algo = FID_FS;
    FIBITMAP* Output = NULL;
    int ColorThreshold = 0;

    NullCheck( Input, return NULL );

    if ( FreeImage_GetBPP( Input ) == 1 ) {
        /* Input is already 1bpp, no changes needed */
        Output = FreeImage_Clone( Input );
    } else {
        /* Convert input to 1BPP based on the user's command
         * line selection or the defaults.
         */
        if ( CmdLine_DitherEnabled( ) == true ) {
            Algo = CmdLine_GetDitherAlgorithm( );
            Output = FreeImage_Dither( Input, Algo );
        } else {
            ColorThreshold = CmdLine_GetColorThreshold( );
            Output = FreeImage_Threshold( Input, ColorThreshold );
        }
    }

    return Output;
}

/*
 * Sets (or clears) the pixel at (x,y) for the
 * SSD1306's horizontal addressing mode.
 */
void SSD1306_SetPixelHorizontal( uint8_t* Output, int x, int y, int ImageWidth, bool Color ) {
    int BitOffset = ( y & 0x07 );

    /* 
     * Divide the y coordinate by 8 to get which page
     * the bit we want to set is on.
     */
    y>>= 3;

    if ( Color == true ) {
        Output[ x + ( y * ImageWidth ) ] |= BIT( BitOffset );
    } else {
        Output[ x + ( y * ImageWidth ) ] &= ~BIT( BitOffset );
    }
}

/*
 * Converts the input bitmap to an output buffer
 * formatted for the SSD1306's horizontal addressing mode.
 */
void SSD1306_ConvertHorizontal( FIBITMAP* Input, uint8_t* Output, int Width, int Height ) {
    uint8_t Color = 0;
    int x = 0;
    int y = 0;

    NullCheck( Input, return );
    NullCheck( Output, return );

    for ( y = 0; y < Height; y++ ) {
        for ( x = 0; x < Width; x++ ) {
            FreeImage_GetPixelIndex( Input, x, y, &Color );
            SSD1306_SetPixelHorizontal( Output, x, y, Width, ( bool ) Color );
        }
    }
}

void Go_GIF( void ) {
    const char** InputFilenames = NULL;
    uint32_t AnimationDelay = 0;
    int NumberOfInputFiles = 0;
    FIBITMAP* Output = NULL;
    FIBITMAP* Input = NULL;
    int FramesWritten = 0;
    bool Errors = false;
    int Width = 0;
    int Height = 0;
    int i = 0; 

    NullCheck( CmdLine_GetInputFilenames( ), return );
    NullCheck( CmdLine_GetOutputFilename( ), return );

    InputFilenames = CmdLine_GetInputFilenames( );
    NumberOfInputFiles = CmdLine_GetInputCount( );
    AnimationDelay = CmdLine_GetOutputDelay( );

    for ( i = 0; i < NumberOfInputFiles; i++ ) {
        Input = OpenInputImage( InputFilenames[ i ], &Width, &Height );

        if ( Input != NULL ) {
            Output = PreprocessInput( Input );

            if ( Output != NULL ) {
                AddGIFFrame( Output, AnimationDelay );
                FreeImage_Unload( Output );

                FramesWritten++;
            } else {
                Errors = true;
            }

            FreeImage_Unload( Input );
        } else {
            Errors = true;

            /* Always stop on the first error */
            break;
        }
    }

    printf( "Wrote %d of %d frame(s) to [%s]\n", FramesWritten, NumberOfInputFiles, CmdLine_GetOutputFilename( ) );

    if ( Errors == true ) {
        printf( "There output file may be incomplete due to errors.\n" );
    }
}

void Go( void ) {
    NullCheck( CmdLine_GetInputFilenames( ), return );
    NullCheck( CmdLine_GetOutputFilename( ), return );

    if ( IsOutputAGIF( ) == true ) {
        if ( OpenGIFOutput( ) == true ) {
            Go_GIF( );
            CloseGIFOutput( );
        }
    } else {
        if ( OpenRawOutput( ) == true ) {
            //Go_Raw( );
            CloseRawOutput( );
        }
    }
}

int main( int Argc, char** Argv ) {
    FreeImage_Initialise( FALSE );
    FreeImage_SetOutputMessage( ErrorHandler );

    if ( CmdLine_Handler( Argc, Argv ) == 0 ) {
        Go( );
        CmdLine_Free( );
    }

    FreeImage_DeInitialise( );
    return 0;
}

