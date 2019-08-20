/*  ----------------------------------- DSP/BIOS Headers            */
#include <std.h>
#include <gbl.h>
#include <log.h>
#include <swi.h>
#include <sys.h>
#include <tsk.h>
#include <pool.h>

/*  ----------------------------------- DSP/BIOS LINK Headers       */
#include <failure.h>
#include <dsplink.h>
#include <platform.h>
#include <notify.h>
#include <bcache.h>
/*  ----------------------------------- Sample Headers              */
#include <pool_notify_config.h>
#include <task.h>

#include <stdlib.h>

Uint32 FRAC = 0;

extern Uint16 MPCSXFER_BufferSize ;

int start; // starting row

static Void Task_notify (Uint32 eventNo, Ptr arg, Ptr info) ;
unsigned short int* gaussian_smooth(unsigned char *image, int rows, int cols);

Int Task_create (Task_TransferInfo ** infoPtr)
{
    Int status    = SYS_OK ;
    Task_TransferInfo * info = NULL ;

    /* Allocate Task_TransferInfo structure that will be initialized
     * and passed to other phases of the application */
    if (status == SYS_OK)
	{
        *infoPtr = MEM_calloc (DSPLINK_SEGID,
                               sizeof (Task_TransferInfo),
                               0) ; /* No alignment restriction */
        if (*infoPtr == NULL)
		{
            status = SYS_EALLOC ;
        }
        else
		{
            info = *infoPtr ;
        }
    }

    /* Fill up the transfer info structure */
    if (status == SYS_OK)
	{
        info->dataBuf       = NULL ; /* Set through notification callback. */
        info->bufferSize    = MPCSXFER_BufferSize ;
        SEM_new (&(info->notifySemObj), 0) ;
    }

    /*
     *  Register notification for the event callback to get control and data
     *  buffer pointers from the GPP-side.
     */
    if (status == SYS_OK)
	{
        status = NOTIFY_register (ID_GPP,
                                  MPCSXFER_IPS_ID,
                                  MPCSXFER_IPS_EVENTNO,
                                  (FnNotifyCbck) Task_notify,
                                  info) ;
        if (status != SYS_OK)
		{
            return status;
        }
    }

    /*
     *  Send notification to the GPP-side that the application has completed its
     *  setup and is ready for further execution.
     */
    if (status == SYS_OK)
	{
        status = NOTIFY_notify (ID_GPP,
                                MPCSXFER_IPS_ID,
                                MPCSXFER_IPS_EVENTNO,
                                MSG_DSP_INITIALIZED) ; /* No payload to be sent. */
        if (status != SYS_OK)
		{
            return status;
        }
    }

    /*
     *  Wait for the event callback from the GPP-side to post the semaphore
     *  indicating receipt of the data buffer pointer and image width and height.
     */
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ;
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ;
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ;

    return status ;
}

unsigned char* buf;
int length, rows, cols;

Int Task_execute (Task_TransferInfo * info)
{
    //wait for semaphore
	SEM_pend (&(info->notifySemObj), SYS_FOREVER);

	//invalidate cache
    BCACHE_inv ((Ptr)buf, length*2, TRUE) ;

    gaussian_smooth(buf, rows, cols);

    BCACHE_wbInv ((Ptr)buf, length*2, TRUE) ;

    //notify that we are done
    NOTIFY_notify(ID_GPP,MPCSXFER_IPS_ID,MPCSXFER_IPS_EVENTNO, MSG_DSP_DONE);

    return SYS_OK;
}

Int Task_delete (Task_TransferInfo * info)
{
    Int    status     = SYS_OK ;
    /*
     *  Unregister notification for the event callback used to get control and
     *  data buffer pointers from the GPP-side.
     */
    status = NOTIFY_unregister (ID_GPP,
                                MPCSXFER_IPS_ID,
                                MPCSXFER_IPS_EVENTNO,
                                (FnNotifyCbck) Task_notify,
                                info) ;

    /* Free the info structure */
    MEM_free (DSPLINK_SEGID,
              info,
              sizeof (Task_TransferInfo)) ;
    info = NULL ;

    return status ;
}

//callback
static Void Task_notify (Uint32 eventNo, Ptr arg, Ptr info)
{
    static int count = 0;
    Task_TransferInfo * mpcsInfo = (Task_TransferInfo *) arg ;

    (Void) eventNo ; /* To avoid compiler warning. */

    count++;
    if (count==1) {
        buf =(unsigned char*)info ;
    }
    if (count==2) {
        rows = ((Uint32)info >> 16) & 0xFFFF;
        cols = (Uint32)info & 0xFFFF;
        length = rows * cols;


NOTIFY_notify (ID_GPP,
                       MPCSXFER_IPS_ID,
                       MPCSXFER_IPS_EVENTNO,
                       rows);

NOTIFY_notify (ID_GPP,
                       MPCSXFER_IPS_ID,
                       MPCSXFER_IPS_EVENTNO,
                       cols);




    }
    if(count==3) {
        FRAC = (Uint32)info;
        start = rows*(float)(FRAC/100.0f)-8;

    }

    SEM_post(&(mpcsInfo->notifySemObj));
}


/*******************************************************************************
* PROCEDURE: gaussian_smooth
* PURPOSE: Blur an image with a gaussian filter.
* NAME: Mike Heath
* DATE: 2/15/96
*******************************************************************************/

unsigned short int* gaussian_smooth(unsigned char *image, int rows, int cols)
{
    int r, c, rr, cc,     /* Counter variables. */
        windowsize,       /* Dimension of the gaussian kernel. */
        center;           /* Half of the windowsize. */
    unsigned int dot,     /* Dot product summing variable. */
          sum,            /* Sum of the kernel weights variable. */
          temp;

    unsigned short int* smoothedim;
    unsigned char * tmp;

    /* A one dimensional gaussian kernel, normalized to fixed point. */
    static unsigned short int kernel[] = {
          416,  1177,  2837,  5830, 10206,
        15226, 19356, 20969, 19356, 15226,
        10206,  5830,  2837,  1177,  416
    };

    static unsigned short int kernel2[] = {
          208,   588,  1418,  2915, 5103,
         7613,  9678, 10484,  9678, 7613,
         5103,  2915,  1418,   588,  208
    };
 
    windowsize = 15;
    NOTIFY_notify (ID_GPP,
                   MPCSXFER_IPS_ID,
                   MPCSXFER_IPS_EVENTNO,
                   start);


    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    center = windowsize / 2;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
   if((tmp = (unsigned char *) malloc((rows-start)*cols* sizeof(unsigned char))) == NULL)
    {
        // out of memory
        NOTIFY_notify (ID_GPP,
                       MPCSXFER_IPS_ID,
                       MPCSXFER_IPS_EVENTNO,
                       MSG_DSP_MEMORY_ERROR);
    }

    smoothedim = (unsigned short *) image;

    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
    for(r=start; r<rows; r++)
    {
        for(c=0; c<cols; c++)
        {
            dot = 0;
            sum = 0;
            for(cc=(-center); cc<=center; cc++)
            {
                if(((c+cc) >= 0) && ((c+cc) < cols))
                {
                    dot += image[r*cols+(c+cc)] * kernel[center+cc];
                    sum += kernel[center+cc];
                }
            }
            tmp [(r-start)*cols+c] = dot/sum;
        }
    }
    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/
    for(c=0; c<cols; c++)
    {
        for(r=8; r<rows-start; r++)
        {
            sum = 0;
            dot = 0;
            for(rr=(-center); rr<=center; rr++)
            {
                if(((r+rr) >= 0) && ((r+rr) < rows))
                {
                    dot += tmp[(r+rr)*cols+c] * kernel[center+rr];
                    sum += kernel[center+rr];
                }
            }
            temp = ((dot*90/sum));
            smoothedim[(r+start)*cols+c] = temp;
        }
    }

    free(tmp);
    return smoothedim;
}
