/*
 * anim1b:
 * Image to ssd1306 image converter.
 * 2017 - Tara Keeling
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <FreeImage.h>
#include "output.h"
#include "cmdline.h"

static int OpenGIFOutput( void );
static int OpenANMOutput( void );
static void AddFrameTimeTag( FIBITMAP* Input );

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
static int IsOutputGIF = 0;
static int FramesWritten = 0;
static int ShouldWriteHeader = 0;
static uint32_t AnimationDelay = 0;
static int Width = 0;
static int Height = 0;
static int SizeInBytes = 0;

/*
 * Prompts the user if we should overwrite (Filename).
 * Returns 1 if the user selected Y, otherwise 0.
 */
static int AskToOverwrite( const char* Filename ) {
    printf( "File \"%s\" already exists. Overwrite? (Y/N) ", Filename );
    return tolower( getchar( ) ) == 'y' ? 1 : 0;
}

static int OpenGIFOutput( void ) {
    const char* Filename = NULL;

    if ( ( Filename = CmdLine_GetOutputFilename( ) ) != NULL ) {
        /* If the file exists and the user does not want to overwrite it, bail */
        if ( access( Filename, F_OK ) == 0 && AskToOverwrite( Filename ) == 0 )
            return EEXIST;

        OutputGIF = FreeImage_OpenMultiBitmap( FIF_GIF, Filename, TRUE, FALSE, FALSE, 0 );
    }

    return OutputGIF != NULL ? 1 : 0;
}

static int OpenANMOutput( void ) {
    const char* Filename = NULL;
    struct ANMHeader Header;

    if ( ( Filename = CmdLine_GetOutputFilename( ) ) != NULL ) {
        /* If the file exists and the user does not want to overwrite it, bail */
        if ( access( Filename, F_OK ) == 0 && AskToOverwrite( Filename ) == 0 )
            return EEXIST;

        OutputFile = fopen( Filename, "wb+" );

        if ( OutputFile && ShouldWriteHeader ) {
            memset( &Header, 0, sizeof( struct ANMHeader ) );

            Header.Width = Width;
            Header.Height = Height;
            Header.DelayBetweenFrames = CmdLine_GetOutputDelay( );

            /* This needs to be updated later when we know how many frames there are */
            Header.Frames = 0xFFFF;

            fwrite( &Header, 1, sizeof( struct ANMHeader ), OutputFile );
        }
    }

    return OutputFile != NULL ? 1 : 0;    
}

int OpenOutput( void ) {
    IsOutputGIF = AreWeWritingAGIF( );
    ShouldWriteHeader = CmdLine_GetWriteHeaderFlag( );
    AnimationDelay = ( uint32_t ) CmdLine_GetOutputDelay( );
    Width = CmdLine_GetOutputWidth( );
    Height = CmdLine_GetOutputHeight( );
    SizeInBytes = ( Width * Height ) / 8;

    return IsOutputGIF ? OpenGIFOutput( ) : OpenANMOutput( );
}

void CloseOutput( void ) {
    struct ANMHeader Header;

    if ( IsOutputGIF ) {
        FreeImage_CloseMultiBitmap( OutputGIF, 0 );
    } else {
        if ( ShouldWriteHeader ) {
            /* Rewind to the beginning of the file and update the header with
             * the number of frames contained in the file.
             */
            fseek( OutputFile, 0, SEEK_SET );
            fread( &Header, 1, sizeof( struct ANMHeader ), OutputFile );

            Header.Frames = FramesWritten;

            fseek( OutputFile, 0, SEEK_SET );
            fwrite( &Header, 1, sizeof( struct ANMHeader ), OutputFile );
        }

        fclose( OutputFile );
    }

    printf( "Stats for \"%s\":\n", CmdLine_GetOutputFilename( ) );
    printf( "Format:\t\t%s\n", IsOutputGIF ? "GIF" : "ANM/BIN" );

    if ( IsOutputGIF ) printf( "Header:\t\tGIF\n" );
    else printf( "Header:\t\t%s\n", ShouldWriteHeader ? "Yes" : "No" );

    printf( "Size:\t\t%dx%d\n", Width, Height );
    printf( "Frames written:\t%d\n", FramesWritten );
    printf( "Inverted?\t%s\n", CmdLine_GetInvertFlag( ) ? "Yes" : "No" );

    if ( CmdLine_DitherEnabled( ) ) printf( "Dithering:\t%s\n", DitherAlgorithms[ CmdLine_GetDitherAlgorithm( ) ] );
    else printf( "Threshold:\t%d\n", CmdLine_GetColorThreshold( ) );

    if ( FramesWritten > 1 )
        printf( "Frame/ms:\t%d\n", AnimationDelay );

    OutputFile = NULL;
    OutputGIF = NULL;
}

static void AddFrameTimeTag( FIBITMAP* Input ) {
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

void WriteFrame( void* Frame ) {
    uint8_t* FramebufferFrame = ( uint8_t* ) Frame;
    FIBITMAP* GIFFrame = ( FIBITMAP* ) Frame;

    if ( IsOutputGIF ) {
        AddFrameTimeTag( GIFFrame );
        FreeImage_AppendPage( OutputGIF, GIFFrame );
    } else {
        fwrite( FramebufferFrame, 1, SizeInBytes, OutputFile );
    }

    FramesWritten++;
}

int AreWeWritingAGIF( void ) {
    const char* OutputFile = NULL;
    int Length = 0;

    if ( ( OutputFile = CmdLine_GetOutputFilename( ) ) != NULL ) {
        Length = strlen( OutputFile );

        if ( strcasecmp( &OutputFile[ Length - 4 ], ".gif" ) == 0 )
            return 1;
    }

    return 0;
}
