/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 29-04-2017

3D environment
*/
#define PRCBITS 10  //bits of precision for nonFP rational calculations.
#define PRC 1024
#define PRC2 PRC*2
#define PRC3 PRC*3
#define MINHS PRC/8

#define UNITWALLLENGTH 10

enum {NORTH, SOUTH, EAST, WEST};
#define PORTALEND -1
#define PORTALTOSMALL -2
#define PORTALTOSMALLNOBORDER -3
#define PORTALTOBIG -4

struct Vector
{
  LONG X;
  LONG Y;
};

struct Vertex
{
  struct MinNode node;
  ULONG ID;
  ULONG X;
  ULONG Y;
};

struct Wall
{
  struct MinNode node;
  LONG ID;
  struct Vertex* vertex1;  // The two vertices that
  struct Vertex* vertex2;  // define a wall
  struct BitMap* texture;  // the texture for this wall
  BYTE orientation;        // the orientation of the wall NORTH/SOUTH/EAST or WEST
  BYTE length;             // the length of the wall in unit walls
  BOOL isStartEdge;
};

struct Sector
{
  struct MinNode node;
  ULONG ID;
  struct Vertex* vertex1;  //
  struct Vertex* vertex2;  // These are the four vertices that
  struct Vertex* vertex3;  // define a sector. (this logic may be changed)
  struct Vertex* vertex4;  //
  struct MinList* walls;     // a list of walls to be drawn in this sector in draw order.
  struct MinList* neighbors; // a list of neighboring sectors to this sector
};

struct wList
{
  struct MinNode node;
  struct Wall* wall;
};

struct sList
{
  struct MinNode node;
  struct Sector* sector;
};

struct pList
{
  struct MinNode node;
  APTR ptr;
};

struct PortalMem
{
  LONG TXG;
  LONG TYG;
  WORD parentPortalLeft;
  WORD parentPortalRight;
};

struct PlayerData
{
  LONG pInitX;
  LONG pInitY;
  BYTE pInitAngle;
};

BOOL SetUp3D(VOID);
VOID CleanUp3D(VOID);
struct PlayerData* LoadMap(STRPTR);
VOID DumpMap(VOID);

VOID MapTexture(struct BitMap*, struct BitMap*, LONG, LONG, LONG, LONG);
struct Sector* detectSector(LONG, LONG, struct Sector*);
VOID fillTrig(LONG*);
