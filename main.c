/**
 * Copyright (c) 2017-2018 Tara Keeling
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
    const char* FormatName = ( Fmt != FIF_UNKNOWN ) ? FreeImage_GetFormatFromFIF( Fmt ) : "UNKNOWN";
    fprintf( stderr, "FreeImage [%s]: %s\n", FormatName, Message );
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

            fprintf( stderr, "Error: Input image width and height must be divisible by 8.\n" );

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
void SetPixelHorizontal( uint8_t* Output, int x, int y, int ImageWidth, bool Color ) {
    int PixelOffset = x + ( ( y / 8 ) * ImageWidth );
    int BitOffset = ( y & 0x07 );

    Output[ PixelOffset ] = ( Color == true ) ? Output[ PixelOffset ] | BIT( BitOffset ) : Output[ PixelOffset ] & ~BIT( BitOffset );
}

void SetPixelVertical( uint8_t* Output, int x, int y, int ImageWidth, bool Color ) {
    int BitOffset = ( y & 0x07 );
    int PageOffset = ( y / 8 );
    int PixelOffset = 0;

    ( void ) ImageWidth;

    PixelOffset = ( x * 8 ) + PageOffset;
    Output[ PixelOffset ] = ( Color == true ) ? ( Output[ PixelOffset ] | BIT( BitOffset ) ) : ( Output[ PixelOffset ] & ~BIT( BitOffset ) );
}

void SetPixelLinear( uint8_t* Output, int x, int y, int ImageWidth, bool Color ) {
    int BitOffset = 7 - ( x & 0x07 );
    int PixelOffset = 0;

    PixelOffset = y * ( ImageWidth / 8 );
    PixelOffset+= ( x / 8 );

    Output[ PixelOffset ] = ( Color == true ) ? ( Output[ PixelOffset ] | BIT( BitOffset ) ) : ( Output[ PixelOffset ] & ~BIT( BitOffset ) );
}

void DoOutputConversion( FIBITMAP* Input, uint8_t* Output, int Width, int Height ) {
    void ( *SetPixelFn ) ( uint8_t* Output, int x, int y, int ImageWidth, bool Color ) = NULL;
    uint8_t Color = 0;
    int x = 0;
    int y = 0;

    switch ( CmdLine_GetOutputFormat( ) ) {
        case Format_1306_Horizontal: {
            SetPixelFn = SetPixelHorizontal;
            break;
        }
        case Format_1306_Vertical: {
            SetPixelFn = SetPixelVertical;
            break;
        }
        case Format_Linear: {
            SetPixelFn = SetPixelLinear;
            break;
        }
        default: return;
    };

    for ( x = 0; x < Width; x++ ) {
        for ( y = 0; y < Height; y++ ) {
            FreeImage_GetPixelIndex( Input, x, y, &Color );
            SetPixelFn( Output, x, y, Width, Color );
        }
    }
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
            fprintf( stderr, "Failed to open image %s\n", InputFilenames[ i ] );

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
                    fprintf( stderr, "Failed to open output file: %s\n", strerror( errno ) );
                    Errors = true;
                }
                
                break;
            }     

            /* RAW And ANM modes require working on a 1bpp framebuffer so we need
             * to allocate one of the proper size ourselves here.
             */
            if ( ( OutputFramebuffer = ( uint8_t* ) malloc( ( InputWidth * InputHeight ) / 8 ) ) == NULL ) {
                fprintf( stderr, "Failed to allocate an output framebuffer.\n" );

                Errors = true;
                break;
            }       
        }

        /* If the current image is not the same size as the first image then write a warning
         * and move onto the next image.
         */
        if ( InputWidth != OutputWidth || InputHeight != OutputHeight ) {
            fprintf( stderr, "Image %s has a size of %dx%d when we expected %dx%d. Skipping.\n", 
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
            fprintf( stderr, "Failed to convert image %s. Skipping.\n", InputFilenames[ i ] );

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
        fprintf( stderr, "There were errors during the conversion.\nOutput file may be incomplete or invalid.\n" );
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

