/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 14-04-2017
*/

/* /// "definitions" */
#define PROGRAMNAME     "DoomLikeEngine"
#define VERSION         0
#define REVISION        1
#define VERSIONSTRING   "0.4.8b"

/* define command line syntax and number of options */
#define RDARGS_TEMPLATE ""
#define RDARGS_OPTIONS  0

/* #define or #undef GENERATEWBMAIN to support workbench startup */
#define GENERATEWBMAIN

/* use classic libray syntax under OS4 */
#ifdef __amigaos4__
 #define __USE_INLINE__
#endif

// Program defines
#define WIDTH 320
#define HEIGHT 256
#define DEPTH 2

/* /// */
/* /// "includes" */

/* typical standard headers */

#include <stdio.h>
//#include <stdlib.h>
#include <string.h>   //FOR DEBUGGING PURPOSES AND FPS COUNTER
//#include <ctype.h>
//#include <stdarg.h>

/* typical Amiga headers */
#include <exec/exec.h>
#include <dos/dos.h>
//#include <dos/stdio.h>
//#include <dos/dostags.h>
//#include <dos/dosextens.h>
//#include <dos/datetime.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/layers.h>
//#include <hardware/custom.h>
//#include <hardware/dmabits.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
//#include <workbench/workbench.h>
//#include <workbench/startup.h>
//#include <workbench/icon.h>
//#include <datatypes/pictureclass.h>
//#include <libraries/asl.h>
//#include <libraries/commodities.h>
//#include <libraries/gadtools.h>
//#include <libraries/iffparse.h>
//#include <libraries/locale.h>
//#include <rexx/rxslib.h>
//#include <rexx/storage.h>
//#include <rexx/errors.h>
//#include <utility/hooks.h>
//#include <devices/keyboard.h>

/* typical proto files */
//#include <proto/asl.h>
//#include <proto/commodities.h>
//#include <proto/datatypes.h>
//#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
//#include <proto/gadtools.h>
#include <proto/graphics.h>
//#include <proto/icon.h>
//#include <proto/iffparse.h>
#include <proto/intuition.h>
//#include <proto/layers.h>
//#include <proto/locale.h>
//#include <proto/rexxsyslib.h>
//#include <proto/utility.h>
//#include <proto/wb.h>
//#include <clib/graphics_protos.h>
//#include <clib/exec_protos.h>
//#include <clib/dos_protos.h>
#include <libraries/mathffp.h>
#include <clib/mathffp_protos.h>
#include <clib/mathtrans_protos.h>

/* Program includes */

#include "graphics.h"
#include "keyboard.h"
#include "3D.h"
#include "timer.h"

/* /// */
/* /// "prototypes" */
extern int            main   (int argc, char **argv);
extern int            wbmain (struct WBStartup *wbs);
extern struct Config *Init   (void);
extern int            Main   (struct Config *config);
extern void           CleanUp(struct Config *config);
/* /// */
/* /// "structures" */

/* ---------------------------------- Config -----------------------------------
 Global data for this program
*/

struct Config
{
    struct RDArgs *RDArgs;

    /* values of command line options (the result of ReadArgs())*/
    #if RDARGS_OPTIONS
        LONG Options[RDARGS_OPTIONS];
    #endif

    /* <INSERT YOUR GLOBAL DATA HERE AND DELETE THIS LINE> */
};

/* /// */
/* /// "globals" */

/* version tag */
#if defined(__SASC)
    const UBYTE VersionTag[] = "$VER: " PROGRAMNAME " " VERSIONSTRING " "  __AMIGADATE__ "\n\0";
#elif defined(_DCC)
    const UBYTE VersionTag[] = "$VER: " PROGRAMNAME " " VERSIONSTRING " (" __COMMODORE_DATE__ ")\n\0";
#else
    const UBYTE VersionTag[] = "$VER: " PROGRAMNAME " " VERSIONSTRING " (" __DATE__ ")\n\0";
#endif

#ifdef LATTICE
  int CXBRK(void) { return(0); } /* Disable Lattice CTRL/C handling */
  int chkabort(void) { return(0); } /* really */
#endif

extern struct Library *MathBase;
extern struct Library *MathTransBase;

APTR mapPool = NULL; // The memory pool we will use for the 3D map data
struct MinList* vertices = NULL; // The global list for all defined vertices in map.
struct MinList* walls = NULL;    // The global list for all defined walls in map.
struct MinList* sectors = NULL;  // The global list for all defined sectors in map.
struct MinList* textures = NULL; // The global list for all loaded textures.
///
/* /// "entry points" */

/* ----------------------------------- main ------------------------------------
 Shell/wb entry point
*/

int
main(int argc, char **argv)
{
    int rc = 20;

    /* shell startup (indicated by argc != 0)? */
    if (argc)
    {
        struct Config *config;

        if (config = Init())
        {
            #if RDARGS_OPTIONS
                /* parse startup options */
                if (config->RDArgs = ReadArgs(RDARGS_TEMPLATE, config->Options, NULL))
                    rc = Main(config);
                else
                    PrintFault(IoErr(), PROGRAMNAME);
            #else
                rc = Main(config);
            #endif

            CleanUp(config);
        }
    }
    else
        rc = wbmain((struct WBStartup *)argv);

    return(rc);
}

/* ---------------------------------- wbmain -----------------------------------

 Workbench entry point

*/

int
wbmain(struct WBStartup *wbs)
{
    int rc = 20;

    #ifdef GENERATEWBMAIN

        struct Config *config;

        if (config = Init())
        {
            rc = Main(config);

            CleanUp(config);
        }

    #endif

    return(rc);
}

/* /// */
/* /// "initialize" */
/* ----------------------------------- Init ------------------------------------
 Initialize program. Allocate a config structure to store configuration data.
*/

struct Config * Init()
{
    struct Config *config = NULL;

    if (config = (struct Config *) AllocMem(sizeof(struct Config), MEMF_CLEAR))
    {
        /* <INSERT YOUR INITIALIZATION CODE HERE AND DELETE THIS LINE> */
    }

    return(config);
}

/* /// */
/* /// "main" */
/* ----------------------------------- Main ------------------------------------
 Main program (return codes: 0=ok, 5=warn, 20=fatal)
*/

int
Main(struct Config *config)
{
    int rc = RETURN_OK;
    struct ScreenInfo* si;
    struct IORequest* KeyIO = NULL;
    BYTE *keyMatrix, *qRow, *aRow, *zRow, *escRow, *F1Row;
    LONG plane;
    struct timerequest* timerReq = NULL;
    struct MsgPort*     timerMP  = NULL;

    if (SetUp3D())
    {
      // SET UP SOME VARIABLES:
      struct BitMap* texture;

      LONG pX;            //Coordinates of the player
      LONG pY;            // (multiplied by precision)
      LONG old_pX;
      LONG old_pY;

      BOOL  fullScreen = FALSE;
      WORD  scrOffset  = 100;
      WORD  scrSize    = 100;
      WORD  scrSizeP1  = scrSize+1;
      WORD  scrHalf    = scrSize/2;
      WORD  scrCenter  = scrOffset + scrHalf;
      WORD  scrRows    = scrHalf + 1; //Rows to clear with BlitClear()
      BYTE  viewDist   = 1;  // Viewers distance from screen.
      BYTE  wallHeight = 10; // Height of the wall (actually half of its height)
      BYTE  scale      = 60; // Scale image up to fill the screen
      BYTE  angle = 0;       // Angle of the player (0-71) 72 values each representing 5 degrees.
      LONG  HDS        = viewDist * wallHeight * scale * PRC;
      LONG  portalLeft;
      LONG  portalRight;
      WORD  DS         = viewDist * scale;
      BOOL  skipThisPortal;
      WORD  childPortals = 0;
      struct PortalMem portalMem[10];
      struct PortalMem *portalMemCurrent = portalMem;

      LONG trig[19];
      LONG angleCos;
      LONG angleSin;
      WORD angleMod;
      struct Vector east;
      struct Vector south;

      struct Sector* currSector = NULL; // The sector player is currently in (NULL means none)
      struct Sector* old_Sector = NULL; // Memorize the previous sector player was in
      struct wList* wL = NULL;          // The pointer to traverse sector walls.
      struct Wall*  w  = NULL;
      struct PlayerData* pD = NULL;     // The pointer returned by LoadMap()

      LONG tD;                 // Temporary Y value buffer while transforming coords.
      LONG TXG, TYG;           // The (G)IVEN transformed coords for the first vertex of each wall
      LONG TX1, TY1, TX2, TY2; // Temporary value buffers for transformed coords.
      LONG X1, Y1B, X2, Y2B;   // Screen coords for wall vertices (B means Bottom)
      LONG XG, YG;             // The (G)IVEN screen coords for the first vertex.
      LONG dH, DH, DH2; // explanation below
      LONG XS, XE, X;   // XStart, XEnd (for vertical lines) and the iterator
      LONG YS, YE, Y;   // YStart, YEnd (for vertical lines) and the iterator
      BOOL clipped = FALSE;
      enum {ATSTEP1 = 1 , ATSTEP2 = 2};
      WORD viewComplete;

      // Texture Mapping variables
      LONG wH;                        // Drawn wall height at a certain wall X.
      LONG wHs;                       // Wall height at start edge
      LONG tHeight = DEFAULTTEXTUREHEIGHT*PRC; // Since all textures are 48x48 for now
      LONG tWidth  = DEFAULTTEXTUREWIDTH *PRC; // these are preset!
      LONG  hSkip, hS, HS;
      LONG  ySkip, yS;
      LONG  tY;

      LONG wOffset;
      LONG rOffset;
      BYTE* wByte; // The byte to write
      BYTE* rByte; // The byte to read
      BYTE wMask;  // bitMask for the pixel to write
      BYTE rMask;  // bitMask for the pixel to read
      LONG tm_X;
      LONG tm_X1;
      LONG tm_hSkip;

      //frame counter variables
      BOOL showFPS = FALSE;
      BOOL showSectorID = FALSE;
      LONG frames=0, fps=0;
      UBYTE frameStr[30];
      UBYTE sectorStr[30];
      BYTE skipped = TRUE;
      sprintf(frameStr, "FPS: %d\n", fps);

      //fill trigonometry values
      fillTrig(trig);

      if (loadWallTextures())
      {
        if (pD = LoadMap("map.data"))
        {
          pX = pD->pInitX*PRC;
          pY = pD->pInitY*PRC;
          angle = pD->pInitAngle;

          timerReq = OpenTimer();
          if (timerReq)
          {
            timerMP = timerReq->tr_node.io_Message.mn_ReplyPort;
            SendIO((struct IORequest*) timerReq);

            KeyIO = SetKeyboardAccess();
            if (KeyIO)
            {
              keyMatrix = (BYTE*) ((struct IOStdReq*) KeyIO)->io_Data;
              qRow = &keyMatrix[2]; // The bit for the Q key is in the 3rd byte of the keyMatrix
              aRow = &keyMatrix[4]; // The bit for the A key is in the 5th byte of the keyMatrix
              zRow = &keyMatrix[6]; // The bit for the Z key is in the 7th byte of the keyMatrix
              escRow = &keyMatrix[8];  // The bit for the ESC key is in the 9nth byte of the keyMatrix
              F1Row  = &keyMatrix[10]; // The bits for the F keys are in the 10th byte of the keyMatrix

              si = CreateScreen(WIDTH, HEIGHT, DEPTH);
              if (si)
              {
                // Find the sector the player is in at start:
                // This is an aproximate calculation that uses only two vertices.
                // Should be enhanced to use all 4 vertices if there will be angled walls.
                currSector = detectSector(pX, pY, NULL);

                /***********************
                * Main FRAME DRAW LOOP *
                ************************/
                while (TRUE)
                {
                  // Reset the frame completio n counter
                  viewComplete = 0;

                  // Have the trigonometry values at hand
                  angleMod = angle % 18;
                  if      (angle<18) {angleSin =  trig[   angleMod]; angleCos =  trig[18-angleMod];}
                  else if (angle<36) {angleSin =  trig[18-angleMod]; angleCos = -trig[   angleMod];}
                  else if (angle<54) {angleSin = -trig[   angleMod]; angleCos = -trig[18-angleMod];}
                  else               {angleSin = -trig[18-angleMod]; angleCos =  trig[   angleMod];}

                  // Transform the two unit vectors
                  east.X  =  angleSin*UNITWALLLENGTH;
                  east.Y  =  angleCos*UNITWALLLENGTH;
                  south.X = -east.Y;  // -angleCos*UNITWALLLENGTH;
                  south.Y =  east.X;  //  angleSin*UNITWALLLENGTH;

                  /***********************
                  *Capture keyboard input*
                  ************************/
                  // Read Keyboard state
                  DoIO(KeyIO);
                  // Remember old player coords
                  old_pX = pX;
                  old_pY = pY;
                  // Check for the keys
                  // 'ESC' quit key:
                  if (*escRow == 32) break; // The bit for the ESC key is the 6th bit of the 9nth byte
                  // 'F1' key (toggle FPS counter)
                  if (*F1Row == 1) { showFPS ^= 1; Delay(4);}
                  // 'F2' key (toggle sector ID display)
                  if (*F1Row == 2) { showSectorID ^= 1; Delay(4);}
                  // 'W' move forwards key:
                  if (*qRow == 2)
                  {
                    pX += angleCos;
                    pY += angleSin;
                  }
                  // 'S' move backwards key:
                  if (*aRow == 2 || *aRow == 3 || *aRow == 6)
                  {
                    pX -= angleCos;
                    pY -= angleSin;
                  }
                  // 'A' turn left key:
                  if (*aRow == 1 || *aRow == 3)
                  {
                    angle--;
                    if (angle < 0) angle = 71;
                  }
                  // 'D' turn left key:
                  if (*aRow == 4 || *aRow == 6)
                  {
                    angle++;
                    if (angle > 71) angle = 0;
                  }
                  // 'N' sidestep left
                  if (*zRow == 1) { pX += angleSin; pY -= angleCos; }
                  // 'M' sidestep right
                  if (*zRow == 2) { pX -= angleSin; pY += angleCos; }
                  // 'F' switch to fullscreen
                  if (*aRow == 8)
                  {
                    if (fullScreen)
                    {
                      ScreenSwap(si);
                      for(plane=0; plane<DEPTH; plane++)
                        BltClear((UBYTE *) si->rastPort->BitMap->Planes[plane], (si->rastPort->BitMap->BytesPerRow * (scrSize)), 1);
                      ScreenSwap(si);
                      for(plane=0; plane<DEPTH; plane++)
                        BltClear((UBYTE *) si->rastPort->BitMap->Planes[plane], (si->rastPort->BitMap->BytesPerRow * (scrSize)), 1);
                      scrOffset  = 100;
                      scrSize    = 100;
                      scrSizeP1  = scrSize+1;
                      scrHalf    = scrSize/2;
                      scrCenter  = scrOffset + scrHalf;
                      scrRows    = scrHalf + 1; //Rows to clear with BlitClear()
                      scale      = 60; // Scale image up to fill the screen
                      HDS        = viewDist * wallHeight * scale * PRC;
                      DS         = viewDist * scale;
                      fullScreen = FALSE;
                    }
                    else
                    {
                      scrOffset  = 30;
                      scrSize    = 255;
                      scrSizeP1  = scrSize+1;
                      scrHalf    = scrSize/2;
                      scrCenter  = scrOffset + scrHalf;
                      scrRows    = scrHalf + 1; //Rows to clear with BlitClear()
                      scale      = 120; // Scale image up to fill the screen
                      HDS        = viewDist * wallHeight * scale * PRC;
                      DS         = viewDist * scale;
                      fullScreen = TRUE;
                    }
                    Delay(4);
                  }

                  /*****************
                   * DETECT SECTOR *
                   *****************/
                  /* ...while preventing getting out of map (aka. wall clip) */
                  old_Sector = currSector;
                  currSector = detectSector((pX > old_pX) ? pX+PRC3 : pX-PRC3, pY, currSector);
                  if (!currSector) { pX = old_pX; currSector = old_Sector; }
                  currSector = detectSector(pX, (pY > old_pY) ? pY+PRC3 : pY-PRC3, currSector);
                  if (!currSector) { pY = old_pY; currSector = old_Sector; }
                  currSector = detectSector(pX, pY, currSector);

                  if (currSector)
                  {
                    //set portal limits as screensize to begin with
                    portalLeft  = -scrHalf;
                    portalRight =  scrHalf;

                    /**************
                     * DRAW WALLS *
                     **************/

                    // Transform the first vertex of the very first wall in the list
                    wL = (struct wList*) currSector->walls->mlh_Head;
                    w = wL->wall;
                    TXG = w->vertex1->X - pX;
                    tD  = w->vertex1->Y - pY;
                    TYG = ((TXG*angleCos) + (tD*angleSin)) >> PRCBITS;
                    TXG = ((TXG*angleSin) - (tD*angleCos)) >> PRCBITS;

                    // traverse the walls in this sector
                    for ( wL = (struct wList*) currSector->walls->mlh_Head; wL->node.mln_Succ; wL = (struct wList*) wL->node.mln_Succ)
                    {
                      w = wL->wall;
                      TX1 = TXG;
                      TY1 = TYG;

                      /********************
                       * EVALUATE PORTALS *
                       ********************/
                      if (w->ID < 0) // If this is a portal definition evaluate it
                      {

                        if (w->ID == PORTALEND)
                        {
                          // Pop from the portalMem and continue
                          skipped = ATSTEP1;
                          portalMemCurrent--;
                          TXG = portalMemCurrent->TXG;
                          TYG = portalMemCurrent->TYG;
                          portalLeft  = portalMemCurrent->parentPortalLeft;
                          portalRight = portalMemCurrent->parentPortalRight;
                          continue;
                        }

                        skipThisPortal = FALSE;

                        // Transform the second vertex for this portal
                        switch (w->orientation)
                        {
                          case EAST:
                            TX2 = TX1 + east.X*w->length; TY2 = TY1 + east.Y*w->length; break;
                          case WEST:
                            TX2 = TX1 - east.X*w->length; TY2 = TY1 - east.Y*w->length; break;
                          case SOUTH:
                            TX2 = TX1 + south.X*w->length; TY2 = TY1 + south.Y*w->length; break;
                          case NORTH:
                            TX2 = TX1 - south.X*w->length; TY2 = TY1 - south.Y*w->length; break;
                        }

                        if (TY1 < 0 && TY2 < 0) // the portal must be in front of the player to be drawn
                          skipThisPortal = TRUE;
                        else
                        {
                          //Calculate Screen coordinates first vertex 1
                          if (skipped == ATSTEP1)
                          {
                            if (TY1 <= 0) X1 = portalLeft;
                            else X1 = -((TX1*DS)/TY1);
                          }
                          else X1 = XG;

                          // ...now vertex 2
                          if (TY2 <= 0) X2 = portalRight;
                          else X2 = -((TX2*DS)/TY2);

                          if (X1 > portalRight || X2 < portalLeft) // check if portal is out of view
                            skipThisPortal = TRUE;
                          else
                          {
                            // Push portal T?G's and parent portal limits to portalMem.
                            portalMemCurrent->TXG = TX2;
                            portalMemCurrent->TYG = TY2;
                            portalMemCurrent->parentPortalLeft  = portalLeft;
                            portalMemCurrent->parentPortalRight = portalRight;
                            portalMemCurrent++;

                            portalRight = X2 > portalRight ? portalRight : --X2;
                            portalLeft  = X1 < portalLeft  ? portalLeft  : w->ID == PORTALTOSMALLNOBORDER ? X1 : ++X1;

                            if (w->ID == PORTALTOBIG)
                            {
                              // Iterate to the next wall
                              wL = (struct wList*) wL->node.mln_Succ;
                              w  = wL->wall;                           // NOTE: Should you test for NULL here!?

                              // Transform the first vertex of the first wall in this portal
                              TX1 = w->vertex1->X - pX;
                              tD  = w->vertex1->Y - pY;
                              TY1 = ((TX1*angleCos) + (tD*angleSin)) >> PRCBITS;
                              TX1 = ((TX1*angleSin) - (tD*angleCos)) >> PRCBITS;
                              skipped = ATSTEP1;
                            }
                            else
                            {
                              // Iterate to the next wall
                              wL = (struct wList*) wL->node.mln_Succ;
                              w  = wL->wall;                           // NOTE: Should you test for NULL here!?
                            }
                          }
                        }

                        if (skipThisPortal)
                        {
                          while (TRUE)
                          {
                            wL = (struct wList*) wL->node.mln_Succ;
                            if (wL->wall->ID < PORTALEND) childPortals++;
                            if (wL->wall->ID == PORTALEND)
                            {
                              if (childPortals) childPortals--;
                              else break;
                            }
                          }
                          skipped = ATSTEP1;
                          TXG = TX2;
                          TYG = TY2;
                          continue;
                        }
                      }

                      // Select the texture to be used for this wall
                      texture = w->texture;
                      /* Since all textures are 48x48 for now. Optimize this
                         getting out of the loop.
                      tWidth  = (texture->BytesPerRow << 3) << PRCBITS;
                      tHeight = texture->Rows << PRCBITS; */

                      /* Calculate the second vertex transformed coordinates by
                         adding the unit wall vector of the appropriate orientation.
                         (a method proposed by the great nightlord) */
                      switch (w->orientation)
                      {
                        case EAST:
                          TX2 = TX1 + east.X; TY2 = TY1 + east.Y; break;
                        case WEST:
                          TX2 = TX1 - east.X; TY2 = TY1 - east.Y; break;
                        case SOUTH:
                          TX2 = TX1 + south.X; TY2 = TY1 + south.Y; break;
                        case NORTH:
                          TX2 = TX1 - south.X; TY2 = TY1 - south.Y; break;
                      }
                      // Hand this TX2 over to the next wall as its TX1 as a given
                      TXG = TX2;
                      TYG = TY2;

                      /*******************
                       * TRANSFORMED MAP *
                       *******************
                      // Draw the transformed map. FOR DEBUGGING PURPOSES! REMOVE!!!
                      Move(si->rastPort, 160-TX1/PRC, 70-TY1/PRC);
                      Draw(si->rastPort, 160-TX2/PRC, 70-TY2/PRC);
                      WritePixel(si->rastPort, 160, 70); //Mark the position of the player */

                      //Skip if both vertices of the wall are behind the player
                      if (viewDist>TY1 && viewDist>TY2) skipped = ATSTEP1;
                      else
                      {
                       /**********************
                        * CLIP WALL VERTICES *
                        **********************/
                        clipped = FALSE;
                        if (viewDist>TY1)
                        {
                          TX1 = -((((TY2-viewDist) * (TX2-TX1))/(TY2-TY1))-TX2);
                          TY1 = viewDist;
                        }
                        else if (viewDist>TY2)
                        {
                          TX2 = -((((TY1-viewDist) * (TX2-TX1))/(TY2-TY1))-TX1);
                          TY2 = viewDist;
                          clipped = TRUE;
                        }

                        // calculate screen coordinates for perspective transformed wall vertices
                        // first calculate x coords.
                        // If this vertex was calculated before in this frame just recall it...
                        if (skipped == ATSTEP1)
                          X1 = -((TX1*DS)/TY1);  // ...if not calculate...
                        else
                          X1 = XG;

                        X2 = -((TX2*DS)/TY2);
                        XG = X2;

                        //Paint the walls
                        //This if checks if any part of the wall is in screen (or portal)
                        if ( X1 >= X2 || X2 < portalLeft || X1 > portalRight ) skipped = ATSTEP2;
                        else
                        {
                          // if x's are in screen then recall or calculate y coords.
                          if (skipped)
                            Y1B=(HDS << PRCBITS)/TY1;
                          else
                            Y1B = YG;

                          Y2B=(HDS << PRCBITS)/TY2;
                          YG = Y2B;

                          skipped = clipped ? ATSTEP1 : FALSE;

                          /*calculate the slope of the perspective wall top as the next
                            section draws the vertical lines to paint the wall.
                            This value will be used as an "error" value for drawing the
                            perspective height difference at every step:.*/
                          dH = (Y2B-Y1B)/(X2-X1);   // Since Y?B variables have PRECISION
                                                    // this has it too.

                          /*This next 'if' provides the drawing of only one side of a wall*/
                          if (X2 > X1)
                          {
                            /*This next 'if' provides the drawing is done always from small height
                              to big height. This is obligatory because when the opposite is done
                              when DH value is negative it gets a different value when rounded.*/
                            if (Y2B > Y1B)
                            {
                              wHs = Y1B << 1 ; //wall height at wall start edge (Y1B-Y1A)

                              //Clamp the rendering into display area horizontally
                              XS = w->isStartEdge; if (X1 < portalLeft) XS = portalLeft-X1;
                              XE = X2-X1-1; if ((XE+X1) > portalRight) XE = portalRight-X1;
                              viewComplete += (XE-XS+w->isStartEdge+1);

                              DH  = XS*dH; // Skip out of screen x's.
                              DH2 = DH << 1;  // DH * 2  // NOTE: This suprisingly works for negative
                                                         //       dH values in SAS/C. Interesting!

                              hS = tWidth/(X2-X1);
                              if (hS < MINHS) hS = MINHS;
                              HS = XS*hS;

                              for (X=XS; X<=XE; X++)
                              {
                                wH = wHs + DH2; //Wall height at this X
                                ySkip = (tHeight << PRCBITS)/wH;  // PRECISION

                                hSkip = HS >> PRCBITS;

                                //Clamp the rendering into display area vertically
                                YS = -((Y1B+DH) >> PRCBITS);
                                if (YS < (0-scrHalf)) {tY = 0-YS-scrHalf; YS = 0-scrHalf;} else {tY = 0;}
                                YE=0;                               //This is an aproximation for speed
                                //YE = SPFix(SPAdd(Y1Bf, DH));        //If you want't to implement a pseudo
                                //if (YE > scrHalf) YE = scrHalf;     //look up/down use the commented out
                                                                      //lines below it

                                yS = ySkip*tY; // skip outof screen Y's.

                                tm_X  = X1+X+scrCenter;
                                tm_X1 = tm_X / 8;
                                tm_hSkip = hSkip / 8;
                                wMask = 1 << (7 - (tm_X  % 8));  // bitMask for the pixel to write
                                rMask = 1 << (7 - (hSkip % 8));  // bitMask for the pixel to read

                                for (Y=YS; Y<=YE; Y++)
                                {
                                  //WritePixel(si->rastPort, X1+X+scrCenter, Y+scrHalf);
                                  //MapTexture(si->rastPort->BitMap, texture, X1+X+scrCenter, Y+scrHalf, hSkip, SPFix(yS));
                                  /***************
                                   * MAP TEXTURE *
                                   ***************/
                                    wOffset = (si->rastPort->BitMap->BytesPerRow * (Y+scrHalf)) + tm_X1;
                                    rOffset = (texture->BytesPerRow * (yS >> PRCBITS)) + tm_hSkip;

                                    for (plane=0; plane<si->rastPort->BitMap->Depth; plane++)
                                    {
                                      rByte = texture->Planes[plane] + rOffset;              // this is pointer arithmetic here!
                                      wByte = si->rastPort->BitMap->Planes[plane] + wOffset; // this is pointer arithmetic here!

                                      //check the bit in texture
                                      if (*rByte & rMask) *wByte |= wMask; //set the bit in screen BitMap
                                    }

                                  yS += ySkip;
                                }

                                DH  += dH;     //next DH
                                DH2 = DH << 1; // DH2 = DH * 2
                                HS  += hS;
                              }
                            }
                            else
                            {
                              wHs = Y2B << 1;

                              //Clamp the rendering into display area horizontally
                              if (X2 > portalRight) XS = X2-portalRight; else XS = 1;
                              XE= X2-X1-w->isStartEdge; if (XE>(X2-portalLeft)) XE=X2-portalLeft;
                              viewComplete += (XE-XS+w->isStartEdge+1);

                              DH = -XS*dH;
                              DH2 = DH << 1;  // DH x 2

                              hS = tWidth/(X2-X1);
                              if (hS < MINHS) hS = MINHS;
                              HS = tWidth-XS*hS; // Add here a -1 when not using bitshift for divisions

                              for (X=XS; X<=XE; X++)
                              {

                                wH = wHs + DH2; //Wall height at this X
                                ySkip = (tHeight << PRCBITS)/wH;

                                hSkip = HS >> PRCBITS;

                                //Clamp the rendering into display area vertically
                                YS = -((Y2B+DH) >> PRCBITS);
                                if (YS < (0-scrHalf)) {tY = 0-YS-scrHalf; YS = 0-scrHalf;} else {tY = 0;}
                                YE=0;                               //This is an aproximation for speed
                                //YE = SPFix(SPAdd(Y2Bf, DH));        //If you want't to implement a pseudo
                                //if (YE > scrHalf) YE = scrHalf;     //look up/down use the commented out
                                                                      //lines below it
                                yS = ySkip*tY;

                                // Some variables for texture mapping
                                tm_X  = X2-X+scrCenter;
                                tm_X1 = tm_X / 8;
                                tm_hSkip = hSkip / 8;
                                wMask = 1 << (7 - (tm_X  % 8));  // bitMask for the pixel to write
                                rMask = 1 << (7 - (hSkip % 8));  // bitMask for the pixel to read

                                for (Y=YS; Y<=YE; Y++)
                                {
                                  //WritePixel(si->rastPort, X2-X+scrCenter, Y+scrHalf);
                                  //MapTexture(si->rastPort->BitMap, texture, X2-X+scrCenter, Y+scrHalf, hSkip, SPFix(yS));
                                  /***************
                                   * MAP TEXTURE *
                                   ***************/
                                    wOffset = (si->rastPort->BitMap->BytesPerRow * (Y+scrHalf)) + tm_X1;
                                    rOffset = (texture->BytesPerRow * (yS >> PRCBITS)) + tm_hSkip;

                                    for (plane=0; plane<si->rastPort->BitMap->Depth; plane++)
                                    {
                                      rByte = texture->Planes[plane] + rOffset;              // this is pointer arithmetic here!
                                      wByte = si->rastPort->BitMap->Planes[plane] + wOffset; // this is pointer arithmetic here!

                                      //check the bit in texture
                                      if (*rByte & rMask) *wByte |= wMask; //set the bit in screen BitMap
                                    }

                                  yS += ySkip;

                                }

                                DH  -= dH;     // Next DH
                                DH2 = DH << 1; // DH2 = DH * 2
                                HS  -= hS;
                              }
                            }
                          }
                        }
                      }
                      if (viewComplete >= scrSize) {skipped = TRUE; portalMemCurrent = portalMem; break;}
                    }
                  }
                  // View is complete here. Reset some values for the next frame.
                  skipped = TRUE;

                  /*************************************************
                   * COPY THE TOP PART of the SCREEN to the BOTTOM *
                   *************************************************/
                   for (Y=0; Y<=scrHalf; Y++)
                   {
                     BltBitMap(si->rastPort->BitMap, scrOffset, Y, si->rastPort->BitMap, scrOffset, scrSize-Y, scrSizeP1, 1, 0xC0, 0xFF, NULL);
                   }

                  /***************
                   * DISPLAY FPS *
                   ***************/
                  if (showFPS)
                  {
                    /*****************
                    * CALCULATE FPS *
                    *****************/
                    if(GetMsg(timerMP))  //If there is a message at timer message port
                    {
                      // Calculate FPS and reset frame count
                      fps = frames/2; frames = 0;
                      // make a formatted string ready to print on screen
                      sprintf(frameStr, "FPS: %d\n", fps);

                      // Issue another message to be replied 2 seconds later
                      timerReq->tr_node.io_Command = TR_ADDREQUEST;
                      timerReq->tr_time.tv_secs    = 2;
                      timerReq->tr_time.tv_micro   = 0;
                      SendIO((struct IORequest*) timerReq);
                    }

                    Move(si->rastPort, 0, 9);
                    Text(si->rastPort, frameStr, strlen(frameStr));
                    frames++;
                  }

                  /*********************
                   * DISPLAY SECTOR ID *
                   *********************/
                  if (showSectorID)
                  {
                    sprintf(sectorStr, "Sector: %i\n", currSector->ID);
                    Move(si->rastPort, 0, 18);
                    Text(si->rastPort, sectorStr, strlen(sectorStr));
                  }

                  /*********************************************************
                   *Display the drawn frame and clear up for the next frame*
                   *********************************************************/
                  ScreenSwap(si);
                  for(plane=0; plane<DEPTH; plane++)
                    BltClear((UBYTE *) si->rastPort->BitMap->Planes[plane], (si->rastPort->BitMap->BytesPerRow * scrRows), 1); //(si->rastPort->BitMap->Rows) for all screen

                }

              DisposeScreen(si);
              }
              else rc = RETURN_FAIL;
            }
            else rc= RETURN_FAIL;

          EndKeyboardAccess(KeyIO);
          }
          else rc = RETURN_FAIL;

        WaitPort(timerMP);  // Wait for the last sent message to be replied
        GetMsg(timerMP);    // Then remove it and close the timer device.
        CloseTimer(timerMP, timerReq);
        }
        else rc = RETURN_FAIL;
      }
      else rc = RETURN_FAIL;
      freeWallTextures();

    CleanUp3D();
    }
    else rc = RETURN_FAIL;

  return(rc);
}

/* /// */
/* /// "cleanup" */
/* ---------------------------------- CleanUp ----------------------------------
 Free allocated resources
*/

void
CleanUp(struct Config *config)
{
    if (config)
    {
        /* <INSERT YOUR CLEAN-UP CODE HERE AND DELETE THIS LINE> */

        /* free command line options */
        #if RDARGS_OPTIONS
            if (config->RDArgs)
                FreeArgs(config->RDArgs);
        #endif

        FreeMem(config, sizeof(struct Config));
    }
}

/* /// */
