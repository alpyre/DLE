/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 14-04-2017

Graphic related stuff
*/
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/copper.h>
#include <graphics/view.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxnodes.h>
#include <graphics/videocontrol.h>
#include <libraries/dos.h>
#include <utility/tagitem.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics.h"

#define AddMinTail(a, b) AddTail((struct List*) a, (struct Node*) b)

VOID cleanup(struct ScreenInfo*);
VOID fail(STRPTR, struct ScreenInfo*);
ULONG twoToThePowerOf(UBYTE);
VOID bToS(BYTE, UBYTE, STRPTR);
BOOL Exists(char *);
PLANEPTR allocRasterFast(ULONG, ULONG);
VOID freeRasterFast(PLANEPTR, ULONG, ULONG);

struct GfxBase *GfxBase = NULL;

extern APTR mapPool;              // The memPool used for vertices, walls and sectors.
extern struct MinList* textures;  // The global list for all loaded textures.

// NEW COLORS
#define BLACK 0x000 // RGB values for the four colors used.
#define COLOR1 0x666
#define COLOR2 0xAAA
#define COLOR3 0xBBB

UWORD colorTable[] = { BLACK, COLOR1, COLOR2, COLOR3 };

/// CreateScreen()
/*******************************************************************
/*  Creates all the data structures required for a release 1.3     *
 *  compatible double buffered Amiga display, displays it and      *
 *  and returns a custom structure that holds all the required     *
 *  control data.                                                  *
 *******************************************************************/
struct ScreenInfo* CreateScreen(UWORD width, UWORD height, UBYTE depth)
{
  struct ScreenInfo* screenInfo = NULL;
  UBYTE plane;               // a counter to traverse bit planes
  UBYTE buffer;              // a counter to traverse the two buffers (doublebuffer)
  ULONG modeID;
  struct TagItem vcTags[] =
  {
    {VTAG_ATTACH_CM_SET, NULL },
    {VTAG_VIEWPORTEXTRA_SET, NULL },
    {VTAG_NORMAL_DISP_SET, NULL },
    {VTAG_END_CM, NULL }
  };
  ULONG colors = twoToThePowerOf(depth);
  UBYTE *displaymem = NULL; /* Pointer for writing to BitMap memory. */

  // Create the ScreenInfo structure in memory
  screenInfo = (struct ScreenInfo*) AllocMem(sizeof(struct ScreenInfo), MEMF_CLEAR);
  if (screenInfo)
  { /* Allocate the required structures */
    screenInfo->view = (struct View*) AllocMem(sizeof(struct View), MEMF_CLEAR);
    screenInfo->viewPort = (struct ViewPort*) AllocMem(sizeof(struct ViewPort), MEMF_CLEAR);
    screenInfo->rasInfo = (struct RasInfo*) AllocMem(sizeof(struct RasInfo), MEMF_CLEAR);
    screenInfo->rastPort = (struct RastPort*) AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
    screenInfo->dimquery = (struct DimensionInfo*) AllocMem(sizeof(struct DimensionInfo), MEMF_CLEAR);
    /* Allocate memory for the two bitMap structures */
    screenInfo->bitMap[0] = (struct BitMap*) AllocMem(sizeof(struct BitMap), MEMF_CLEAR);
    screenInfo->bitMap[1] = (struct BitMap*) AllocMem(sizeof(struct BitMap), MEMF_CLEAR);

    screenInfo->depth = depth;
    screenInfo->width = width;
    screenInfo->height = height;
  }
  if ((!screenInfo) || (!screenInfo->view) || (!screenInfo->viewPort) || (!screenInfo->rasInfo) || (!screenInfo->rastPort) || (!screenInfo->bitMap[0]) || (!screenInfo->bitMap[1]) )
  {
    fail("Not enough memory!\n", screenInfo);
    return(NULL);
  }
  /* Store the intuitions screen assuming Intuition is around. */
  screenInfo->oldView = GfxBase->ActiView; /* Save current View to restore later. */

  InitView(screenInfo->view); /* Initialize the View and set View.Modes. */
  screenInfo->view->Modes = 0x0000; /* This is the old 1.3 way (only LACE counts). */
  if(GfxBase->LibNode.lib_Version >= 36)
  {
    /* Form the ModeID from values in <displayinfo.h> */
    modeID = DEFAULT_MONITOR_ID | LORES_KEY;
    /* Make the ViewExtra structure */
    if( screenInfo->vExtra = GfxNew(VIEW_EXTRA_TYPE) )
    {
      /* Attach the ViewExtra to the View */
      GfxAssociate(screenInfo->view , screenInfo->vExtra);
      /* Create and attach a MonitorSpec to the ViewExtra */
      if( !(screenInfo->vExtra->Monitor = OpenMonitor(NULL, modeID)) )
      {
        fail("Could not get MonitorSpec\n", screenInfo);
        return(NULL);
      }
    }
    else
    {
      fail("Could not get ViewExtra\n", screenInfo);
      return(NULL);
    }
  }

  /* Initialize the BitMap structures for RasInfo. */
  InitBitMap(screenInfo->bitMap[0], depth, width, height);
  InitBitMap(screenInfo->bitMap[1], depth, width, height);

  /* Allocate space for BitMaps. */
  for (buffer=0; buffer<2; buffer++)
  {
    for (plane=0; plane<depth; plane++)
    {
      screenInfo->bitMap[buffer]->Planes[plane] = (PLANEPTR) AllocRaster(width, height);
      if (screenInfo->bitMap[buffer]->Planes[plane] == NULL)
      {
        fail("Could not get BitPlanes\n", screenInfo);
        return(NULL);
      }
    }
  }

  // Create the rastPort
  screenInfo->rastPort = (struct RastPort*) AllocMem(sizeof(struct RastPort), MEMF_CLEAR);
  if (screenInfo->rastPort)
  {
    InitRastPort(screenInfo->rastPort);
    screenInfo->rastPort->BitMap = screenInfo->bitMap[0];
    screenInfo->rastPort->DrawMode = JAM1;
    SetAPen(screenInfo->rastPort, 1);
  }
  else
  {
    fail("Could not create RastPort\n", screenInfo);
    return(NULL);
  }

  InitVPort(screenInfo->viewPort); /* Initialize the ViewPort. */
  /* Set the display mode the old-fashioned way */
  screenInfo->viewPort->Modes = 0x0000;
  /* And the new-fashioned way */
  if(GfxBase->LibNode.lib_Version >= 36)
  {
    /* Make a ViewPortExtra and get ready to attach it */
    if( screenInfo->vpExtra = GfxNew(VIEWPORT_EXTRA_TYPE) )
    {
      vcTags[1].ti_Data = (ULONG) screenInfo->vpExtra;
      /* Initialize the DisplayClip field of the ViewPortExtra */
      if( GetDisplayInfoData( NULL, (UBYTE *) screenInfo->dimquery, sizeof(struct DimensionInfo), DTAG_DIMS, modeID) )
      {
        screenInfo->vpExtra->DisplayClip = screenInfo->dimquery->Nominal;

        /* Make a DisplayInfo and get ready to attach it */
        if( !(vcTags[2].ti_Data = (ULONG) FindDisplayInfo(modeID)) )
        {
          fail("Could not get DisplayInfo\n", screenInfo);
          return(NULL);
        }
      }
      else
      {
        fail("Could not get DimensionInfo \n", screenInfo);
        return(NULL);
      }
    }
    else
    {
      fail("Could not get ViewPortExtra\n", screenInfo);
      return(NULL);
    }

    /* This is for backwards compatibility with, for example, */
    /* a 1.3 screen saver utility that looks at the Modes field */
    screenInfo->viewPort->Modes = (UWORD) (modeID & 0x0000ffff);
  }

  /* Initialize the ColorMap. */
  screenInfo->colorMap = GetColorMap(colors);
  if(screenInfo->colorMap == NULL)
  {
    fail("Could not get ColorMap\n", screenInfo);
    return(NULL);
  }

  if(GfxBase->LibNode.lib_Version >= 36)
  {
    /* Get ready to attach the ColorMap, Release 2-style */
    vcTags[0].ti_Data = (ULONG) screenInfo->viewPort;
    /* Attach the color map and Release 2 extended structures */
    if( VideoControl(screenInfo->colorMap, vcTags) )
    {
      fail("Could not attach extended structures\n", screenInfo);
      return(NULL);
    }
  }
  else
    /* Attach the ColorMap, old 1.3-style */
    screenInfo->viewPort->ColorMap = screenInfo->colorMap;

  screenInfo->viewPort->RasInfo = screenInfo->rasInfo;
  screenInfo->viewPort->DWidth = width;
  screenInfo->viewPort->DHeight = height;
  LoadRGB4(screenInfo->viewPort, colorTable, colors); /* Change colors to those in colortable. */
  screenInfo->view->ViewPort = screenInfo->viewPort; /* Link the ViewPort into the View. */

  /* Initialize the RasInfo. */
  screenInfo->rasInfo->RxOffset = 0;
  screenInfo->rasInfo->RyOffset = 0;
  screenInfo->rasInfo->Next = NULL;

  /* Clear the ViewPort */
  for (buffer=0; buffer<2; buffer++)
  {
    for(plane=0; plane<depth; plane++)
    {
      displaymem = (UBYTE *) screenInfo->bitMap[buffer]->Planes[plane];
      BltClear(displaymem, (screenInfo->bitMap[buffer]->BytesPerRow * screenInfo->bitMap[buffer]->Rows), 1L);
    }
  }

  for (buffer=0; buffer<2; buffer++)
  {
    screenInfo->rasInfo->BitMap = screenInfo->bitMap[buffer];

    MakeVPort(screenInfo->view, screenInfo->viewPort); /* Construct preliminary Copper instruction list. */
    /* Merge preliminary lists into a real Copper list in the View structure. */
    MrgCop( screenInfo->view );

    //WaitTOF();
    LoadView(screenInfo->view);

    screenInfo->LOFcprList[buffer] = screenInfo->view->LOFCprList;
    screenInfo->SHFcprList[buffer] = screenInfo->view->SHFCprList;
    if (buffer == 0)
    {
      WaitTOF();
      screenInfo->view->LOFCprList = NULL;
      screenInfo->view->SHFCprList = NULL;
    }
  }
  screenInfo->currentBuffer = 1;

return(screenInfo);
}
///
/// DisposeScreen()
/*******************************************************************
 * Closes the screen created by CreateScreen(), frees all the      *
 * allocations made by it and recovers the previous view (probably *
 * of the intuition) if there was any...                           *
 *******************************************************************/
VOID DisposeScreen(struct ScreenInfo* si)
{
  UBYTE buffer;

  if (!si) return;

  LoadView(si->oldView); /* Put back the old View. */
  WaitTOF(); /* Wait until the the View is being */
             /* rendered before freeing it. */

  for (buffer=0; buffer<2; buffer++)
  {
    FreeCprList(si->LOFcprList[buffer]);   /* Deallocate the hardware Copper list */
    if(si->SHFcprList[buffer])             /* created by MrgCop(). Since this */
      FreeCprList(si->SHFcprList[buffer]); /* is interlace, also check for a */
                                           /* short frame copper list to free. */
  }
  FreeVPortCopLists(si->viewPort); /* Free all intermediate Copper lists */
                                   /* from created by MakeVPort(). */
  cleanup(si);
}
///
/// fail()
/*************************************************************
* Called when an allocation fails in CreateScreen().         *
* Prints the error string and calls cleanup() to free other  *
* allocations made so far...                                 *
**************************************************************/
void fail(STRPTR errorstring, struct ScreenInfo* si)
{
  printf(errorstring);
  cleanup(si);
}
///
/// cleanup()
/*********************************************************
* Frees everything that was allocated in CreateScreen(). *
**********************************************************/
VOID cleanup(struct ScreenInfo* si)
{
  UBYTE plane;
  UBYTE buffer;

  if (si)
  {
    /* Free the color map created by GetColorMap(). */
    if(si->colorMap) FreeColorMap(si->colorMap);
    /* Free the ViewPortExtra created by GfxNew() */
    if(si->vpExtra) GfxFree(si->vpExtra);

    /* Free the BitPlanes drawing area. */
    for (buffer=0; buffer<2; buffer++)
    {
      for(plane=0; plane<si->depth; plane++)
      {
        if (si->bitMap[buffer]->Planes[plane])
        FreeRaster(si->bitMap[buffer]->Planes[plane], si->width, si->height);
      }
      FreeMem(si->bitMap[buffer], sizeof(struct BitMap));
    }

    /* Free the MonitorSpec created with OpenMonitor() */
    if(si->vExtra->Monitor) CloseMonitor( si->vExtra->Monitor );
    /* Free the ViewExtra created with GfxNew() */
    if(si->vExtra) GfxFree(si->vExtra);

    if (si->rasInfo)  FreeMem(si->rasInfo,  sizeof(struct RasInfo));
    if (si->view)     FreeMem(si->view,     sizeof(struct View));
    if (si->viewPort) FreeMem(si->viewPort, sizeof(struct ViewPort));
    if (si->rastPort) FreeMem(si->rastPort, sizeof(struct RastPort));
    if (si->dimquery) FreeMem(si->dimquery, sizeof(struct DimensionInfo));
    FreeMem(si, sizeof(struct ScreenInfo));
  }
}
///
/// twoToThePowerOf()
ULONG twoToThePowerOf(UBYTE x)
{
  ULONG result = 1;
  UBYTE i;

  for (i=x; i>0; i--)
  {
    result = result * 2;
  }
return result;
}
///

/// ScreenSwap()
/*Should be inlined for speed*/
/*******************************************************
 * Swaps the two buffers of the doublebuffered screen. *
 * While one buffer is shown, always the other one is  *
 * attached to the rastPort for drawing operations.    *
 *******************************************************/
__inline VOID ScreenSwap(struct ScreenInfo *si)
{
  si->rastPort->BitMap = si->bitMap[si->currentBuffer];
  si->currentBuffer ^=1;
  //WaitTOF();  // In Amiga Reference manual WaitTOF was recommended to be called here.
                // but in fact it didn't work well here. So I moved it below LoadView();
  si->view->LOFCprList = si->LOFcprList[si->currentBuffer];
  si->view->SHFCprList = si->SHFcprList[si->currentBuffer];
  LoadView(si->view);
  WaitTOF();
}
///

/// loadWallTextures()
/***********************************************
 * Load all texture files in PROGDIR to memory *
 ***********************************************/
BOOL loadWallTextures()
{
  struct BitMap *bitMap = NULL;
  struct Texture *texture = NULL;

  BOOL done = FALSE;
  BOOL error = FALSE;
  STRPTR fileName = "WallTexture  .iff";
  STRPTR fileNum  = fileName + 11;
  UBYTE numLength = 2;
  UBYTE   i = 1;

  /* Open the graphics library */
  GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 33L);
  if(GfxBase == NULL)
  {
    puts("Could not open graphics library\n");
    return(FALSE);
  }

  // Allocate and initialize the list struct
  if (textures)
  {
    // Read all wall textures from the directory
    while (!done)
    {
      bToS(i, numLength, fileNum);

      if (Exists(fileName))
      {
        bitMap = loadILBMTexture(fileName);

        //create a texture node
        if (bitMap)
        {
          texture = AllocPooled(mapPool, sizeof(struct Texture));
          texture->ID = i;
          texture->bitMap = bitMap;

          AddMinTail(textures, &texture->node);
        }
        else { error = TRUE; break; }
      }
      else done = TRUE;

      i++;
    }
  }
  else
  {
    puts("Textures list not allocated");
    return (FALSE);
  }

  if (error)
  {
    freeWallTextures();
    return (FALSE);
  }
  else
    return (TRUE);
}
///
/// freeWallTextures()
/*************************
 * Does what it says! :) *
 *************************/
VOID freeWallTextures(VOID)
{
  struct Texture *texture;

  if (textures)
  {
    for ( texture = (struct Texture*) textures->mlh_Head; texture->node.mln_Succ; texture = (struct Texture*) texture->node.mln_Succ )
    {
      if (texture->bitMap) freeTexture(texture->bitMap);
    }
  }

  /* Close the graphics library */
  CloseLibrary((struct Library *)GfxBase);
  GfxBase = NULL;

}
///
/// loadILBMTexture()
/**********************************************************************
 * Loads the given ILBM texture file (a 48x48 DPaint Brush) to memory *
 * (to fastMem where available)                                       *
 **********************************************************************/
struct BitMap* loadILBMTexture(STRPTR fileName)
{
  struct BitMap* bitMap = NULL;
  LONG ch;
  BPTR fh;
  BYTE idBuf[5];
  LONG i, r, b; // Some loop vars
  LONG plane, row;
  LONG ec = 2; // error code for the evaluation loop (set to "Bad file!" as default)
  BYTE* dispMem;
  BYTE length, height, depth, comp, masking;

  // Error stings to report about the file
  STRPTR errStr[9];
  errStr[0] = "File could not be opened!";
  errStr[1] = "Not an IFF file!";
  errStr[2] = "Bad file!";
  errStr[3] = "Not an ILBM file!";
  errStr[4] = "Bad ILBM file (BitMap Header not found)!";
  errStr[5] = "Bad ILBM file (BitMap BODY not found)!";
  errStr[6] = "Brush too big!";
  errStr[7] = "Unknown ILBM compression type!";
  errStr[8] = "Could not allocate BitMap";

  fh = Open(fileName, MODE_OLDFILE);
  if (!fh) { puts(errStr[0]); return (NULL); }

  //check for file type
  for (i = 0; i<4; i++) idBuf[i] = FGetC(fh);
  idBuf[4] = NULL;
  if (strcmp(idBuf, "FORM")) { ec=1; goto error; }

  // skip 4 bytes (which is IFF file size)
  for (i = 0; i<4; i++) {ch = FGetC(fh); if (ch == -1) goto error;}

  //check for file type again
  for (i = 0; i<4; i++) idBuf[i] = FGetC(fh);
  if (strcmp(idBuf, "ILBM")) { ec=3; goto error; }

  //search for the BitMap Header  (Very dirty coding here :) )
  while(TRUE)
  { ch = FGetC(fh);
    if (ch == -1) { ec=4; goto error; }
    if (ch == 'B')
    { ch = FGetC(fh);
      if (ch == 'M')
      { ch = FGetC(fh);
        if (ch == 'H')
        { ch = FGetC(fh);
          if (ch == 'D') break;
          else if (ch == -1) goto error;
        }
        else if (ch == -1) goto error;
      }
      else if (ch == -1) goto error;
    }
  }

  //Now we are at the BitMap Header start. Skip 4 bytes (which are BMHD size)
  for (i = 0; i<4; i++) {ch = FGetC(fh); if (ch == -1) goto error;}

  //Now the following two words are BitMap sizes.
  ch = FGetC(fh);
  if (ch == -1) goto error;
  if (ch) { ec=6; goto error; }
  ch = FGetC(fh); if (ch == -1) goto error;
  length = ch;
  ch = FGetC(fh);
  if (ch == -1) goto error;
  if (ch) { ec=6; goto error; }
  ch = FGetC(fh); if (ch == -1) goto error;
  height = ch;

  // skip 4 more bytes (which are two words of position coordinates)
  for (i = 0; i<4; i++) {ch = FGetC(fh); if (ch == -1) goto error;}

  //Now the following byte is the depth (number of bitplanes)
  ch = FGetC(fh); if (ch == -1) goto error;
  depth = ch;

  //Following byte is masking type.
  ch = FGetC(fh); if (ch == -1) goto error;
  masking = ch;

  //Following byte is compression type.
  ch = FGetC(fh); if (ch == -1) goto error;
  comp = ch;
  if (comp > 1) { ec=7; goto error; }

  //Now head to the start of BitMap data (id= BODY)
  while(TRUE)
  { ch = FGetC(fh);
    if (ch == -1) { ec=5; goto error; }
    if (ch == 'B')
    { ch = FGetC(fh);
      if (ch == 'O')
      { ch = FGetC(fh);
        if (ch == 'D')
        { ch = FGetC(fh);
          if (ch == 'Y') break;
          else if (ch == -1) goto error;
        }
        else if (ch == -1) goto error;
      }
      else if (ch == -1) goto error;
    }
  }

  //Following 4 bytes are BODY size just skip them
  for (i = 0; i<4; i++) { ch = FGetC(fh); if (ch == -1) goto error;}

  // File seems to be compatible so far. So lets load it to a BitMap.
  // First allocate a BitMap
  bitMap = (struct BitMap*) AllocMem(sizeof(struct BitMap), MEMF_CLEAR);
  if (!bitMap) { ec=8; goto error; }

  InitBitMap(bitMap, depth, length, height);

  for (plane=0; plane<depth; plane++)
  {
    bitMap->Planes[plane] = allocRasterFast(length, height);
    if (bitMap->Planes[plane] == NULL) { ec=8; goto error; }
  }

  //Now load the data from the body according to the compression type.
  if (comp == 0) // uncompressed ILBM. So load raw data directy to bitmap
  {
    for (row = 0; row < bitMap->Rows; row++)
    {
      for (plane=0; plane<depth; plane++)
      {
        dispMem = bitMap->Planes[plane] + (bitMap->BytesPerRow * row); // Pointer arithmetic here
        for (i = 0; i < bitMap->BytesPerRow; i++)
        {
          ch = FGetC(fh); if (ch == -1) goto error;
          *dispMem++ = ch;
        }
      }
      if (masking == 1) //skip mask plane
      {
        for (i = 0; i < bitMap->BytesPerRow; i++)
        {
          ch = FGetC(fh); if (ch == -1) goto error;
        }
      }
    }
  }
  else if (comp == 1) // PackBits compressed ILBM.
  {
    for (row = 0; row < bitMap->Rows; row++)
    {
      for (plane=0; plane<depth; plane++)
      {
        dispMem = bitMap->Planes[plane] + (bitMap->BytesPerRow * row); // Pointer arithmetic here
        r = 0;
        while (r < bitMap->BytesPerRow)
        {
          //read a codeByte
          ch = FGetC(fh); if (ch == -1) goto error;
          //evaluate the codeByte
          if (ch < 128) // following ch+1 bytes are literal bitmap data
          {
            b=ch; // memorize codeByte value.
            for (i=0; i<=b; i++)
            {
              ch = FGetC(fh); if (ch == -1) goto error;
              *dispMem++ = ch; r++;
            }
          }
          else if (ch > 128) // following (257-ch) bytes are the same
          {
            b=ch; // memorize codeByte value.
            ch = FGetC(fh); if (ch == -1) goto error;
            for (i=0; i<(257-b); i++)
            {
              *dispMem++ = ch; r++;
            }
          }
          else if (ch == 128) goto error;
        }
      }
      if (masking == 1) // if mskHasMsk skip the mask plane
      {
        r = 0;
        while (r < bitMap->BytesPerRow)
        {
          //read a codeByte
          ch = FGetC(fh); if (ch == -1) goto error;
          //evaluate the codeByte
          if (ch < 128) // following ch+1 bytes are literal bitmap data
          {
            for (i=0; i<=ch; i++) //
            {
              ch = FGetC(fh); if (ch == -1) goto error;
              r++;
            }
          }
          else if (ch > 128) // following (257-ch) bytes are the same
          {
            ch = FGetC(fh); if (ch == -1) goto error;
            for (i=0; i<(257-ch); i++)
            {
              r++;
            }
          }
          else if (ch == 128) goto error;
        }
      }
    }
  }
  Close(fh);
  return (bitMap);

  error:
  puts(errStr[ec]);
  Close(fh);
  // Cleanup
  if (bitMap) {
    for (plane=0; plane<depth; plane++)
      if (bitMap->Planes[plane]) freeRasterFast(bitMap->Planes[plane], length, height);
    FreeMem(bitMap, sizeof(struct BitMap)); bitMap = NULL;
  }
  return (NULL);
}
///
/// freeTexture()
/*********************************************
 * Frees the bitMap part of a texture struct *
 *********************************************/
VOID freeTexture(struct BitMap* bitMap)
{
  LONG depth = bitMap->Depth;
  LONG plane;

  for (plane=0; plane<depth; plane++)
    if (bitMap->Planes[plane]) freeRasterFast(bitMap->Planes[plane], bitMap->BytesPerRow*8, bitMap->Rows);
  FreeMem(bitMap, sizeof(struct BitMap)); bitMap = NULL;
}
///

/*****************
 * Utility stuff *
 *****************/
/// bToS()
/****************************************************
 * Take a BYTE integer value and convert it to a    *
 * string, preceeding with zeroes.                  *
 * This is used to traverse texture file names.     *
 * Requires an already NULL terminated destStr ptr. *
 ****************************************************/
VOID bToS(BYTE value, UBYTE strLength, STRPTR destStr)
{
  STRPTR curs = destStr;
  UBYTE i;
  UBYTE factor = 1;

  //Calculate the initial factor
  for (i = 1; i<strLength; i++)
  {
    factor *= 10;
  }

  // Fill the string...
  for (i = 0; i<strLength; i++)
  {
    *curs = '0' + (value/factor);
    curs++;
    factor /= 10;
  }
}
///
/// Makeshift Exists() func.
/********************************************
 * Checks if a filename is existent on disk *
 * (used in loadWallTextures())             *
 ********************************************/
BOOL Exists(char *filename)
{
  BPTR lock;
  if(lock = Lock(filename, SHARED_LOCK))
  {
    UnLock(lock);
    return TRUE;
  }
  if(IoErr() == ERROR_OBJECT_IN_USE)
    return TRUE;

return FALSE;
}
///****************************************************************
/// allocRasterFast()
/***********************************************************************
 * A memory allocator similar to AllocRaster(), but allocates          *
 * in FastMEM where available. This is used for storing textures.      *
 * Textures are rendered by CPU so no Chipset operations are done      *
 * on them.                                                            *
 * WARNING: Not suitable for rasters with width non proportional to 8. *
 ***********************************************************************/
PLANEPTR allocRasterFast(ULONG width, ULONG height)
{
  return (PLANEPTR) AllocMem((width/8)*height, NULL); // <- Allocates in FastMEM where available
}
///
/// freeRasterFast()
/*****************************************************
 * The memory freer counterpart of allocRasterFast() *
 *****************************************************/
VOID freeRasterFast(PLANEPTR plane, ULONG width, ULONG height)
{
  FreeMem(plane, (width/8)*height);
}
///
