/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 24-04-2017

Keyboard i/o related stuff
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <devices/keyboard.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

#include "keyboard.h"

/*
 * There are keycodes from 0x00 to 0x7F, so the matrix needs to be
 * of 0x80 bits in size, or 0x80/8 which is 0x10 or 16 bytes...
 */
#define MATRIX_SIZE 16L

/*
 * This assembles the matrix for display that translates directly
 * to the RAW key value of the key that is up or down
 */

extern struct Library *SysBase;
struct MsgPort  *KeyMP = NULL;
UBYTE    *keyMatrix = NULL;
BOOL deviceOpened = FALSE;

/// SetKeyboardAccess()
/****************************************************
 * Creates an IORequest to query the keyboard state *
 ****************************************************/
struct IORequest* SetKeyboardAccess(VOID)
{
  struct IOStdReq *KeyIO = NULL;

  if (KeyMP=CreatePort(NULL,NULL))
  {
    if (KeyIO=(struct IOStdReq *)CreateExtIO(KeyMP,sizeof(struct IOStdReq)))
    {
      if (!OpenDevice("keyboard.device",NULL,(struct IORequest *)KeyIO,NULL))
      {
        deviceOpened = TRUE;
        if (keyMatrix=AllocMem(MATRIX_SIZE, MEMF_PUBLIC|MEMF_CLEAR))
        {
          KeyIO->io_Command=KBD_READMATRIX;
          KeyIO->io_Data=(APTR)keyMatrix;
          KeyIO->io_Length= SysBase->lib_Version >= 36 ? MATRIX_SIZE : 13;
          return ((struct IORequest*)KeyIO);
        }
        else
          printf("Error: Could not allocate keymatrix memory\n");
      }
      else
        printf("Error: Could not open keyboard.device\n");
    }
    else
      printf("Error: Could not create I/O request\n");
  }
  else
    printf("Error: Could not create message port\n");

  EndKeyboardAccess((struct IORequest*)KeyIO);
  return NULL;
}
///
/// EndKeyboardAccess()
VOID EndKeyboardAccess(struct IORequest* KeyIO)
{
  if(keyMatrix)
    FreeMem(keyMatrix, MATRIX_SIZE);
  if(deviceOpened)
    CloseDevice(KeyIO);
  if(KeyIO)
    DeleteExtIO(KeyIO);
  if(KeyMP)
    DeletePort(KeyMP);
}
///
