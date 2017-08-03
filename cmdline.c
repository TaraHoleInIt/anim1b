/*
 * anim1b:
 * Image to ssd1306 image converter.
 * 2017 - Tara Keeling
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <FreeImage.h>
#include <argp.h>

static FREE_IMAGE_DITHER ParseDither( const char* DitherText );
static error_t ParseArgs( int Key, char* Arg, struct argp_state* State );

static FREE_IMAGE_DITHER DitherAlgorithm = FID_FS;
static int DitherFlag = 0;
static int InvertFlag = 0;
static int ThresholdValue = 128;

static struct argp_option Options[ ] = {
    { "dither", 'd', "algorithm", OPTION_ARG_OPTIONAL, "Dither output" },
    { "threshold", 't', "value", 0, "Threshold for non dithered output [0-255]" },
    { "invert", 'i', 0, "Invert output" },
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

static char ArgsDocumentation[ ] = "input output";

static struct argp P = {
    Options, ParseArgs, ArgsDocumentation, Documentation
};

static char** Filenames = NULL;
static int FilenameCount = 0;

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

static error_t ParseArgs( int Key, char* Arg, struct argp_state* State ) {
    int Value = 0;

    switch ( Key ) {
        case 'd': {
            DitherAlgorithm = ( Arg == NULL ? FID_FS : ParseDither( Arg ) );
            DitherFlag = 1;

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
            InvertFlag = 1;
            break;
        }
        case ARGP_KEY_ARG: {
            if ( Arg ) {
                Filenames[ FilenameCount ] = ( char* ) malloc( strlen( Arg ) + 1 );

                if ( Filenames[ FilenameCount ] != NULL ) {
                    memset( Filenames[ FilenameCount ], 0, strlen( Arg ) + 1 );
                    strncpy( Filenames[ FilenameCount ], Arg, strlen( Arg ) );
                    FilenameCount++;
                }
            }

            break;
        }
        case ARGP_KEY_END: {
            if ( State->arg_num < 2 )
                argp_usage( State );

            break;
        }
        default: return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

FREE_IMAGE_DITHER CMDLine_GetDitherAlgorithm( void ) {
    return DitherAlgorithm;
}

int CMDLine_DitherEnabled( void ) {
    return DitherFlag;
}

int CMDLine_GetColorThreshold( void ) {
    return ThresholdValue;
}

const char** CMDLine_GetInputFilenames( void ) {
    return ( const char** ) Filenames;
}

const char* CMDLine_GetOutputFilename( void ) {
    return ( Filenames && Filenames[ FilenameCount - 1 ] ) ? Filenames[ FilenameCount - 1 ] : NULL;
}

int CMDLine_GetInputCount( void ) {
    return FilenameCount > 1 ? FilenameCount - 1 : 1;
}

int CMDLine_Handler( int Argc, char** Argv ) {
    Filenames = ( char** ) malloc( sizeof( char* ) * Argc );

    if ( Filenames ) {
        memset( Filenames, 0, sizeof( char* ) * Argc );
        return argp_parse( &P, Argc, Argv, ARGP_IN_ORDER, 0, NULL );
    }

    return 1;
}

void CMDLine_Free( void ) {
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
