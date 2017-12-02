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
    printf( "File \"%s\" already exists. Overwrite? (Y/N) ", Filename );
    return tolower( getchar( ) ) == 'y' ? true : false;
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

bool AddRawFrame( uint8_t* Data, int Width, int Height ) {
    int DataSize = ( Width * Height ) / 8;

    NullCheck( OutputFile, return false );
    NullCheck( Data, return false );

    return ( fwrite( Data, 1, DataSize, OutputFile ) == DataSize ) ? true : false;
}

bool AddGIFFrame( FIBITMAP* Input, uint32_t AnimationDelay ) {
    NullCheck( OutputGIF, return false );
    NullCheck( Input, return false );

    AddFrameTimeTag( Input, AnimationDelay );
    FreeImage_AppendPage( OutputGIF, Input );

    return true;
}
