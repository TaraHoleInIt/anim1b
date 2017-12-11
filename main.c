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
#include <errno.h>
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
        }
    }

    return Input;
}

/*
 * Checks to see if the input image is already in the correct (1BPP) format
 * and if it isn't we perform the conversion ourselves.
 */
FIBITMAP* GetProcessedOutput( FIBITMAP* Input ) {
    FREE_IMAGE_DITHER Algo = FID_FS;
    FIBITMAP* Output = NULL;
    int ColorThreshold = 0;

    NullCheck( Input, return NULL );

    /* If requested, invert the colours on the input image data */
    if ( CmdLine_GetInvertFlag( ) == true ) {
        FreeImage_Invert( Input );
    }

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

    /* Invert color if command line option set */
    if ( CmdLine_GetInvertFlag( ) == true ) {
        Color = ! Color;
    }

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

void DoOutputConversion( FIBITMAP* Input, uint8_t* Output, int Width, int Height ) {
    switch ( CmdLine_GetOutputFormat( ) ) {
        case Format_1306_Horizontal: {
            SSD1306_ConvertHorizontal( Input, Output, Width, Height );
            break;
        }
        case Format_1306_Vertical: {
            printf( "TODO: Not yet done.\n" );
            break;
        }
        case Format_Linear: {
            printf( "TODO: Not yet done.\n" );
            break;
        }
        default: {
            printf( "Unknown output format.\n" );
            break;
        }
    };
}

void ProcessFiles( void ) {
    const char** InputFilenames = NULL;
    const char* OutputFilename = NULL;
    uint8_t* OutputFramebuffer = NULL;
    FIBITMAP* OutputBitmap = NULL;
    FIBITMAP* InputBitmap = NULL;
    int InputFileCount = 0;
    int FramesWritten = 0;
    int InputWidth = 0;
    int InputHeight = 0;
    int OutputWidth = 0;
    int OutputHeight = 0;   
    bool Errors = false;
    int i = 0;

    InputFilenames = CmdLine_GetInputFilenames( );
    OutputFilename = CmdLine_GetOutputFilename( );
    InputFileCount = CmdLine_GetInputCount( );

    NullCheck( InputFilenames, return );
    NullCheck( OutputFilename, return );

    for ( i = 0; i < InputFileCount; i++ ) {
        /* Make sure we successfully open the input image, if we don't then just bail immediately */
        if ( ( InputBitmap = OpenInputImage( InputFilenames[ i ], &InputWidth, &InputHeight ) ) == NULL ) {
            printf( "Failed to open image %s\n", InputFilenames[ i ] );

            Errors = true;
            break;
        }

        /* Small setup bits at the start */
        if ( i == 0 ) {
            SetOutputParameters( InputWidth, InputHeight );

            OutputWidth = InputWidth;
            OutputHeight = InputHeight;

            /* Ditto */
            if ( OpenOutputFile( ) == false ) {
                if ( DidUserCancel( ) == false ) {
                    printf( "Failed to open output file: %s\n", strerror( errno ) );
                    Errors = true;
                }
                
                break;
            }     

            /* RAW And ANM modes require working on a 1bpp framebuffer so we need
             * to allocate one of the proper size ourselves here.
             */
            if ( ( OutputFramebuffer = ( uint8_t* ) malloc( ( InputWidth * InputHeight ) / 8 ) ) == NULL ) {
                printf( "Failed to allocate an output framebuffer.\n" );

                Errors = true;
                break;
            }       
        }

        /* If the current image is not the same size as the first image then write a warning
         * and move onto the next image.
         */
        if ( InputWidth != OutputWidth || InputHeight != OutputHeight ) {
            printf( "Image %s has a size of %dx%d when we expected %dx%d. Skipping.\n", 
                InputFilenames[ i ],
                InputWidth,
                InputHeight,
                OutputWidth,
                OutputHeight
            );

            Errors = true;
            continue;
        }

        /* This really should never fail, but if it does try to keep going anyway */
        if ( ( OutputBitmap = GetProcessedOutput( InputBitmap ) ) == NULL ) {
            printf( "Failed to convert image %s. Skipping.\n", InputFilenames[ i ] );

            Errors = true;
            continue;
        }

        if ( IsOutputAGIF( ) == false ) {
            /* Do special format conversions for RAW/ANM output */
            DoOutputConversion( OutputBitmap, OutputFramebuffer, OutputWidth, OutputHeight );
            WriteOutputFile( OutputFramebuffer );
        } else {
            /* Straight through for GIFs */
            WriteOutputFile( ( void* ) OutputBitmap );
        }

        FreeImage_Unload( InputBitmap );
        FreeImage_Unload( OutputBitmap );

        FramesWritten++;
    }

    printf( "Processed %d of %d input images.\n", FramesWritten, InputFileCount );
    
    if ( Errors == true ) {
        printf( "There were errors during the conversion.\nOutput file may be incomplete or invalid.\n" );
    }

    if ( OutputFramebuffer != NULL ) {
        free( OutputFramebuffer );
    }

    CloseOutputFile( );
}

int main( int Argc, char** Argv ) {
    FreeImage_Initialise( FALSE );
    FreeImage_SetOutputMessage( ErrorHandler );

    if ( CmdLine_Handler( Argc, Argv ) == 0 ) {
        ProcessFiles( );
        CmdLine_Free( );
    }

    FreeImage_DeInitialise( );
    return 0;
}

