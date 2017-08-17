/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 01-05-2017

3D environment
*/

#include <exec/exec.h>
#include <proto/exec.h>
#include <libraries/mathffp.h>
#include <clib/mathffp_protos.h>
#include <clib/mathtrans_protos.h>
#include <dos.h>
#include <proto/dos.h>
#include <graphics/gfx.h>
#include <string.h>
#include <clib/alib_stdio_protos.h>

#include "3D.h"
#include "graphics.h"

struct Library *MathBase = NULL;
struct Library *MathTransBase = NULL;

extern APTR mapPool;             // The memPool used for vertices, walls and sectors.
extern struct MinList* vertices; // The global list for all defined vertices in map.
extern struct MinList* walls;    // The global list for all defined walls in map.
extern struct MinList* sectors;  // The global list for all defined sectors in map.
extern struct MinList* textures; // The global list for all loaded textures.
#define EOF -1
#define AddMinTail(a, b) AddTail((struct List*) a, (struct Node*) b)
enum {WALL, SECTOR};
enum {OK, BADFILE, FAIL};

ULONG sToU(STRPTR str);
struct MinList* lToL(STRPTR str, int type);
LONG Abs(LONG x);
LONG hypoth(LONG a, LONG b);

/// SetUp3D()
BOOL SetUp3D(VOID)
{
  MathBase = OpenLibrary("mathffp.library", 33);
  if (!MathBase) return (FALSE);

  MathTransBase = OpenLibrary("mathtrans.library", 33);
  if (!MathTransBase) {CleanUp3D(); return (FALSE);}

  mapPool = CreatePool(NULL, 4096, 4096);
  if (!mapPool) {CleanUp3D(); return (FALSE);}

  //Allocate and initialize lists
  vertices = AllocPooled(mapPool, sizeof(struct MinList));
  walls    = AllocPooled(mapPool, sizeof(struct MinList));
  sectors  = AllocPooled(mapPool, sizeof(struct MinList));
  textures = AllocPooled(mapPool, sizeof(struct MinList));
  if (vertices == NULL || walls == NULL || sectors == NULL || textures == NULL)
  {
    CleanUp3D();
    return (FALSE);
  }

  NewList((struct List*) vertices);
  NewList((struct List*) walls);
  NewList((struct List*) sectors);
  NewList((struct List*) textures);

return (TRUE);
}
///
/// CleanUp3D
VOID CleanUp3D(VOID)
{
  if (MathBase) CloseLibrary(MathBase);
  MathBase = NULL;
  if (MathTransBase) CloseLibrary(MathTransBase);
  MathTransBase = NULL;
  if (mapPool) DeletePool(mapPool);
  mapPool = NULL;
  vertices = NULL;
  walls = NULL;
  sectors = NULL;
  textures = NULL;
}
///

/// findVertex()
/*******************************************************
 * Search a vertex with ID in the global vertices list *
 * and return its pointer.                             *
 *******************************************************/
struct Vertex* findVertex(ULONG ID)
{
  struct Vertex* v;
  for (v = (struct Vertex*) vertices->mlh_Head; v->node.mln_Succ; v = (struct Vertex*) v->node.mln_Succ)
  {
    if (v->ID == ID) return v;
  }
  return NULL;
}
///
/// findWall()
/**************************************************
 * Search a wall with ID in the global walls list *
 * and return its pointer.                        *
 **************************************************/
struct Wall* findWall(ULONG ID)
{
  struct Wall* w;
  for (w = (struct Wall*) walls->mlh_Head; w->node.mln_Succ; w = (struct Wall*) w->node.mln_Succ)
  {
    if (w->ID == ID) return w;
  }
  return NULL;
}
///
/// findSector()
/******************************************************
 * Search a sector with ID in the global sectors list *
 * and return its pointer.                            *
 ******************************************************/
struct Sector* findSector(ULONG ID)
{
  struct Sector* s;
  for (s = (struct Sector*) sectors->mlh_Head; s->node.mln_Succ; s = (struct Sector*) s->node.mln_Succ)
  {
    if (s->ID == ID) return s;
  }
  return NULL;
}
///
/// findTexture()
/********************************************************
 * Search a texture with ID in the global textures list *
 * and return its pointer.                              *
 *******************************************************/
struct Texture* findTexture(ULONG ID)
{
  struct Texture* t;

  for (t = (struct Texture*) textures->mlh_Head; t->node.mln_Succ; t = (struct Texture*) t->node.mln_Succ)
  {
    if (t->ID == ID) return t;
  }
  return NULL;
}
///

/// NewVertex()
/********************************************************************
 * Allocate a new vertex in the mapPool fill it with the input args *
 * and insert it into the global vertex list.                       *
 ********************************************************************/
int NewVertex(ULONG ID, ULONG X, ULONG Y)
{
  struct Vertex* v = NULL;
  v = (struct Vertex*) AllocPooled(mapPool, sizeof(struct Vertex));
  if (v)
  {
    v->ID = ID;
    v->X  = X*PRC;
    v->Y  = Y*PRC;
    AddMinTail(vertices, &v->node);
  }
  else return (FAIL);
return (OK);
}
///
/// NewWall()
int NewWall(ULONG ID, ULONG vID1, ULONG vID2, ULONG tID, BOOL startEdge)
{
  struct Wall* w;
  w = (struct Wall*) AllocPooled(mapPool, sizeof(struct Wall));
  if (w)
  {
    w->ID = ID;
    w->vertex1 = findVertex(vID1);  // Find firstVertex from vertex ID vID1
    w->vertex2 = findVertex(vID2);  // Find secondVertex from vertex ID vID2
    w->texture = findTexture(tID)->bitMap;  // Find texture from texture ID tID
    w->isStartEdge = startEdge;
    if (w->vertex1 == NULL)
    {
      printf("Undefined vertex1 for wall %i\n", ID);
      return (BADFILE);
    }
    if (w->vertex2 == NULL)
    {
      printf("Undefined vertex2 for wall %i\n", ID);
      return (BADFILE);
    }
    if (w->texture == NULL)
    {
      printf("Texture file %i not found for wall %i\n", tID, ID);
      return (BADFILE);
    }

    // Detect the walls orientation and length and fill appropriate members
    if (w->vertex1->X == w->vertex2->X)
    {
      if (w->vertex1->Y > w->vertex2->Y) w->orientation = NORTH;
      else                               w->orientation = SOUTH;
      w->length = (Abs(w->vertex2->Y - w->vertex1->Y)/PRC)/UNITWALLLENGTH;
    }
    else if (w->vertex1->Y == w->vertex2->Y)
    {
      if (w->vertex2->X > w->vertex1->X) w->orientation = EAST;
      else                               w->orientation = WEST;
      w->length = (Abs(w->vertex2->X - w->vertex1->X)/PRC)/UNITWALLLENGTH;
    }
    else
    {
      puts("diagonal walls not implemented yet!");
      return (BADFILE);
    }

    AddMinTail(walls, &w->node);     // add it to the walls list
  }
  else return (FAIL);

return (OK);
}
///
/// NewSector()
int NewSector(ULONG ID, ULONG vID1, ULONG vID2, ULONG vID3, ULONG vID4, STRPTR walls, STRPTR neigh)
{
  struct Sector* s;
  s = (struct Sector*) AllocPooled(mapPool, sizeof(struct Sector));
  if(s)
  {
    s->ID      = ID;
    s->vertex1 = findVertex(vID1); // Find firstVertex from vertex ID vID1
    s->vertex2 = findVertex(vID2); // Find secondVertex from vertex ID vID2
    s->vertex3 = findVertex(vID3); // Find thirdVertex from vertex ID vID3
    s->vertex4 = findVertex(vID4); // Find fourthVertex from vertex ID vID4
    if (s->vertex1 == NULL || s->vertex2 == NULL || s->vertex3 == NULL || s->vertex4 == NULL)
      return (BADFILE);
    s->neighbors = (struct MinList*) neigh; // Temporarily make these pointers point to the
    s->walls     = (struct MinList*) walls; // strings that contain walls and neighbors.
                                            // typecasting is just to prevent warnings!
    AddMinTail(sectors, &s->node);     // add it to the sectors list
  }
  else return (FAIL);

return (OK);
}
///

/**********************
 * Load Map from disk *
 **********************/
/// LoadMap()
struct PlayerData* LoadMap(STRPTR mapFile)
{
  BPTR lock;
  LONG fileSize;
  APTR fileBuffer;
  struct FileInfoBlock *fib;
  struct PlayerData* pD = NULL;

  BPTR fh;
  LONG l = 0;   // the line where the read cursor is at (for error reporting)
  LONG ch;      // read buffer for FGetC()
  BYTE *crs;    // read cursor for readBuffer
  BYTE *bufEnd; // a pointer to the end of buffer
  BOOL eof = FALSE;     // set this when end of file is reached
  BOOL badFile = FALSE; // set this when map file is determined to be corrupt
  BOOL fail    = FALSE; // set this when an allocation fails
  BOOL playerCoordsRead = FALSE;  // set this when playerCoords are found in map file

  char  ID[10];    // read buffers to read vertices
  char  V1[10];    //
  char  V2[10];    //
  STRPTR id, v1, v2, v3, v4, wl, nl, tID; // temp pointers used in file evaluation
  int result;  // a value to hold return values from NewWall/NewSector funcs.

  struct Sector* s;  // iteration pointer for sector lists

  BOOL startWall;    // This will be set if wall definition ends with a '*'
  UBYTE pos = 0;     // position variable for read buffers.

  /*********************
   * GET THE FILE SIZE *
   *********************/
  // allocate a fib
  fib = AllocDosObject(DOS_FIB, NULL);
  if (!fib) {
   puts("failed allocation of fileInfoBlock");
   return(NULL);
  }
  // lock the file
  lock = Lock(mapFile, ACCESS_WRITE);
  if (!lock) {
   FreeDosObject(DOS_FIB, fib);
   puts("failed lock");
   return(NULL);
  }
  // examine the file size
  Examine(lock, fib);
  fileSize = fib->fib_Size;
  // unlock the file
  UnLock(lock);
  lock = NULL;
  // free the fib
  FreeDosObject(DOS_FIB, fib);
  fib = NULL;
  // check for empty file
  if (fileSize == 0){
   puts("empty file");
   return(NULL);
  }

  /************************
   * ALLOCATE READ BUFFER *
   ************************/
   fileBuffer = AllocMem(fileSize + 1 , NULL);
   if (!fileBuffer)
   { puts("buffer allocation failed");
     return(NULL);
   }

   /*****************************
    * ALLOCATE THE RETURN VALUE *
    *****************************/
  pD = (struct PlayerData*) AllocPooled(mapPool, sizeof(struct PlayerData));
  if (!pD)
  { puts("memory allocation fail");
    FreeMem(fileBuffer, fileSize + 1);
    return(NULL);
  }
  pD->pInitAngle = 0; // Default value

  /*****************
   * READ THE FILE *
   *****************/
  // Open the file and get the fileHandle
  fh = Open(mapFile, MODE_OLDFILE);
  if (!fh) {
    FreeMem(fileBuffer, fileSize + 1);
    puts("could not open file");
    return(NULL);
  }

  // read the file (while skipping comments and capturing vertices)
  crs = fileBuffer;
  ch = FGetC(fh);
  while (!eof)         // FILE READER MAIN LOOP
  {
    if (ch == '/')     // COMMENT SKIPPER
    {
      ch = FGetC(fh);
      if (ch == '*')   // multiline comments
      {
        while (TRUE)
        {
          ch = FGetC(fh);
          if (ch == EOF) { badFile = TRUE; eof = TRUE; break; }
          if (ch == 10 ) l++;
          if (ch == '*')
          {
            ch = FGetC(fh);
            if (ch == '/') { ch = FGetC(fh); break; }
            if (ch == EOF) { badFile = TRUE; eof = TRUE; break; }
          }
        }
      }
      else if (ch == '/')   // single line comments
      {
        while (TRUE)
        {
          ch = FGetC(fh);
          if (ch == EOF)
          {
            eof = TRUE;
            break;
          }
          if (ch == 10) { l++; break; }
        }
      }
      else {badFile = TRUE; break;}
    }
    if (ch == 'v' || ch == 'V')   // VERTEX READER
    {
      ch = FGetC(fh);
      if (ch != ':') { badFile = TRUE; break; }
      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else ID[pos++] = ch;                                // for ID's
      while (TRUE) {
        ch = FGetC(fh);
        if (ch == ':') break;
        if (ch < 48 || ch > 57) { badFile = TRUE; break; }
        ID[pos++] = ch;
      }
      if (badFile) break;
      ID[pos] = NULL;
      pos = 0;

      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else V1[pos++] = ch;                                // for vertice X
      while (TRUE) {
        ch = FGetC(fh);
        if (ch == ',') break;
        if (ch < 48 || ch > 57) { badFile = TRUE; break; }
        V1[pos++] = ch;
      }
      if (badFile) break;
      V1[pos] = NULL;
      pos = 0;

      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else V2[pos++] = ch;                                // for vertice Y
      while (TRUE) {
        ch = FGetC(fh);
        if (ch < 48 || ch > 57) break;
        V2[pos++] = ch;
      }

      V2[pos] = NULL;
      pos = 0;

      result = NewVertex(sToU(ID), sToU(V1), sToU(V2));
      if (result == FAIL) { fail = TRUE; break; }
      else if (result == BADFILE) { badFile = TRUE; break; }

      if (ch == 13) ch = FGetC(fh);                    // If a carriage return skip it
      if (ch == 10) { ch = FGetC(fh); l++; continue; } // Don't buffer \n's at the end
    }
    if (ch == 'P' || ch == 'p')  // PLAYER COORDINATE READER
    {
      ch = FGetC(fh);
      if (ch != ':') { badFile = TRUE; break; }
      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else V1[pos++] = ch;                                // for player init X coord
      while (TRUE) {
        ch = FGetC(fh);
        if (ch == ',') break;
        if (ch < 48 || ch > 57) { badFile = TRUE; break; }
        V1[pos++] = ch;
      }
      if (badFile) break;
      V1[pos] = NULL;
      pos = 0;

      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else V2[pos++] = ch;                                // for player init Y cood
      while (TRUE) {
        ch = FGetC(fh);
        if (ch < 48 || ch > 57) break;
        V2[pos++] = ch;
      }
      V2[pos] = NULL;
      pos = 0;

      pD->pInitX = sToU(V1);
      pD->pInitY = sToU(V2);
      playerCoordsRead = TRUE;

      if (ch == 13) ch = FGetC(fh);                    // If a carriage return skip it
      if (ch == 10) { ch = FGetC(fh); l++; continue; } // Don't buffer \n's at the end
    }
    if (ch == 'A' || ch == 'a')   // PLAYER ANGLE READER
    {
      ch = FGetC(fh);
      if (ch != ':') { badFile = TRUE; break; }
      ch = FGetC(fh);                                     // at least one numeric
      if (ch < 48 || ch > 57) { badFile = TRUE; break; }  // char is obligatory
      else V1[pos++] = ch;                                // for player init angle
      while (TRUE) {
        ch = FGetC(fh);
        if (ch < 48 || ch > 57) break;
        V2[pos++] = ch;
      }
      if (badFile) break;
      V1[pos] = NULL;
      pos = 0;

      pD->pInitAngle = sToU(V1);
      pD->pInitAngle = pD->pInitAngle % 72;   // This magic number 72 is the angle max!

      if (ch == 13) ch = FGetC(fh);                    // If a carriage return skip it
      if (ch == 10) { ch = FGetC(fh); l++; continue; } // Don't buffer \n's at the end
    }
    if (ch == EOF) {eof = TRUE; break;}
    if (ch == 13) { ch = FGetC(fh); continue; } //Skip carriage returns if any
    if (ch == 10 ) l++;                         // count lines
    *crs = ch; crs++;
    ch = FGetC(fh);
  }
  *crs = NULL;  // NULL terminate the buffer
  bufEnd = crs; // remember the address the buffer ends
  Close(fh); //Close the file

  if (fail) {
    FreeMem(fileBuffer, fileSize + 1);
    puts("memory allocation failed");
    return(NULL);
  }

  if (badFile) {
    FreeMem(fileBuffer, fileSize + 1);
    printf("corrupt map file at line:%i", l);
    return(NULL);
  }

  if (crs == fileBuffer)
  {
    FreeMem(fileBuffer, fileSize + 1);
    puts("empty map file");
    return(NULL);
  }

  if (!playerCoordsRead)
  {
    FreeMem(fileBuffer, fileSize + 1);
    puts("Player initial coords not found in map file");
    return(NULL);
  }

  /***********************
   * Evaluate the buffer *
   ***********************/
   // Read all Walls
   crs = fileBuffer;  // head back to buffer start
   eof = FALSE;       // restart reading this time from the buffer
   id  = "first";     // Report this if even the very first wall definition is corrupt.

   // read Walls
   while (crs != bufEnd)
   {
     if (*crs == 'W' || *crs == 'w')      // WALL READER
     {
       crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else crs++;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else id = crs++;                                        // is obligatory for ID
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else { *crs = NULL; crs++; }
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v1 = crs++;                                        // is obligatory for Vertex1
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ',') { badFile = TRUE; break; }
       else { *crs = NULL; crs++; }
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v2 = crs++;                                        // is obligatory for Vertex2
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else { *crs = NULL; crs++; }
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else tID = crs++;                                       // is obligatory for textureID
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs == '*') startWall = TRUE; else startWall = FALSE;
       if (!(*crs == 10 || *crs == '*' || *crs == NULL)) { badFile = TRUE; break; }
       else { *crs = NULL; }

       result = NewWall(sToU(id), sToU(v1), sToU(v2), sToU(tID), startWall);
       if (result == FAIL) { fail = TRUE; break; }
       else if (result == BADFILE) { badFile = TRUE; break; }
     }
     if (crs != bufEnd) crs++;
   }
   if (badFile) {
     FreeMem(fileBuffer, fileSize + 1);
     printf("corrupt map file at wall: %s\n", id);
     return(NULL);
   }
   if (fail) {
     FreeMem(fileBuffer, fileSize + 1);
     puts("memory allocation failure");
     return(NULL);
   }

   // Read all Sectors
   crs = fileBuffer;  // head back to buffer start
   eof = FALSE;       // restart reading this time from the buffer
   id  = "first";     // Report this if even the very first wall definition is corrupt.

   // read sectors
   while (!eof)
   {
     if (*crs == 'S' || *crs == 's')      // SECTOR READER
     {
       crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else crs++;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else id = crs++;                                        // is obligatory for ID
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else *crs++ = NULL;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v1 = crs++;                                        // is obligatory for Vertex1
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ',') { badFile = TRUE; break; }
       else *crs++ = NULL;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v2 = crs++;                                        // is obligatory for Vertex2
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ',') { badFile = TRUE; break; }
       else *crs++ = NULL;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v3 = crs++;                                        // is obligatory for Vertex3
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ',') { badFile = TRUE; break; }
       else *crs++ = NULL;
       if (*crs < 48 || *crs > 57) { badFile = TRUE; break; }  // at least one numeric char
       else v4 = crs++;                                        // is obligatory for Vertex4
       while (*crs > 47 && *crs < 58) crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else *crs++ = NULL;
       // read walls list
       wl = crs;
       while ((*crs > 47 && *crs < 58) || *crs == ',' || *crs == '(' || *crs == ')' || *crs == '[' || *crs == ']' || *crs == '{' || *crs == '}' || *crs == ';') crs++;
       if (*crs != ':') { badFile = TRUE; break; }
       else *crs++ = NULL;
       // read the neighbors list
       nl = crs;
       while ((*crs > 47 && *crs < 58) || *crs == ',') crs++;
       if (!(*crs == 10 || *crs == NULL)) { badFile = TRUE; break; }
       else *crs = NULL;

       result = NewSector(sToU(id), sToU(v1), sToU(v2), sToU(v3), sToU(v4), wl, nl);
       if (result == BADFILE) { badFile = TRUE; break; }
       else if (result == FAIL) { fail = TRUE; break; }
     }
     if (crs == bufEnd) eof = TRUE;
     else crs++;
   }
   if (badFile) {
     FreeMem(fileBuffer, fileSize + 1);
     printf("corrupt map file at sector: %s\n", id);
     return(NULL);
   }
   if (fail) {
     FreeMem(fileBuffer, fileSize + 1);
     puts("memory allocation failure");
     return(NULL);
   }

   /*******************************
    * Evaluate sector inner lists *
    *******************************/
   // Create the wall lists for sectors, using the attached temporary string pointers
   for (s = (struct Sector*) sectors->mlh_Head; s->node.mln_Succ; s = (struct Sector*) s->node.mln_Succ)
   {
     s->walls = lToL((STRPTR) s->walls, WALL);
     if (!s->walls) { badFile = TRUE; break; }
   }
   if (badFile) {
     FreeMem(fileBuffer, fileSize + 1);
     printf("corrupt map file at sector: %i\nBad wall list\n", s->ID);
     return(NULL);
   }

   // Create the neighbors lists for sectors, using the attached temporary string pointers
   for (s = (struct Sector*) sectors->mlh_Head; s->node.mln_Succ; s = (struct Sector*) s->node.mln_Succ)
   {
     s->neighbors = lToL((STRPTR) s->neighbors, SECTOR);
     if (!s->neighbors) { badFile = TRUE; break; }
   }
   if (badFile) {
     FreeMem(fileBuffer, fileSize + 1);
     printf("corrupt map file at sector: %i\nBad neighbours list", s->ID);
     return(NULL);
   }

   // if we've come this far the map is valid and it is loaded to the memory
   // so let's free the buffers we've allocated and return.
   FreeMem(fileBuffer, fileSize + 1);
return (pD);
}
///

// A Temporary diagnostic function
/// DumpMap()
/******************************************
 * Print loaded map structures to console *
 ******************************************/
VOID DumpMap(VOID)
{
  struct Vertex* v;
  struct Wall* w;
  struct Sector* s;
  struct wList* wL;
  struct sList* sL;

  // Dump vertices
  for (v = (struct Vertex*) vertices->mlh_Head; v->node.mln_Succ; v = (struct Vertex*) v->node.mln_Succ)
  {
    printf("V:%i:%i,%i\n", v->ID, v->X/PRC, v->Y/PRC);
  }
  // Dump walls
  for (w = (struct Wall*) walls->mlh_Head; w->node.mln_Succ; w = (struct Wall*) w->node.mln_Succ)
  {
    if (w->isStartEdge)
      printf("W:%i:%i,%i*", w->ID, w->vertex1->ID, w->vertex2->ID);
    else
      printf("W:%i:%i,%i", w->ID, w->vertex1->ID, w->vertex2->ID);
    printf ("length = %i\n", w->length);
  }
  // Dump sectors
  for (s = (struct Sector*) sectors->mlh_Head; s->node.mln_Succ; s = (struct Sector*) s->node.mln_Succ)
  {
    printf("S:%i:%i,%i,%i,%i:", s->ID, s->vertex1->ID, s->vertex2->ID, s->vertex3->ID, s->vertex4->ID);
    // sector walls list
    for ( wL = (struct wList*) s->walls->mlh_Head; wL->node.mln_Succ; wL = (struct wList*) wL->node.mln_Succ)
    {
      if (wL->wall->ID == PORTALTOSMALL)
        printf("[%i,%iL=%i;", wL->wall->vertex1->ID, wL->wall->vertex2->ID, wL->wall->length);
      else if (wL->wall->ID == PORTALTOBIG)
        printf("(%i,%iL=%i;", wL->wall->vertex1->ID, wL->wall->vertex2->ID, wL->wall->length);
      else if (wL->wall->ID == PORTALEND)
        printf(")");
      else
        printf("%i,", wL->wall->ID);
    }
    printf(":");
    // sector neighbors list
    for ( sL = (struct sList*) s->neighbors->mlh_Head; sL->node.mln_Succ; sL = (struct sList*) sL->node.mln_Succ)
    {
      printf("%i,", sL->sector->ID);
    }
    printf("\n");
  }
}
///

/**************
 * Math stuff *
 **************/
/// Abs()
/*******************************
 * Absolute value for integers *
 *******************************/
LONG Abs(LONG x)
{
  if (x<0) x=-x;
  return (x);
}
///
/// hypoth()
/**************************************************************
 * Take two LONG values that represent the two right edges of *
 * a right triangle and calculate the hypothenus as a LONG.   *
 * This is used in calculating the exact wall lengths         *
 **************************************************************/
LONG hypoth(LONG a, LONG b)
{
  return SPFix(SPSqrt(SPFlt(a*a+b*b)));
}
///

/*****************
 * Utility stuff *
 *****************/
/// sToU()
/*****************************************************************
 * Take a STRPTR (to a string) and convert it to an ULONG value. *
 * This is used for converting values in map files.              *
 *****************************************************************/
ULONG sToU(STRPTR str)
{
  ULONG num = 0;
  ULONG length = strlen(str);
  ULONG factor = 1;
  BYTE digit;

  for (digit = length-1 ; digit >= 0; digit--)
  {
    num    += (str[digit] - '0') * factor;
    factor *= 10;
  }
  return num;
}
///
/// lToL()
/****************************************************
 * Take a STRPTR to a list of walls or sectors      *
 * in string form and convert it to a real (exec)   *
 * list of wall/sector pointers.                    *
 * This is used in "evaluate sector inner lists"    *
 * section of LoadMap().                            *
 ****************************************************/
struct MinList* lToL(STRPTR str, int type)
{
  struct Wall* p;
  struct pList* pNode;
  struct MinList* list = NULL;

  STRPTR start;
  STRPTR portalv2;
  BOOL done = FALSE;
  BOOL fail = FALSE;

  BOOL portal = FALSE;
  BOOL portalEnd = FALSE;
  BOOL portalToBig = FALSE;
  BOOL portalNoBorder = FALSE;
  BYTE portalCount = 0;
  BYTE portalEndCount = 0;
  WORD c=0;

  list = AllocPooled(mapPool, sizeof(struct MinList));
  if (list)
  {
    NewList((struct List*) list);

    while (!done)
    {
      start = &str[c];

      if (portal)
      {
        portalCount++;
        while ( str[c] > 47 && str[c] < 58 ) c++;
        if (str[c]!=',') {printf("Missing vertex separator (,) in portal %i\n", portalCount);
                          fail = TRUE; done = TRUE; continue;}
        str[c++] = NULL;
        portalv2 = &str[c];
        while ( str[c] > 47 && str[c] < 58 ) c++;
        if (str[c]!=';') {printf("Missing vertex separator (;) in portal %i\n", portalCount);
                          fail = TRUE; done = TRUE; continue;}
        str[c++] = NULL;

        pNode = AllocPooled(mapPool, sizeof(struct pList));
        if (pNode)
        {
          p = AllocPooled(mapPool, sizeof(struct Wall));
          if (p)
          {
            p->ID = portalToBig ? PORTALTOBIG : portalNoBorder ? PORTALTOSMALLNOBORDER : PORTALTOSMALL;
            p->vertex1 = findVertex(sToU(start));
            p->vertex2 = findVertex(sToU(portalv2));
            if (p->vertex1 && p->vertex2)
            {
              // Detect the portals orientation and length and fill appropriate members
              if (p->vertex1->X == p->vertex2->X)
              {
                if (p->vertex1->Y > p->vertex2->Y)
                {
                  p->orientation = NORTH;
                  p->length = ((p->vertex1->Y - p->vertex2->Y)/PRC)/UNITWALLLENGTH;
                }
                else
                {
                  p->orientation = SOUTH;
                  p->length = ((p->vertex2->Y - p->vertex1->Y)/PRC)/UNITWALLLENGTH;
                }

              }
              else if (p->vertex1->Y == p->vertex2->Y)
              {
                if (p->vertex2->X > p->vertex1->X)
                {
                  p->orientation = EAST;
                  p->length = ((p->vertex2->X - p->vertex1->X)/PRC)/UNITWALLLENGTH;
                }
                else
                {
                  p->orientation = WEST;
                  p->length = ((p->vertex1->X - p->vertex2->X)/PRC)/UNITWALLLENGTH;
                }
              }
              else
              {
                puts("diagonal portals not implemented yet!");
                fail = TRUE; done = TRUE; continue;
              }
            }

            pNode->ptr = p;

            if (p->vertex1 && p->vertex2) AddMinTail(list, &pNode->node);
            else {printf("Undefined vertex in portal %i\n", portalCount);
                  fail = TRUE; done = TRUE; continue;}
          }
          else {puts("memory allocation failed"); fail = TRUE; done = TRUE; continue;}
        }
        else {puts("memory allocation failed"); fail = TRUE; done = TRUE; continue;}

        portal = FALSE;
        portalToBig = FALSE;
        portalNoBorder = FALSE;
        continue;
      }

      if (portalEnd)
      {
        portalEndCount++;
        pNode = AllocPooled(mapPool, sizeof(struct pList));
        if (pNode)
        {
          p = AllocPooled(mapPool, sizeof(struct Wall));
          if (p)
          {
            p->ID = PORTALEND;
            pNode->ptr = p;
            AddMinTail(list, &pNode->node);
          }
          else {puts("memory allocation failed"); fail = TRUE; done = TRUE; continue;}
        }
        else {puts("memory allocation failed"); fail = TRUE; done = TRUE; continue;}

        portalEnd = FALSE;
        if (str[c] == NULL) { done = TRUE; break; }
        if (str[c] == '(') { str[c] = NULL; portal = TRUE; portalToBig = TRUE; c++; }
        if (str[c] == '[') { str[c] = NULL; portal = TRUE; c++; }
        if (str[c] == '{') { str[c] = NULL; portal = TRUE; portalNoBorder = TRUE; c++; }
        if (str[c] == ']' || str[c] == ')' || str[c] == '}') { str[c] = NULL; portalEnd = TRUE; c++; }
        continue;
      }

      while (TRUE)
      {
        if (str[c] == ',') { str[c] = NULL; break; }
        if (str[c] == '(') { str[c] = NULL; portal = TRUE; portalToBig = TRUE; break; }
        if (str[c] == '[') { str[c] = NULL; portal = TRUE; break; }
        if (str[c] == '{') { str[c] = NULL; portal = TRUE; portalNoBorder = TRUE; break; }
        if (str[c] == ']' || str[c] == ')' || str[c] == '}') { str[c] = NULL; portalEnd = TRUE; break; }
        if (str[c] == NULL) { done = TRUE; break; }
        c++;
      }
      pNode = AllocPooled(mapPool, sizeof(struct pList));
      if (pNode)
      {
        if (type == WALL) pNode->ptr = findWall(sToU(start));
        else if (type == SECTOR) pNode->ptr = findSector(sToU(start));

        if (pNode->ptr) AddMinTail(list, &pNode->node);
        else
        {
          if (type == WALL) printf("Undefined wall %s\n", start);
          else printf("Undefined sector %s\n", start);
          fail = TRUE; break;
        }
      }
      else { fail = TRUE; break; }
      c++;
    }
  }

  if (portalCount != portalEndCount)
  {
    printf("Portals:%i, PortalEnds:%i\n", portalCount, portalEndCount);
    puts("mismatching portal parentheses");
    fail = TRUE;
  }
  if (fail) return (NULL);
return list;
}
///

/*******************
 * Texture Mapping *
 *******************/
///MapTexture()
/*******************************************************************************
 * Paints a pixel on screen with the colour it looks up in the texture bitMap. *
 * NOTE: NOT USED ANYMORE! (inlined in the main loop)                          *
 *******************************************************************************/
__inline VOID MapTexture(struct BitMap* scrBM, struct BitMap* txtBM, LONG x, LONG y, LONG tX, LONG tY)
{
  BYTE plane;
  LONG wOffset = (scrBM->BytesPerRow * y ) + (x  / 8);
  LONG rOffset = (txtBM->BytesPerRow * tY) + (tX / 8);
  BYTE* wByte;                       // The byte to write
  BYTE* rByte;                       // The byte to read
  BYTE wMask = 1 << (7 - ( x % 8));  // bitMask for the pixel to write
  BYTE rMask = 1 << (7 - (tX % 8));  // bitMask for the pixel to read

  for (plane=0; plane<scrBM->Depth; plane++)
  {
    rByte = txtBM->Planes[plane] + rOffset; // this is pointer arithmetic here!
    wByte = scrBM->Planes[plane] + wOffset; // this is pointer arithmetic here!

    //check the bit in texture
    if (*rByte & rMask) *wByte |= wMask; //set the bit in screen BitMap
  }
}
///

/*****************
 * DETECT SECTOR *
 *****************/
///detectSector()
/******************************************************************************
 * Detect the map sector the player is currently in the fastest way possible. *
 * and return its pointer.                                                    *
 *****************************************************************************/
__inline struct Sector* detectSector(LONG pX, LONG pY, struct Sector* currSector)
{
  struct sList* sL = NULL;          // The pointer to traverse sector neighbors list
  BOOL found = FALSE;

  //Check if the player was in a sector before
  if (currSector)
  {
    // Check if the player is still in the same sector:
    // This is an aproximate evaluation. A better one should be written for diagonal walls in the future.
    if (!( pX > currSector->vertex1->X && pY > currSector->vertex1->Y && pX < currSector->vertex4->X && pY < currSector->vertex4->Y ))
    // Find the sector the player is in (USING the previous sectors neighbors)
    {
      found = FALSE;
      for ( sL = (struct sList*) currSector->neighbors->mlh_Head; sL->node.mln_Succ; sL = (struct sList*) sL->node.mln_Succ)
      {
        if ( pX >= sL->sector->vertex1->X && pY >= sL->sector->vertex1->Y && pX <= sL->sector->vertex4->X && pY <= sL->sector->vertex4->Y )
          {currSector = sL->sector; found = TRUE; break;}
      }
      if (!found) currSector = NULL;
    }
  }
  else
  {
    struct Sector* s;  // The pointer to traverse the sectors list
    // Player was not in any sector. Check if he/she went in.
    for (s = (struct Sector*) sectors->mlh_Head; s->node.mln_Succ; s = (struct Sector*) s->node.mln_Succ)
    {
      if ( pX > s->vertex1->X && pY > s->vertex1->Y && pX < s->vertex4->X && pY < s->vertex4->Y )
        {currSector = s; break;}
    }
  }
  return currSector;
}
///

/**************************
 * TRIGONOMETRY FUNCTIONS *
 **************************/
///fillTrig()
/******************************************************************************
 * Fill the given array with sin() values for 18 angles (total 72 angles / 4) *
 ******************************************************************************/
 VOID fillTrig(LONG *array)
 {
   FLOAT pi       = afp("3.1415926535897");
   FLOAT angleInc = SPDiv(SPFlt(180), SPMul(SPFlt(5), pi)); // 5 degrees of angle in radians
   FLOAT PRCf     = SPFlt(PRC);
   BYTE i;

   for (i=0; i<19; i++)
   {
     array[i]=SPFix(SPMul(SPSin(SPMul(SPFlt(i), angleInc)), PRCf));
   }
 }
///
