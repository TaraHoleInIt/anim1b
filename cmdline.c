/**
 * Copyright (c) 2017 Tara Keeling
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <FreeImage.h>
#include <argp.h>
#include "cmdline.h"

#define DEFAULT_IMAGE_DELAY 100

static FREE_IMAGE_DITHER ParseDither( const char* DitherText );
static error_t ParseArgs( int Key, char* Arg, struct argp_state* State );

static FREE_IMAGE_DITHER DitherAlgorithm = FID_FS;
static uint32_t Delay = DEFAULT_IMAGE_DELAY;
static bool ShouldWriteHeader = true;
static bool DitherFlag = false;
static bool InvertFlag = false;
static bool ThresholdValue = 128;

static struct argp_option Options[ ] = {
    { "dither", 'd', "algorithm", OPTION_ARG_OPTIONAL, "Dither output" },
    { "threshold", 't', "value", 0, "Threshold for non dithered output [0-255]" },
    { "invert", 'i', NULL, 0, "Invert output" },
    { "delay", 'l', "delay", 0, "Delay between frames in milliseconds" },
    { "noheader", 'n', NULL, 0, "Do not write header, only write raw frames" },
    { NULL }
};

const char* argp_program_version = "0.0.1";
static char Documentation[ ] = "anim1b: Image to SSD1306 converter" \
    "\v" \
    "Supported dithering algorithms: \n" \
    "  fs       Floyd-Steinberg\n" \
    "  b4x4     Bayer 4x4\n" \
    "  b8x8     Bayer 8x8\n" \
    "  b16x16   Bayer 16x16\n" \
    "  c6x6     Cluster 6x6\n" \
    "  c8x8     Cluster 8x8\n" \
    "  c16x16   Cluster 16x16\n\n" \
;

static char ArgsDocumentation[ ] = "images output";

static struct argp P = {
    Options, ParseArgs, ArgsDocumentation, Documentation
};

static char** Filenames = NULL;
static int FilenameCount = 0;

/*
 * ParseDither:
 *
 * Returns a FREE_IMAGE_DITHER type from the given string.
 */
static FREE_IMAGE_DITHER ParseDither( const char* DitherText ) {
    FREE_IMAGE_DITHER Result = FID_FS;

    if ( DitherText ) {
        if ( strcasecmp( DitherText, "fs" ) == 0 )
            Result = FID_FS;
        else if ( strcasecmp( DitherText, "b4x4" ) == 0 )
            Result = FID_BAYER4x4;
        else if ( strcasecmp( DitherText, "b8x8" ) == 0 )
            Result = FID_BAYER8x8;
        else if ( strcasecmp( DitherText, "b16x16" ) == 0 )
            Result = FID_BAYER16x16;
        else if ( strcasecmp( DitherText, "c6x6" ) == 0 )
            Result = FID_CLUSTER6x6;
        else if ( strcasecmp( DitherText, "c8x8" ) == 0 )
            Result = FID_CLUSTER8x8;
        else if ( strcasecmp( DitherText, "c16x16" ) == 0 )
            Result = FID_CLUSTER16x16;
        else
            Result = -1;
    }

    return Result;
}

char* AddInputFile( const char* Filename ) {
    char* Result = NULL;
    int Len = 0;

    if ( Filename && ( Len = strlen( Filename ) ) > 0 ) {
        if ( ( Result = ( char* ) malloc( Len + 1 ) ) != NULL ) {
            memset( Result, 0, Len + 1 );
            strncpy( Result, Filename, Len );

            Filenames[ FilenameCount++ ] = Result;
        }
    }

    return Result;
}

/*
 * ParseArgs:
 *
 * Callback for argp command line parsing.
 */
static error_t ParseArgs( int Key, char* Arg, struct argp_state* State ) {
    int Value = 0;

    switch ( Key ) {
        case 'd': {
            DitherAlgorithm = ParseDither( Arg );
            DitherFlag = true;

            if ( DitherAlgorithm == -1 )
                argp_error( State, "Unknown dithering algorithm: \"%s\"\n", Arg == NULL ? "" : Arg );

            break;
        }
        case 't': {
            Value = atoi( Arg );

            if ( Value < 0 || Value > 255 )
                argp_error( State, "Threshold out of range, expected 0-255 got %d", Value );

            ThresholdValue = Value;
            break;
        }
        case 'i': {
            InvertFlag = true;
            break;
        }
        case 'n': {
            ShouldWriteHeader = 0;
            break;
        }
        case 'l': {
            Delay = atoi( Arg );
            break;
        }
        case ARGP_KEY_ARG: {
            /* Add another input file to the list */
            AddInputFile( Arg );
            break;
        }
        case ARGP_KEY_END: {
            /* Bail unless we at least have one input and output */
            if ( State->arg_num < 2 )
                argp_usage( State );

            break;
        }
        default: return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

FREE_IMAGE_DITHER CmdLine_GetDitherAlgorithm( void ) {
    return DitherAlgorithm;
}

bool CmdLine_DitherEnabled( void ) {
    return DitherFlag;
}

int CmdLine_GetColorThreshold( void ) {
    return ThresholdValue;
}

const char** CmdLine_GetInputFilenames( void ) {
    return ( const char** ) Filenames;
}

const char* CmdLine_GetOutputFilename( void ) {
    return ( Filenames && Filenames[ FilenameCount - 1 ] ) ? Filenames[ FilenameCount - 1 ] : NULL;
}

int CmdLine_GetInputCount( void ) {
    return FilenameCount > 1 ? FilenameCount - 1 : 1;
}

bool CmdLine_GetInvertFlag( void ) {
    return InvertFlag;
}

uint32_t CmdLine_GetOutputDelay( void ) {
    return Delay;
}

bool CmdLine_GetWriteHeaderFlag( void ) {
    return ShouldWriteHeader;
}

int CmdLine_Handler( int Argc, char** Argv ) {
    Filenames = ( char** ) malloc( sizeof( char* ) * Argc );

    if ( Filenames ) {
        memset( Filenames, 0, sizeof( char* ) * Argc );
        return argp_parse( &P, Argc, Argv, ARGP_IN_ORDER, 0, NULL );
    }

    return 1;
}

void CmdLine_Free( void ) {
    int i = 0;

    if ( Filenames ) {
        for ( i = 0; i < FilenameCount; i++ ) {
            if ( Filenames[ i ] ) {
                free( Filenames[ i ] );
            }
        }

        free( Filenames );
    }

    FilenameCount = 0;
    Filenames = NULL;
}
