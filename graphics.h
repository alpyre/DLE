/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 14-04-2017

Graphic related stuff
*/

/*********************************************************************
 * This is a custom struct to pass all the info on the opened screen *
 * in graphics.c, to the main.c.                                     *
 *********************************************************************/
struct ScreenInfo
{
  struct View* view;
  struct View* oldView;           //hold a pointer to the intuitions screen
  struct ViewPort* viewPort;
  struct BitMap* bitMap[2];       //two bitmaps for a double buffered screen
  struct cprlist* LOFcprList[2];  //two copper lists for the double buffer
  struct cprlist* SHFcprList[2];
  struct RastPort* rastPort;
  struct ColorMap* colorMap;
  UWORD width;
  UWORD height;
  BOOL  currentBuffer;            //The currentBuffer displayed
  /* Pointers to be remembered that will be used when disposing the screen */
  struct ViewExtra* vExtra;
  struct ViewPortExtra* vpExtra;
  struct RasInfo* rasInfo;
  struct DimensionInfo* dimquery;
  UBYTE depth;                  //Screens depth (how many bitplanes)
};

struct Texture
{
  struct MinNode node;
  LONG ID;
  struct BitMap *bitMap;
};

// default texture sizes (for now)
#define DEFAULTTEXTUREWIDTH  48
#define DEFAULTTEXTUREHEIGHT 48

struct ScreenInfo* CreateScreen(UWORD, UWORD, UBYTE);
VOID DisposeScreen(struct ScreenInfo*);
VOID ScreenSwap(struct ScreenInfo*);
struct BitMap* loadILBMTexture(STRPTR);
VOID freeTexture(struct BitMap*);

BOOL loadWallTextures(VOID);
VOID freeWallTextures(VOID);
