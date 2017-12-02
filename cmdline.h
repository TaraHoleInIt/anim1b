#ifndef _CMDLINE_H_
#define _CMDLINE_H_

FREE_IMAGE_DITHER CmdLine_GetDitherAlgorithm( void );
bool CmdLine_DitherEnabled( void );
int CmdLine_GetColorThreshold( void );
const char** CmdLine_GetInputFilenames( void );
const char* CmdLine_GetOutputFilename( void );
int CmdLine_GetInputCount( void );
bool CmdLine_GetInvertFlag( void );
uint32_t CmdLine_GetOutputDelay( void );
bool CmdLine_GetWriteHeaderFlag( void ); 
int CmdLine_Handler( int Argc, char** Argv );
void CmdLine_Free( void );

#endif
