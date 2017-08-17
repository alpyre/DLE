/* -----------------------------------------------------------------------------
A DoomLike 3D graphics engine by Alpyre 14-04-2017

Timer device related stuff
*/

#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

static BYTE error;

/// CloseTimer()
VOID CloseTimer(struct MsgPort* timerMP, struct timerequest* timerReq)
{
  if (!error)
    CloseDevice((struct IORequest*) timerReq);

  if (timerReq)
    DeleteExtIO((struct IORequest*) timerReq);

  if (timerMP)
    DeletePort(timerMP);
}
///
/// OpenTimer()
/********************************************************************************
 * Creates a timerrequest to get a message from timer.device every to 2 seconds *
 * (used in calculating FPS)                                                    *
 ********************************************************************************/
struct timerequest* OpenTimer(VOID)
{
  struct timerequest* timerReq = NULL;
  struct MsgPort*     timerMP  = NULL;

  // Open the messageport for timer device messages
  timerMP = CreatePort(NULL, 0);
  if (!timerMP) {puts("Couldn't create MsgPort for timer."); return NULL;}

  // Create the message
  timerReq = (struct timerequest*) CreateExtIO(timerMP, sizeof(struct timerequest));
  if (!timerReq)
  {
    puts("Couldn't create MsgPort for timer.");
    CloseTimer(timerMP, timerReq);
    return NULL;
  }

  // Open timer device
  error = OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest*) timerReq, NULL);
  if (error)
  {
    puts("Coudn't open timer device.");
    CloseTimer(timerMP, timerReq);
    return NULL;
  }

  /* Set request for a 2 seconds alarm */
  timerReq->tr_node.io_Command = TR_ADDREQUEST;
  timerReq->tr_time.tv_secs   = 2;
  timerReq->tr_time.tv_micro  = 0;

  return timerReq;
}
///
