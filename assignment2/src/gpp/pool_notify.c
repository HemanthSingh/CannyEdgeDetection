#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <semaphore.h>
/*  ----------------------------------- DSP/BIOS Link                   */
#include <dsplink.h>

/*  ----------------------------------- DSP/BIOS LINK API               */
#include <proc.h>
#include <pool.h>
#include <mpcs.h>
#include <notify.h>
#if defined (DA8XXGEM)
#include <loaderdefs.h>
#endif

#define VERBOSE
#define DEBUG

/*  ----------------------------------- Application Header              */
#include <pool_notify.h>
//#include <pool_notify_os.h>

#include "pgm_io.h"
#include "canny_edge.h"
#include "hysteresis.h"
#include "neon.h"


/* ---- Specify the fraction of computations to be performed on the NEON ----- */
#define FRAC 35

#if defined (__cplusplus)
extern "C" {
#endif /* defined (__cplusplus) */


/*  ============================================================================
 *  @const   NUM_ARGS
 *
 *  @desc   Number of arguments specified to the DSP application.
 *  ============================================================================
 */
#define NUM_ARGS                       1

/** ============================================================================
 *  @name   SAMPLE_POOL_ID
 *
 *  @desc   ID of the POOL used for the sample.
 *  ============================================================================
 */
#define SAMPLE_POOL_ID                 0

/** ============================================================================
 *  @name   NUM_BUF_SIZES
 *
 *  @desc   Number of buffer pools to be configured for the allocator.
 *  ============================================================================
 */
#define NUM_BUF_SIZES                  1

/** ============================================================================
 *  @const  NUM_BUF_POOL0
 *
 *  @desc   Number of buffers in first buffer pool.
 *  ============================================================================
 */
#define NUM_BUF_POOL0                  1

/*  ============================================================================
 *  @const   pool_notify_INVALID_ID
 *
 *  @desc   Indicates invalid processor ID within the pool_notify_Ctrl structure.
 *  ============================================================================
 */
#define pool_notify_INVALID_ID            (Uint32) -1

/** ============================================================================
 *  @const  pool_notify_IPS_ID
 *
 *  @desc   The IPS ID to be used for sending notification events to the DSP.
 *  ============================================================================
 */
#define pool_notify_IPS_ID                0

/** ============================================================================
 *  @const  pool_notify_IPS_EVENTNO
 *
 *  @desc   The IPS event number to be used for sending notification events to
 *          the DSP.
 *  ============================================================================
 */
#define pool_notify_IPS_EVENTNO           5


/*  ============================================================================
 *  @name   pool_notify_BufferSize
 *
 *  @desc   Size of buffer to be used for data transfer.
 *  ============================================================================
 */
STATIC Uint32  pool_notify_BufferSize ;

/*  ============================================================================
 *  @name   pool_notify_NumIterations
 *
 *  @desc   Number of iterations of data transfer.
 *  ============================================================================
 */
STATIC Uint32  pool_notify_NumIterations ;

/** ============================================================================
 *  @name   pool_notify_DataBuf
 *
 *  @desc   Pointer to the shared data buffer used by the pool_notify sample
 *          application.
 *  ============================================================================
 */
unsigned char * pool_notify_DataBuf = NULL ;
Uint16* databuf16 = NULL;

/** ============================================================================
 *  @func   pool_notify_Notify
 *
 *  @desc   This function implements the event callback registered with the
 *          NOTIFY component to receive notification indicating that the DSP-
 *          side application has completed its setup phase.
 *
 *  @arg    eventNo
 *              Event number associated with the callback being invoked.
 *  @arg    arg
 *              Fixed argument registered with the IPS component along with
 *              the callback function.
 *  @arg    info
 *              Run-time information provided to the upper layer by the NOTIFY
 *              component. This information is specific to the IPS being
 *              implemented.
 *
 *  @ret    None.
 *
 *  @enter  None.
 *
 *  @leave  None.
 *
 *  @see    None.
 *  ============================================================================
 */
STATIC Void pool_notify_Notify (Uint32 eventNo, Pvoid arg, Pvoid info) ;

sem_t sem;
sem_t free_sem;

long long start;
long long dspTime;
long long neonTime;
long long Time0;
long long Time1;
long long Time2;
long long Time3;
long long Time4;
long long Time5;
long long Time6;
long long Time7;


unsigned char *image;
int imageSize;
int rows, cols;           /* The dimensions of the image. */

char filename[32] = "";

/** ============================================================================
 *  @func   pool_notify_Create
 *
 *  @desc   This function allocates and initializes resources used by
 *          this application.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API DSP_STATUS pool_notify_Create (IN Char8 * dspExecutable, IN Char8 * strBufferSize, IN Uint8   processorId)
{
    DSP_STATUS      status     = DSP_SOK  ;
    Uint32          numArgs    = NUM_ARGS ;
    Void *          dspDataBuf = NULL ;
    Uint32          numBufs [NUM_BUF_SIZES] = {NUM_BUF_POOL0, } ;
    Uint32          size    [NUM_BUF_SIZES] ;
    SMAPOOL_Attrs   poolAttrs ;
    Char8 *         args [NUM_ARGS] ;

	#ifdef DEBUG
    printf ("Entered pool_notify_Create ()\n") ;
	#endif

    sem_init(&sem,0,0);
    sem_init(&free_sem,0,0);
    /*
     *  Create and initialize the proc object.
     */
    status = PROC_setup (NULL) ;
    #ifdef VERBOSE
	printf("Attach the dsp\n");
    #endif
     /*
     *  Attach the Dsp with which the transfers have to be done.
     */
    if (DSP_SUCCEEDED (status))
	{
        status = PROC_attach (processorId, NULL) ;
        if (DSP_FAILED (status))
		{
            printf ("PROC_attach () failed. Status = [0x%x]\n", (int)status );
        }
    }
    else
	{
        printf ("PROC_setup () failed. Status = [0x%x]\n", (int)status) ;
    }
    #ifdef VERBOSE
	printf("Open pool \n");
    #endif
    /*
     *  Open the pool.
     */
    if (DSP_SUCCEEDED (status))
	{
        size [0] = pool_notify_BufferSize ;
        poolAttrs.bufSizes      = (Uint32 *) &size ;
        poolAttrs.numBuffers    = (Uint32 *) &numBufs ;
        poolAttrs.numBufPools   = NUM_BUF_SIZES ;
        poolAttrs.exactMatchReq = TRUE ;
        status = POOL_open (POOL_makePoolId(processorId, SAMPLE_POOL_ID), &poolAttrs) ;
        if (DSP_FAILED (status))
		{
            printf ("POOL_open () failed. Status = [0x%x]\n", (int)status );
        }
    }
#ifdef VERBOSE
	printf("Allocate buffer\n");
#endif
    /*
     *  Allocate the data buffer to be used for the application.
     */
    if (DSP_SUCCEEDED (status))
	{
        status = POOL_alloc (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                             (Void **) &pool_notify_DataBuf,
                             pool_notify_BufferSize) ;

        /* Get the translated DSP address to be sent to the DSP. */
        if (DSP_SUCCEEDED (status))
		{
            status = POOL_translateAddr (
                                   POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                                         &dspDataBuf,
                                         AddrType_Dsp,
                                         (Void *) pool_notify_DataBuf,
                                         AddrType_Usr) ;

            if (DSP_FAILED (status))
			{
                printf ("POOL_translateAddr () DataBuf failed."
                                 " Status = [0x%x]\n",
                                 (int)status) ;
            }
        }
        else
		{
            printf ("POOL_alloc() DataBuf failed. Status = [0x%x]\n",(int)status);
        }
    }

    /*
     *  Register for notification that the DSP-side application setup is
     *  complete.
     */
    if (DSP_SUCCEEDED (status))
	{
        status = NOTIFY_register (processorId,
                                  pool_notify_IPS_ID,
                                  pool_notify_IPS_EVENTNO,
                                  (FnNotifyCbck) pool_notify_Notify,
                                  0/* vladms XFER_SemPtr*/) ;
        if (DSP_FAILED (status))
		{
            printf ("NOTIFY_register () failed Status = [0x%x]\n",
                             (int)status) ;
        }
    }


    /*
     *  Load the executable on the DSP.
     */
    if (DSP_SUCCEEDED (status)) {
        args [0] = strBufferSize ;
        {
            status = PROC_load (processorId, dspExecutable, numArgs, args) ;
        }

        if (DSP_FAILED (status)) {
            printf ("PROC_load () failed. Status = [0x%x]\n", (int)status) ;
        }
    }

    /*
     *  Start execution on DSP.
     */
	#ifdef DEBUG
         printf ("Start execution on DSP \n") ;
	#endif

    if (DSP_SUCCEEDED (status)) {
        status = PROC_start (processorId) ;
        if (DSP_FAILED (status)) {
            printf ("PROC_start () failed. Status = [0x%x]\n",
                             (int)status) ;
        }
    }

    /*
     *  Wait for the DSP-side application to indicate that it has completed its
     *  setup. The DSP-side application sends notification of the IPS event
     *  when it is ready to proceed with further execution of the application.
     */
    if (DSP_SUCCEEDED (status)) {
        // wait for initialization
 	#ifdef DEBUG
         printf ("Wait for initialization, sem_wait(&sem) \n") ;
	#endif

        sem_wait(&sem); //there is no point in continuing if DSP is not initialized
    }

    /*
     *  Send notifications to the DSP with information about the address of the
     *  control structure and data buffer to be used by the application.
     *
     */

    #ifdef DEBUG
    printf ("Notify dspDataBuf, Notify pool_notify_BufferSize \n") ;
    printf ("dspDataBuf = %lu, pool_notify_BufferSize = %lu ", (Uint32) dspDataBuf, (Uint32) pool_notify_BufferSize);
     #endif

    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32) dspDataBuf);
    if (DSP_FAILED (status))
	{
        printf ("NOTIFY_notify () DataBuf failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32) ((rows<<16) | cols));
    if (DSP_FAILED (status))
	{
        printf ("NOTIFY_notify () DataBuf failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32) FRAC);
    if (DSP_FAILED (status))
	{
        printf ("NOTIFY_notify () FRAC failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

#ifdef DEBUG
    printf ("Leaving pool_notify_Create ()\n\n") ;
	#endif

    return status ;
}

void unit_init(void)
{
	memcpy(pool_notify_DataBuf, image, imageSize);
	databuf16 = (Uint16*)pool_notify_DataBuf;
}

#include <sys/time.h>


long long get_usec(void);

long long get_usec(void)
{
  long long r;
  struct timeval t;
  gettimeofday(&t,NULL);
  r=t.tv_sec*1000000+t.tv_usec;
  return r;
}

/** ============================================================================
 *  @func   pool_notify_Execute
 *
 *  @desc   This function implements the execute phase for this application.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API DSP_STATUS pool_notify_Execute (IN Uint32 numIterations, Uint8 processorId)
{
    DSP_STATUS  status    = DSP_SOK ;

    unsigned char *nms;
    unsigned char *edge = NULL;
	unsigned short *smoothedIm = NULL;
    short int *delta_x,*delta_y,*magnitude;
    float *dir_radians=NULL;
    char outfilename[128];    /* Name of the output "edge" image */
    int neon_rows;

	#ifdef DEBUG
    printf ("Entered pool_notify_Execute ()\n") ;
	#endif
    printf ("**Current Load Balancing is %d \n", FRAC) ;

    unit_init();

    POOL_writeback (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                    pool_notify_DataBuf,
                    pool_notify_BufferSize);

    //START GAUSSIAN FILTERING

    start = get_usec();
    dspTime= get_usec();
    NOTIFY_notify (processorId,pool_notify_IPS_ID,pool_notify_IPS_EVENTNO,1); //<--tells DSP to start

    float temp;
    temp = rows*FRAC/100;
    neon_rows = (int)temp;

    #ifdef DEBUG
    printf("Neon rows = %d \n", neon_rows);
    #endif
    neonTime= get_usec();

    smoothedIm = gaussian_smooth_neon(pool_notify_DataBuf,neon_rows+8,cols,2.5, rows);

    printf("---NEON execution time %lld us.\n", get_usec()-neonTime);

    sem_wait(&sem); // <--- that DSP is done.

        POOL_invalidate (POOL_makePoolId(0, SAMPLE_POOL_ID),
                     pool_notify_DataBuf,
                     pool_notify_BufferSize);


    #ifdef DEBUG
    Time0 = get_usec();
    #endif


    //memcpy(pool_notify_DataBuf, smoothedIm, cols*neon_rows*sizeof(short int));
    memcpy(&smoothedIm[cols * neon_rows], &databuf16[cols * neon_rows], cols*(rows - neon_rows)*sizeof(short int));
    #ifdef DEBUG
    printf("memcpy execution time %lld us.\n", get_usec()-Time0);
    #endif

    //CONTINUE THE REST

    #ifdef DEBUG
    Time1 = get_usec();
    #endif
    derrivative_x_y((short int *)smoothedIm,rows,cols,&delta_x,&delta_y);
    #ifdef DEBUG
    printf("derrivative execution time %lld us.\n", get_usec()-Time1);
    #endif

	#ifdef RADIANS
    #ifdef DEBUG
    Time2 = get_usec();
    #endif
    radian_direction(delta_x,delta_y,rows,cols,&dir_radians,-1,-1);
    #ifdef DEBUG
    printf("radian direction execution time %lld us.\n", get_usec()-Time2);
    #endif
	#endif /* RADIAN */

    #ifdef DEBUG
    Time3 = get_usec();
    #endif

    if((magnitude = (short *) malloc(rows*cols* sizeof(short))) == NULL)
    {
        fprintf(stderr, "Error allocating the magnitude image.\n");
    }

    #ifdef VERBOSE
    printf("Computing the magnitude of the gradient.\n");
    #endif
    magnitude_x_y(delta_x, delta_y, rows, cols, magnitude);
	#ifdef DEBUG
    printf("magnitude execution time %lld us.\n", get_usec()-Time3);
    #endif

    #ifdef DEBUG
    Time4 = get_usec();
    #endif

    #ifdef VERBOSE
    printf("Computing the non_max_supp function.\n");
    #endif
    if((nms = (unsigned char *) malloc(rows*cols*sizeof(unsigned char)))==NULL)
    {
        fprintf(stderr, "Error allocating the nms image.\n");
    }
    non_max_supp(magnitude, delta_x, delta_y, rows, cols, nms);
    #ifdef DEBUG
    printf("non max supp execution time %lld us.\n", get_usec()-Time4);
    #endif

    #ifdef DEBUG
    Time5 = get_usec();
    #endif
    #ifdef VERBOSE
    printf("Computing the hysteresis.\n");
    #endif
    if( (edge=(unsigned char *)malloc(rows*cols*sizeof(unsigned char))) == NULL )
    {
        fprintf(stderr, "Error allocating the edge image.\n");
        exit(1);
    }
    apply_hysteresis(magnitude, nms, rows, cols, 0.5, 0.5, edge);
    #ifdef DEBUG
    printf("hysteresis execution time %lld us.\n", get_usec()-Time5);
    #endif


    /****************************************************************************
    * Write out the edge image to a file.
    ****************************************************************************/

    #ifdef DEBUG
    Time6 = get_usec();
    #endif
    
    strcpy(outfilename, filename );
    outfilename[strlen(outfilename)-4] = 0;
    sprintf(outfilename, "%s_out.pgm", outfilename);
    
    //#ifdef VERBOSE
    printf("Writing the edge iname in the file %s \n", outfilename);
    //#endif

    if(write_pgm_image(outfilename, edge, rows, cols, "", 255) == 0)
      {
    fprintf(stderr, "Error writing the edge image, %s.\n", outfilename);
    exit(1);
      }

    #ifdef DEBUG
    printf("writing image execution time %lld us.\n", get_usec()-Time6);
    #endif

    #ifdef DEBUG
    Time7 = get_usec();
    #endif

    free(image);


    free(delta_x);
    free(delta_y);
    free(magnitude);
    free(nms);
    free(edge);
	#ifdef DEBUG
    printf("free execution time %lld us.\n", get_usec()-Time7);
#endif

    printf("-----Sum execution time %lld us.\n", get_usec()-start);

  return status ;
}


/** ============================================================================
 *  @func   pool_notify_Delete
 *
 *  @desc   This function releases resources allocated earlier by call to
 *          pool_notify_Create ().
 *          During cleanup, the allocated resources are being freed
 *          unconditionally. Actual applications may require stricter check
 *          against return values for robustness.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API Void pool_notify_Delete (Uint8 processorId)
{
    DSP_STATUS status    = DSP_SOK ;
    DSP_STATUS tmpStatus = DSP_SOK ;

	#ifdef DEBUG
    printf ("Entered pool_notify_Delete ()\n") ;
	#endif

    /*
     *  Stop execution on DSP.
     */
    status = PROC_stop (processorId) ;
    if (DSP_FAILED (status)) {
        printf ("PROC_stop () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Unregister for notification of event registered earlier.
     */
    tmpStatus = NOTIFY_unregister (processorId,
                                   pool_notify_IPS_ID,
                                   pool_notify_IPS_EVENTNO,
                                   (FnNotifyCbck) pool_notify_Notify,
                                   0/* vladms pool_notify_SemPtr*/) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("NOTIFY_unregister () failed Status = [0x%x]\n",
                         (int)status) ;
    }

    /*
     *  Free the memory allocated for the data buffer.
     */
    tmpStatus = POOL_free (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                           (Void *) pool_notify_DataBuf,
                           pool_notify_BufferSize) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("POOL_free () DataBuf failed. Status = [0x%x]\n",
                         (int)status) ;
    }

    /*
     *  Close the pool
     */
    tmpStatus = POOL_close (POOL_makePoolId(processorId, SAMPLE_POOL_ID)) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("POOL_close () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Detach from the processor
     */
    tmpStatus = PROC_detach  (processorId) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("PROC_detach () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Destroy the PROC object.
     */
    tmpStatus = PROC_destroy () ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("PROC_destroy () failed. Status = [0x%x]\n", (int)status) ;
    }

	#ifdef DEBUG
    printf ("Leaving pool_notify_Delete ()\n") ;
	#endif
}


/** ============================================================================
 *  @func   pool_notify_Main
 *
 *  @desc   Entry point for the application
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API Void pool_notify_Main (IN Char8 * dspExecutable, IN Char8 * infilename)
{
    DSP_STATUS status       = DSP_SOK ;
    Uint8      processorId  = 0 ;
    strcpy(filename, infilename);
    char strbuf[32];

    /****************************************************************************
    * Read in the image. This read function allocates memory for the image.
    ****************************************************************************/
    //#ifdef VERBOSE
		printf("Reading the image %s.\n", filename);
    //#endif
	if(read_pgm_image(filename, &image, &rows, &cols) == 0)
    {
        fprintf(stderr, "Error reading the input image, %s.\n", filename);
        exit(1);
    }

	imageSize = rows * cols;
    printf ("rows: %d,  cols: %d \n", rows, cols);
	#ifdef DEBUG
    printf ("========== Sample Application : pool_notify ==========\n") ;
	#endif

    if(dspExecutable != NULL)
	{
        /*
         *  Validate the buffer size and number of iterations specified.
         */
        pool_notify_BufferSize = DSPLINK_ALIGN ( rows * cols * sizeof(Uint16),
                                             DSPLINK_BUF_ALIGN);

		sprintf(strbuf, "%lu", pool_notify_BufferSize);

		#ifdef DEBUG
        printf(" Allocated a buffer of %d bytes\n",(int)pool_notify_BufferSize );
		#endif

        processorId = 0 ;
        if (pool_notify_BufferSize == 0)
		{
            status = DSP_EINVALIDARG ;
            printf ("ERROR! Invalid arguments specified for  ");
            printf ("     Buffer size    = %d\n", (int)pool_notify_BufferSize) ;
        }

        /*
         *  Specify the dsp executable file name and the buffer size for
         *  pool_notify creation phase.
         */
        status = pool_notify_Create (dspExecutable,
                                     strbuf,
                                     0) ;

        if(DSP_SUCCEEDED (status))
		{
            status = pool_notify_Execute (pool_notify_NumIterations, 0) ;
        }
		 sem_wait(&free_sem);
         pool_notify_Delete (processorId) ;

    }
    else
	{
        status = DSP_EINVALIDARG ;
        printf ("ERROR! Invalid arguments specified for  "
                         "pool_notify application\n") ;
    }

    printf ("==========end of main===============================\n") ;
}

/** ----------------------------------------------------------------------------
 *  @func   pool_notify_Notify
 *
 *  @desc   This function implements the event callback registered with the
 *          NOTIFY component to receive notification indicating that the DSP-
 *          side application has completed its setup phase.
 *
 *  @modif  None
 *  ----------------------------------------------------------------------------
 */
STATIC Void pool_notify_Notify (Uint32 eventNo, Pvoid arg, Pvoid info)
{
    #ifdef DEBUG
    printf("Notification %8d \n", (int)info);
    #endif

    switch((int)info)
    {
        case MSG_DSP_INITIALIZED:
            /* Post the semaphore. */
            sem_post(&sem);
            break;
        case MSG_DSP_DONE:
            printf("---DSP execution time %lld us.\n", get_usec()-dspTime);
            sem_post(&sem);
            sem_post(&free_sem); //we can proceed with deletion
            #ifdef VERBOSE
            printf(" Gaussian Ended! %d \n", (int)info);
            #endif
            break;
        case MSG_DSP_MEMORY_ERROR:
            printf("DSP Memory error %d!\n", (int)info);
            break;
        default:
#ifdef DEBUG
            printf(" xxxDEBUG : %d \n", (int)info);
#endif    
   			break;
	}
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
