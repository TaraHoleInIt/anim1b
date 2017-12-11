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
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <FreeImage.h>
#include "output.h"
#include "cmdline.h"

static void AddFrameTimeTag( FIBITMAP* Input, uint32_t AnimationDelay );

static bool OpenGIFOutput( void );
static void CloseGIFOutput( void );
static bool AddGIFFrame( FIBITMAP* Input, uint32_t AnimationDelay );

static bool OpenRawOutput( void );
static void CloseRawOutput( void );
static bool AddRawFrame( uint8_t* Data );

static bool OpenANMOutput( void );
static bool AddANMFrame( uint8_t* Data );
static void CloseANMOutput( void );

static const char* DitherAlgorithms[ ] = {
    "Floyd-Steinberg",
    "Bayer 4x4",
    "Bayer 8x8",
    "Cluster 6x6",
    "Cluster 8x8",
    "Cluster 16x16",
    "Bayer16x16"
};

static FIMULTIBITMAP* OutputGIF = NULL;
static FILE* OutputFile = NULL;

static int OutputWidth = 0;
static int OutputHeight = 0;
static int OutputFormat = 0;
static int FramesWritten = 0;

void SetOutputParameters( int Width, int Height ) {
    OutputWidth = Width;
    OutputHeight = Height;
}

/*
 * Allocates space in memory for the output image and
 * returns a pointer to it.
 */
uint8_t* AllocateFramebuffer( int Width, int Height ) {
    int DisplaySize = ( Width * Height ) / 8;
    uint8_t* Result = NULL;

    if ( ( Result = ( ( uint8_t* ) malloc( DisplaySize ) ) ) != NULL ) {
        memset( Result, 0, DisplaySize );
    }

    return Result;
}

/*
 * Prompts the user if we should overwrite (Filename).
 * Returns 1 if the user selected Y, otherwise 0.
 */
static bool AskToOverwrite( const char* Filename ) {
    int Result = 0;

    printf( "File \"%s\" already exists. Overwrite? (Y/N) ", Filename );
        Result = tolower( getchar( ) );
    printf( "\n" );

    return ( Result == 'y' ) ? true : false;
}

bool OpenGIFOutput( void ) {
    const char* Filename = NULL;

    if ( ( Filename = CmdLine_GetOutputFilename( ) ) != NULL ) {
        /* If the file exists and the user does not want to overwrite it, bail */
        if ( access( Filename, F_OK ) == 0 && AskToOverwrite( Filename ) == 0 )
            return false;

        OutputGIF = FreeImage_OpenMultiBitmap( FIF_GIF, Filename, TRUE, FALSE, FALSE, 0 );
    }

    return OutputGIF != NULL ? true : false;
}

void CloseGIFOutput( void ) {
    if ( OutputGIF != NULL ) {
        FreeImage_CloseMultiBitmap( OutputGIF, 0 );
        OutputGIF = NULL;
    }
}

bool OpenRawOutput( void ) {
    const char* Filename = NULL;

    if ( ( Filename = CmdLine_GetOutputFilename( ) ) != NULL ) {
        /* If the file exists and the user does not want to overwrite it, bail */
        if ( access( Filename, F_OK ) == 0 && AskToOverwrite( Filename ) == 0 )
            return false;
    }

    OutputFormat = CmdLine_GetOutputFormat( );
    OutputFile = fopen( Filename, "wb+" );

    return OutputFile != NULL ? true : false;
}

void CloseRawOutput( void ) {
    if ( OutputFile != NULL ) {
        fflush( OutputFile );
        fclose( OutputFile );

        OutputFile = NULL;
    }
}

static void AddFrameTimeTag( FIBITMAP* Input, uint32_t AnimationDelay ) {
    FITAG* Tag = NULL;

    if ( ( Tag = FreeImage_CreateTag( ) ) != NULL ) {
        FreeImage_SetTagKey( Tag, "FrameTime" );
        FreeImage_SetTagType( Tag, FIDT_LONG );
        FreeImage_SetTagCount( Tag, 1 );
        FreeImage_SetTagLength( Tag, 4 );
        FreeImage_SetTagValue( Tag, &AnimationDelay );
        FreeImage_SetMetadata( FIMD_ANIMATION, Input, FreeImage_GetTagKey( Tag ), Tag );
        FreeImage_DeleteTag( Tag );
    }
}

bool IsOutputAGIF( void ) {
    const char* OutputFile = NULL;
    int Length = 0;

    if ( ( OutputFile = CmdLine_GetOutputFilename( ) ) != NULL ) {
        Length = strlen( OutputFile );

        if ( strcasecmp( &OutputFile[ Length - 4 ], ".gif" ) == 0 )
            return true;
    }

    return false;
}

bool IsOutputANM( void ) {
    const char* Filename = NULL;
    int Length = 0;

    if ( ( Filename = CmdLine_GetOutputFilename( ) ) != NULL ) {
        Length = strlen( Filename );

        if ( strcasecmp( &Filename[ Length - 4 ], ".anm" ) == 0 ) {
            return true;
        }
    }

    return false;
}

bool AddRawFrame( uint8_t* Data ) {
    int DataSize = ( OutputWidth * OutputHeight ) / 8;

    NullCheck( OutputFile, return false );
    NullCheck( Data, return false );

    if ( fwrite( Data, 1, DataSize, OutputFile ) == DataSize ) {
        FramesWritten++;
        return true;
    }

    return false;
}

bool AddGIFFrame( FIBITMAP* Input, uint32_t AnimationDelay ) {
    NullCheck( OutputGIF, return false );
    NullCheck( Input, return false );

    AddFrameTimeTag( Input, AnimationDelay );
    FreeImage_AppendPage( OutputGIF, Input );

    return true;
}

bool OpenANMOutput( void ) {
    struct ANM0_Header Header;

    if ( OpenRawOutput( ) == true ) {
        memset( &Header, 0, sizeof( struct ANM0_Header ) );
        
        if ( fwrite( &Header, 1, sizeof( struct ANM0_Header ), OutputFile ) == sizeof( struct ANM0_Header ) ) {
            return true;
        }
    }

    return false;
}

bool AddANMFrame( uint8_t* Data ) {
    return AddRawFrame( Data );
}

#define MakeWord( a, b, c, d ) ( \
    ( d << 24 ) | \
    ( c << 16 ) | \
    ( b << 8 ) | \
    ( a ) \
)

void CloseANMOutput( void ) {
    struct ANM0_Header Header;

    NullCheck( OutputFile, return );

    fseek( OutputFile, 0, SEEK_SET );
        fread( &Header, sizeof( struct ANM0_Header ), 1, OutputFile );
    fseek( OutputFile, 0, SEEK_SET );

    Header.ANMId = MakeWord( 'A', 'N', 'M', '0' );
    Header.AddressMode = ( uint8_t ) OutputFormat;
    Header.CompressionType = 0;
    Header.FrameCount = ( uint16_t ) FramesWritten;
    Header.DelayBetweenFrames = ( uint16_t ) CmdLine_GetOutputDelay( );
    Header.Width = ( uint16_t ) OutputWidth;
    Header.Height = ( uint16_t ) OutputHeight;
    Header.Reserved = 0;

    fwrite( &Header, 1, sizeof( struct ANM0_Header ), OutputFile );
    CloseRawOutput( );
}

bool OpenOutputFile( void ) {
    if ( IsOutputAGIF( ) == true ) {
        return OpenGIFOutput( );
    } else if ( IsOutputANM( ) == true ) {
        return OpenANMOutput( );
    } else {
    }

    return OpenRawOutput( );
}

void CloseOutputFile( void ) {
    if ( IsOutputAGIF( ) == true ) {
        CloseGIFOutput( );
    } else if ( IsOutputANM( ) == true ) {
        CloseANMOutput( );
    } else {
        CloseRawOutput( );
    }
}

bool WriteOutputFile( void* Data ) {
    if ( IsOutputAGIF( ) == true ) {
        return AddGIFFrame( ( FIBITMAP* ) Data, CmdLine_GetOutputDelay( ) );
    } else if ( IsOutputANM( ) == true ) {
        /* TODO:
         * Later revisions may add an individual frame header?
         */
        return AddRawFrame( ( uint8_t* ) Data );
    } else {
    }

    return AddRawFrame( ( uint8_t* ) Data );
}
