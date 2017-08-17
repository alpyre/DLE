/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 14-04-2017

Timer device related stuff
*/

struct timerequest* OpenTimer(VOID);
VOID CloseTimer(struct MsgPort*, struct timerequest*);
