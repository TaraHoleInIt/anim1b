#ifndef _CMDLINE_H_
#define _CMDLINE_H_

FREE_IMAGE_DITHER CmdLine_GetDitherAlgorithm( void );
int CmdLine_DitherEnabled( void );
int CmdLine_GetColorThreshold( void );
const char** CmdLine_GetInputFilenames( void );
const char* CmdLine_GetOutputFilename( void );
int CmdLine_GetInputCount( void );
int CmdLine_GetInvertFlag( void );
int CmdLine_GetOutputWidth( void );
int CmdLine_GetOutputHeight( void );
int CmdLine_GetOutputDelay( void );
int CmdLine_GetWriteHeaderFlag( void ); 
int CmdLine_Handler( int Argc, char** Argv );
void CmdLine_Free( void );

#endif
