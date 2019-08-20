/** ============================================================================
 *  @file   helloDSP.c
 *
 *  @path
 *
 *  @desc   This is an application which sends messages to the DSP
 *          processor and receives them back using DSP/BIOS LINK.
 *          The message contents received are verified against the data
 *          sent to DSP.
 *
*/
/*  ----------------------------------- DSP/BIOS Link                   */
#include <dsplink.h>

/*  ----------------------------------- DSP/BIOS LINK API               */
#include <proc.h>
#include <msgq.h>
#include <pool.h>

/*  ----------------------------------- Application Header              */
#include <helloDSP.h>
#include <system_os.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timer.h"
#include <stdint.h>

#include <arm_neon.h>

#if defined (__cplusplus)
extern "C"
{
#endif /* defined (__cplusplus) */

/*
 * Uncomment DEBUG if you want debug information, VERIFY does the
 * verification
 */
#define VERIFY
  //#define DEBUG

Timer totalTime;
Timer dsp_only;
Timer serialTime;
Timer neonTime;

Uint8 size;
Uint8 size_n;


    /* Number of arguments specified to the DSP application. */
#define NUM_ARGS 1

    /* Argument size passed to the control message queue */
#define ARG_SIZE 256

    /* ID of the POOL used by helloDSP. */
#define SAMPLE_POOL_ID  0

    /*  Number of BUF pools in the entire memory pool */
#define NUMMSGPOOLS     4

    /* Number of messages in each BUF pool. */
#define NUMMSGINPOOL0   1
#define NUMMSGINPOOL1   2
#define NUMMSGINPOOL2   2
#define NUMMSGINPOOL3   4

#define NUM_ITERATIONS  5
#define MAT_SIZE 128
#define SIZE (MAT_SIZE/2)

/*
 * Data structure for sending quarters of the original matricies
 * 16-bit each
 */
struct mat2x16 {
    int16_t mat1[SIZE][SIZE];
    int16_t mat2[SIZE][SIZE];
};
/*
 * Data structure for recieving back quarters of the product matrix from DSP
 * 32-bit each
 */
struct mat32 {
    int32_t mat1[SIZE][SIZE];
};
/*
 * Union data structure used by ControlMsg
 */
typedef union {
    struct mat2x16 m16;
    struct mat32 m32;
} mat_t;



    /* Control message data structure. */
    /* Must contain a reserved space for the header */
    typedef struct ControlMsg
    {
        MSGQ_MsgHeader header;
        Uint16 command;
        Uint8 size;
        Char8 arg1[ARG_SIZE];
        mat_t mat;
    } ControlMsg;

    NORMAL_API DSP_STATUS helloDSP_Recieve(ControlMsg *msg);

    /**
     * Source matrices are heap- allocated while destination matrices
     * are statically stack allocated
     */
    int16_t *pmat1, *pmat2;
  int32_t *pres,*pres_ver;
    int32_t prod[MAT_SIZE][MAT_SIZE], prod_ver[MAT_SIZE][MAT_SIZE];

    /* Messaging buffer used by the application.
     * Note: This buffer must be aligned according to the alignment expected
     * by the device/platform. */
#define APP_BUFFER_SIZE DSPLINK_ALIGN (sizeof (ControlMsg), DSPLINK_BUF_ALIGN)

    /* Definitions required for the sample Message queue.
     * Using a Zero-copy based transport on the shared memory physical link. */
#if defined ZCPY_LINK
#define SAMPLEMQT_CTRLMSG_SIZE  ZCPYMQT_CTRLMSG_SIZE
    STATIC ZCPYMQT_Attrs mqtAttrs;
#endif

    /* Message sizes managed by the pool */
    STATIC Uint32 SampleBufSizes[NUMMSGPOOLS] =
    {
        APP_BUFFER_SIZE,
        SAMPLEMQT_CTRLMSG_SIZE,
        DSPLINK_ALIGN (sizeof(MSGQ_AsyncLocateMsg), DSPLINK_BUF_ALIGN),
        DSPLINK_ALIGN (sizeof(MSGQ_AsyncErrorMsg), DSPLINK_BUF_ALIGN)
    };

    /* Number of messages in each pool */
    STATIC Uint32 SampleNumBuffers[NUMMSGPOOLS] =
    {
        NUMMSGINPOOL0,
        NUMMSGINPOOL1,
        NUMMSGINPOOL2,
        NUMMSGINPOOL3
    };

    /* Definition of attributes for the pool based on physical link used by the transport */
#if defined ZCPY_LINK
    STATIC SMAPOOL_Attrs SamplePoolAttrs =
    {
        NUMMSGPOOLS,
        SampleBufSizes,
        SampleNumBuffers,
        TRUE   /* If allocating a buffer smaller than the POOL size, set this to FALSE */
    };
#endif

    /* Name of the first MSGQ on the GPP and on the DSP. */
    STATIC Char8 SampleGppMsgqName[DSP_MAX_STRLEN] = "GPPMSGQ1";
    STATIC Char8 SampleDspMsgqName[DSP_MAX_STRLEN] = "DSPMSGQ";

    /* Local GPP's and DSP's MSGQ Objects. */
    STATIC MSGQ_Queue SampleGppMsgq = (Uint32) MSGQ_INVALIDMSGQ;
    STATIC MSGQ_Queue SampleDspMsgq = (Uint32) MSGQ_INVALIDMSGQ;

    /* Place holder for the MSGQ name created on DSP */
    Char8 dspMsgqName[DSP_MAX_STRLEN];

    /* Extern declaration to the default DSP/BIOS LINK configuration structure. */
    extern LINKCFG_Object LINKCFG_config;

    /*
     * Some macro's for different ways of printing the matrices
     */
    #define max(a, b) (a > b ? a : b)
    #define min(a, b) (a < b ? a : b)
    #define _gen_print_matrix(mat, size, k, j, expr, to)    \
        for (k = 0; k < to; k++)                    \
        {                                                \
            printf("\n");                                \
            for (j = 0; j < to; j++)                \
            {                                            \
                printf("\t%d ", expr);                    \
            }                                            \
        }                                                \
        printf("\n");
    #define print_matrix(mat, size, k, j)        _gen_print_matrix(mat, size, k, j, mat[k][j], min(10, size))
    #define print_flat_matrix(mat, size, k, j)    _gen_print_matrix(mat, size, k, j, mat[k * size + j], min(10, size))
    #define print_full_matrix(mat, size, k, j)    _gen_print_matrix(mat, size, k, j, mat[k][j], size)
    
    #define print_new_matrix(mat, size, k, j)    _gen_print_matrix(mat, size, k, j, mat[k*MAT_SIZE+j], size)

#if defined (VERIFY_DATA)
    /** ============================================================================
     *  @func   helloDSP_VerifyData
     *
     *  @desc   This function verifies the data-integrity of given message.
     *  ============================================================================
     */
    STATIC NORMAL_API DSP_STATUS helloDSP_VerifyData(IN MSGQ_Msg msg, IN Uint16 sequenceNumber);
#endif

    /** ============================================================================
     *  @func   helloDSP_Create
     *
     *  @desc   This function allocates and initializes resources used by
     *          this application.
     *
     *  @modif  helloDSP_InpBufs , helloDSP_OutBufs
     *  ============================================================================
     */
    NORMAL_API DSP_STATUS helloDSP_Create(IN Char8* dspExecutable, IN Uint8 processorId)
    {
        DSP_STATUS status = DSP_SOK;
        Uint32 numArgs = NUM_ARGS;
        MSGQ_LocateAttrs syncLocateAttrs;
        Char8* args[NUM_ARGS];

        /*
         * Initialize the timers
         */
        initTimer(&totalTime, "Total execution time");
        initTimer(&dsp_only,  "Total execution time (w/o comm oh.)");
        initTimer(&serialTime,  "Serial execution time");
        initTimer(&neonTime,  "Neon execution time");

        SYSTEM_0Print("Entered helloDSP_Create ()\n");

        /* Create and initialize the proc object. */
        status = PROC_setup(NULL);

        /* Attach the Dsp with which the transfers have to be done. */
        if (DSP_SUCCEEDED(status))
        {
            status = PROC_attach(processorId, NULL);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("PROC_attach () failed. Status = [0x%x]\n", status);
            }
        }

        /* Open the pool. */
        if (DSP_SUCCEEDED(status))
        {
            status = POOL_open(POOL_makePoolId(processorId, SAMPLE_POOL_ID), &SamplePoolAttrs);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("POOL_open () failed. Status = [0x%x]\n", status);
            }
        }
        else
        {
            SYSTEM_1Print("PROC_setup () failed. Status = [0x%x]\n", status);
        }

        /* Open the GPP's message queue */
        if (DSP_SUCCEEDED(status))
        {
            status = MSGQ_open(SampleGppMsgqName, &SampleGppMsgq, NULL);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("MSGQ_open () failed. Status = [0x%x]\n", status);
            }
        }

        /* Set the message queue that will receive any async. errors */
        if (DSP_SUCCEEDED(status))
        {
            status = MSGQ_setErrorHandler(SampleGppMsgq, POOL_makePoolId(processorId, SAMPLE_POOL_ID));
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("MSGQ_setErrorHandler () failed. Status = [0x%x]\n", status);
            }
        }

        /* Load the executable on the DSP. */
        if (DSP_SUCCEEDED(status))
        {
            args [0] = "5";
            {
                status = PROC_load(processorId, dspExecutable, numArgs, args);
            }
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("PROC_load () failed. Status = [0x%x]\n", status);
            }
        }

         SYSTEM_0Print("  Executable loaded onto DSP! \n");

        /* Start execution on DSP. */
        if (DSP_SUCCEEDED(status))
        {
            status = PROC_start(processorId);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("PROC_start () failed. Status = [0x%x]\n", status);
            }
        }

        /* Open the remote transport. */
        if (DSP_SUCCEEDED(status))
        {
            mqtAttrs.poolId = POOL_makePoolId(processorId, SAMPLE_POOL_ID);
            status = MSGQ_transportOpen(processorId, &mqtAttrs);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("MSGQ_transportOpen () failed. Status = [0x%x]\n", status);
            }
        }

        /* Locate the DSP's message queue */
        /* At this point the DSP must open a message queue named "DSPMSGQ" */
        if (DSP_SUCCEEDED(status))
        {
            syncLocateAttrs.timeout = WAIT_FOREVER;
            status = DSP_ENOTFOUND;
            SYSTEM_2Sprint(dspMsgqName, "%s%d", (Uint32) SampleDspMsgqName, processorId);
            while ((status == DSP_ENOTFOUND) || (status == DSP_ENOTREADY))
            {
                status = MSGQ_locate(dspMsgqName, &SampleDspMsgq, &syncLocateAttrs);
                if ((status == DSP_ENOTFOUND) || (status == DSP_ENOTREADY))
                {
                    SYSTEM_Sleep(100000);
                }
                else if (DSP_FAILED(status))
                {
                    SYSTEM_1Print("MSGQ_locate () failed. Status = [0x%x]\n", status);
                }
            }
        }

        SYSTEM_0Print("  DSP opened a message queue named \"DSPMSGQ\" \n");
        SYSTEM_0Print("Leaving helloDSP_Create ()\n");
        return status;
    }



    inline void MAC4 (int32x4_t *additive_value, int16x4_t *data1, int16x4_t *data2,int32x4_t *mac_output)
    {
        *mac_output = vmlal_s16 (*additive_value,*data1, *data2);
    }

    /** ============================================================================
     *  @func   helloDSP_Execute
     *
     *  @desc   This function implements the execute phase for this application.
     *
     *  @modif  None
     *  ============================================================================
     */
    NORMAL_API DSP_STATUS helloDSP_Execute(Uint8 processorId)
    {
        DSP_STATUS status = DSP_SOK;
        Uint16 sequenceNumber = 0;
        Uint16 msgId = 0;
        Uint32 i, j, l;
#ifdef DEBUG
        Uint32 k;
#endif

        ControlMsg *msg;

        SYSTEM_0Print("Entered helloDSP_Execute ()\n");

#if defined (PROFILE)
        SYSTEM_GetStartTime();
#endif

        for (i = 1 ; ((NUM_ITERATIONS == 0) || (i <= (NUM_ITERATIONS + 1))) && (DSP_SUCCEEDED (status)); i++)
        {
            /* Receive the message. */
#ifdef DEBUG
            printf("Waiting for message %u\n", i);
#endif
            status = MSGQ_get(SampleGppMsgq, WAIT_FOREVER, (MsgqMsg *) &msg);
            if (DSP_FAILED(status))
            {
                SYSTEM_1Print("MSGQ_get () failed. Status = [0x%x]\n", status);
            }
#if defined (VERIFY_DATA)
            /* Verify correctness of data received. */
            if (DSP_SUCCEEDED(status))
            {
                status = helloDSP_VerifyData(msg, sequenceNumber);
                if (DSP_FAILED(status))
                {
                    MSGQ_free((MsgqMsg) msg);
                }
            }
#endif

#ifdef DEBUG
            if (msg->command == 0x01)
                SYSTEM_1Print("Message received: %s\n", (Uint32) msg->arg1);
            else if (msg->command == 0x02)
                SYSTEM_1Print("Message received: %s\n", (Uint32) msg->arg1);
#endif
            /*
             * Message one before last
             * here we recieve 1/2 of 1/2 of the product from the DSP
             */
            if ((NUM_ITERATIONS != 0) && (i == (NUM_ITERATIONS)))
            {
                /*
                 * Stop all timers, since DSP is already done it's cauclations
                 */
                printf("\nTiming information:\n\n");
                stopTimer(&totalTime);
                stopTimer(&dsp_only);
                //printf("Parallel calculation time including communication overhead is: \n");
                printTimer(&totalTime);
                //printf("DSP Time only: \n");
                printTimer(&dsp_only);

                /*
                 * Fill in the product matrix here (on GPP) with the results from DSP
                 */
                for (l = 0;l < size/2; l++)
                {
                    for (j = 0; j < size/2; j++)
                    {
                        prod[l][j] = msg->mat.m32.mat1[l][j];
                    }
                }
#ifdef DEBUG
                printf("\nMessage #1 back from DSP: msg->mat.m32.mat1 \n");
                print_matrix(msg->mat.m32.mat1, SIZE, k, j);
                //print_full_matrix(prod, MAT_SIZE, k, j);
#endif
                status = MSGQ_put(SampleDspMsgq, (MsgqMsg) msg); //just need to send some stuff
                if (DSP_FAILED(status))
                {
                    MSGQ_free((MsgqMsg) msg);
                    SYSTEM_1Print("MSGQ_put () failed. Status = [0x%x]\n", status);
                }
            }
            /*
             * Final message from the DSP
             * here we recieve the second 1/2 of 1/2 of the product from the DSP
             */
            else if ((NUM_ITERATIONS != 0) && (i == (NUM_ITERATIONS + 1)))
            {
                for (l = 0;l < size/2; l++)
                {
                    for (j = 0; j < size/2; j++)
                    {
                        prod[l][j+size/2] = msg->mat.m32.mat1[l][j];
                    }
                }

#ifdef DEBUG
                printf("\nMessage #2 back from DSP: msg->mat.m32.mat1 \n");
                print_matrix(msg->mat.m32.mat1, SIZE, k, j);
#endif

#ifdef VERIFY
                printf("%s\n\n", (helloDSP_VerifyCalculations() ? "SUCCESS!" : "Failure!"));
#endif

#ifdef DEBUG
                printf("Calculated product: \n");
                print_matrix(prod, MAT_SIZE, k, j);

                printf("Correct product: \n");
                print_matrix(prod_ver, MAT_SIZE, k, j);
#endif
                MSGQ_free((MsgqMsg) msg);
            }
            else   //the first four iterations
            {
                /* Send the same message received in earlier MSGQ_get () call. */
                if (DSP_SUCCEEDED(status))
                {
                    msgId = MSGQ_getMsgId(msg);
                    MSGQ_setMsgId(msg, msgId);

                    if (i==1)
                    {
						startTimer(&totalTime); // START the overall timer
						msg->size = size;
					}
					
                    /* Sending the four quarters, one in each iteration */
                    memcpy(msg->mat.m16.mat1, (pmat1 + (i-1)*SIZE*SIZE), SIZE*SIZE*sizeof(int16_t));
                    memcpy(msg->mat.m16.mat2, (pmat2 + (i-1)*SIZE*SIZE), SIZE*SIZE*sizeof(int16_t));

                    status = MSGQ_put(SampleDspMsgq, (MsgqMsg) msg);

                    if (DSP_FAILED(status))
                    {
                        MSGQ_free((MsgqMsg) msg);
                        SYSTEM_1Print("MSGQ_put () failed. Status = [0x%x]\n", status);
                    }

                    if (i == 4) //Sent fourth quarter
                    {
                        performMultiplications();
                    }
                }

                sequenceNumber++;
                /* Make sure that the sequenceNumber stays within the permitted
                 * range for applications. */
                if (sequenceNumber == MSGQ_INTERNALIDSSTART)
                {
                    sequenceNumber = 0;
                }

#if !defined (PROFILE)
                if (DSP_SUCCEEDED(status) && ((i % 100) == 0))
                {
                    SYSTEM_1Print("Transferred %ld messages\n", i);
                }
#endif
            }
        }

#if defined (PROFILE)
        if (DSP_SUCCEEDED(status))
        {
            SYSTEM_GetEndTime();
        }
#endif

        SYSTEM_0Print("Leaving helloDSP_Execute ()\n");

        return status;
    }


    /** ============================================================================
     *  @func   helloDSP_Delete
     *
     *  @desc   This function releases resources allocated earlier by call to
     *          helloDSP_Create ().
     *          During cleanup, the allocated resources are being freed
     *          unconditionally. Actual applications may require stricter check
     *          against return values for robustness.
     *
     *  @modif  None
     *  ============================================================================
     */
    NORMAL_API Void helloDSP_Delete(Uint8 processorId)
    {
        DSP_STATUS status = DSP_SOK;
        DSP_STATUS tmpStatus = DSP_SOK;

        SYSTEM_0Print("Entered helloDSP_Delete ()\n");

        /* Release the remote message queue */
        status = MSGQ_release(SampleDspMsgq);
        if (DSP_FAILED(status))
        {
            SYSTEM_1Print("MSGQ_release () failed. Status = [0x%x]\n", status);
        }

        /* Close the remote transport */
        tmpStatus = MSGQ_transportClose(processorId);
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("MSGQ_transportClose () failed. Status = [0x%x]\n", status);
        }

        /* Stop execution on DSP. */
        tmpStatus = PROC_stop(processorId);
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("PROC_stop () failed. Status = [0x%x]\n", status);
        }

        /* Reset the error handler before deleting the MSGQ that receives */
        /* the error messages.                                            */
        tmpStatus = MSGQ_setErrorHandler(MSGQ_INVALIDMSGQ, MSGQ_INVALIDMSGQ);

        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("MSGQ_setErrorHandler () failed. Status = [0x%x]\n", status);
        }

        /* Close the GPP's message queue */
        tmpStatus = MSGQ_close(SampleGppMsgq);
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("MSGQ_close () failed. Status = [0x%x]\n", status);
        }

        /* Close the pool */
        tmpStatus = POOL_close(POOL_makePoolId(processorId, SAMPLE_POOL_ID));
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("POOL_close () failed. Status = [0x%x]\n", status);
        }

        /* Detach from the processor */
        tmpStatus = PROC_detach(processorId);
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("PROC_detach () failed. Status = [0x%x]\n", status);
        }

        /* Destroy the PROC object. */
        tmpStatus = PROC_destroy();
        if (DSP_SUCCEEDED(status) && DSP_FAILED(tmpStatus))
        {
            status = tmpStatus;
            SYSTEM_1Print("PROC_destroy () failed. Status = [0x%x]\n", status);
        }

        SYSTEM_0Print("Leaving helloDSP_Delete ()\n");
    }


    /** ============================================================================
     *  @func   helloDSP_Main
     *
     *  @desc   Entry point for the application
     *
     *  @modif  None
     *  ============================================================================
     */

    NORMAL_API Void helloDSP_Main(IN Char8* dspExecutable, IN Char8* strProcessorId)
    {
      int i,i2=0;
      int j,j2=0;

        DSP_STATUS status = DSP_SOK;
        Uint8 processorId = 0;
        pmat1 = malloc(MAT_SIZE * MAT_SIZE * sizeof(int16_t));
        pmat2 = malloc(MAT_SIZE * MAT_SIZE * sizeof(int16_t));
        pres = malloc(MAT_SIZE * MAT_SIZE * sizeof(int32_t));
        if (pmat1 == NULL || pmat2 == NULL || pres == NULL) {
            printf("Out of memory\n");
        }

	size_n = (size%4) ? size+4-size%4 : size;

	///////////////////////////////////////////

        /*
         * Generating initial matricies
         */
       
        for (i = 0; i < size_n; i++) //row
        {
	    for (j = 0; j < size_n; j++)  //column
	    {
	        if((i>=size)||(j>=size))
		{
		    pmat1[i*MAT_SIZE+j]=0;
		    pmat2[i*MAT_SIZE+j]=0;
		}else{
#ifdef DEBUG
		    pmat1[i*MAT_SIZE+j] = j2;
		    pmat2[i*MAT_SIZE+j] = (i  == j) ; //identity matrix
#else
		    pmat1[i*MAT_SIZE+j] = j2*13+12;
		    pmat2[i*MAT_SIZE+j] = j2*7+12;
#endif
		    j2++;
		}
	    }
	}
	size+=size%2;
       
   /*     for (i = 0; i < MAT_SIZE * MAT_SIZE; i++)
        {
#ifdef DEBUG
            pmat1[i] = i;//i;
            //mat2[i][j] = i*MAT_SIZE+j + MAT_SIZE*MAT_SIZE;
            pmat2[i] = (i % MAT_SIZE == i / MAT_SIZE) ; //identity matrix
#else
            pmat1[i] = i*13+1;
            pmat2[i] = i*7+1;
#endif
        }
        * 
        * */

#ifdef DEBUG
        SYSTEM_0Print ("========== Initial matricies ==========\n");
        print_new_matrix(pmat1, size_n, i, j);
        print_new_matrix(pmat2, size_n, i, j);
#endif

        SYSTEM_0Print ("========== Matrix Multiplication ==========\n");

        if ((dspExecutable != NULL))
        {
            if (NUM_ITERATIONS > 0xFFFF)
            {
                status = DSP_EINVALIDARG;
                SYSTEM_1Print("ERROR! Invalid arguments specified for helloDSP application.\n Max iterations = %d\n", 0xFFFF);
            }
            else
            {
                processorId = SYSTEM_Atoi(strProcessorId);

                if (processorId >= MAX_DSPS)
                {
                    SYSTEM_1Print("== Error: Invalid processor id %d specified ==\n", processorId);
                    status = DSP_EFAIL;
                }
                /* Specify the dsp executable file name for message creation phase. */
                if (DSP_SUCCEEDED(status))
                {
                    status = helloDSP_Create(dspExecutable, processorId);

                    /* Execute the message execute phase. */
                    if (DSP_SUCCEEDED(status))
                    {
                        status = helloDSP_Execute(processorId);
                    }

                    /* Perform cleanup operation. */
                    helloDSP_Delete(processorId);
                }
            }
        }
        else
        {
            status = DSP_EINVALIDARG;
            SYSTEM_0Print("ERROR! Invalid arguments specified for helloDSP application\n");
        }
        SYSTEM_0Print ("====================================================\n");
    }

#if defined (VERIFY_DATA)
    /** ============================================================================
     *  @func   helloDSP_VerifyData
     *
     *  @desc   This function verifies the data-integrity of given buffer.
     *
     *  @modif  None
     *  ============================================================================
     */
    STATIC NORMAL_API DSP_STATUS helloDSP_VerifyData(IN MSGQ_Msg msg, IN Uint16 sequenceNumber)
    {
        DSP_STATUS status = DSP_SOK;
        Uint16 msgId;

        /* Verify the message */
        msgId = MSGQ_getMsgId(msg.header);
        if (msgId != sequenceNumber)
        {
            status = DSP_EFAIL;
            SYSTEM_0Print("ERROR! Data integrity check failed\n");
        }

        return status;
    }
#endif /* if defined (VERIFY_DATA) */

     /** ============================================================================
     *  @func   helloDSP_VerifyCalculations
     *
     *  @desc   This function verifies if the calculated product is equal
     *             to the actual product
     *  @ret    success
     *                 The products match!
     *             !success
     *                 Failure. Products do not match
     *
     *  ============================================================================
     */
    NORMAL_API int helloDSP_VerifyCalculations(void)
    {
        int l, j, k, success;
        int16x4_t data1;
        int32x4_t mac_output[MAT_SIZE/4];
        int32x4_t MAC_addvalue[MAT_SIZE/4];
        int16x4_t constant_value;
        unsigned int index_input = 0;
        unsigned int transfer_index = 0 ;

        pres_ver = malloc(MAT_SIZE * MAT_SIZE * sizeof(int32_t));

        // --Start Timer
        startTimer(&serialTime);
        for (l = 0;l < size; l++)
        {
            for (j = 0; j < size; j++)
            {
                prod_ver[l][j]=0;
                for(k=0; k<size;k++)
                {
                    prod_ver[l][j] = prod_ver[l][j]+pmat1[l * MAT_SIZE + k] * pmat2[k * MAT_SIZE + j];
                }
            }
        }
        // -- Stop Timer
        stopTimer(&serialTime);
        //printf("Serial calculation time is: \n");
        printTimer(&serialTime);
	
        startTimer(&neonTime);
        for(l = 0 ; l < size_n/4; l++)
        {
            MAC_addvalue[l] = vmovq_n_s32(0);
        }

        for(l = 0; l < size_n*size_n; l++)
        {
            constant_value = vmov_n_s16 (pmat1[l]);
            for(k = 0 ; k < size_n/4 ; k++)
            {
                data1 = vld1_s16 (&pmat2[index_input]);
                MAC4 (&MAC_addvalue[k], &constant_value, &data1,&mac_output[k]);
                MAC_addvalue[k] = mac_output[k];
                index_input +=4;
            }
	    index_input+=MAT_SIZE-size_n;
            if ((l + 1) % size_n == 0 )
            {
                index_input = 0;

                for(k = 0 ; k < size_n/4 ; k++)
                {
                    vst1q_s32(&pres_ver[transfer_index],MAC_addvalue[k]);
                    transfer_index +=4;
                }
		transfer_index += MAT_SIZE-size_n;
                for(k = 0 ; k < size_n/4; k++)
                {
                    MAC_addvalue[k] = vmovq_n_s32(0);
                }
            }
        }

        stopTimer(&neonTime);
        printTimer(&neonTime);
	
        printf("Verification: \n");
        success = 1;
        for (k = 0; (k < size) && success; k++)
        {
            for (j = 0; j < size; j++)
            {
              if ((prod_ver[k][j] != prod[k][j])&&(prod_ver[k][j] != pres_ver[k*MAT_SIZE+j]))
                {
                   success = 0;
                   break;
                }
            }
        }
        return success;
    }

    NORMAL_API DSP_STATUS helloDSP_Recieve(ControlMsg *msg)
    {
        DSP_STATUS status = DSP_SOK;
        /* Receive the message. */
        status = MSGQ_get(SampleGppMsgq, WAIT_FOREVER, (MsgqMsg *) &msg);
        if (DSP_FAILED(status))
        {
            SYSTEM_1Print("MSGQ_get () failed. Status = [0x%x]\n", status);
        }
#if defined (VERIFY_DATA)
        /* Verify correctness of data received. */
        if (DSP_SUCCEEDED(status))
        {
            status = helloDSP_VerifyData(msg, sequenceNumber);
            if (DSP_FAILED(status))
            {
                MSGQ_free((MsgqMsg) msg);
            }
        }
#endif
        return status;
    }

    NORMAL_API void performMultiplications(void)
    {
        int l, k;
        int16x4_t data1;
        int32x4_t mac_output[MAT_SIZE/4];
        int32x4_t MAC_addvalue[MAT_SIZE/4];
        int16x4_t constant_value;
        unsigned int index_input = 0;
        unsigned int index_input2 = 0;//size*MAT_SIZE/2;
        unsigned int transfer_index = 0 ;
	Uint8 size2;
	
	index_input2 = size*MAT_SIZE/2;

        startTimer(&dsp_only); // START TIMER for DSP

        for(l = 0 ; l < size_n/4; l++)
        {
            MAC_addvalue[l] = vmovq_n_s32(0);
        }

        /* here the multiplications will be done */
        for(l = 0; l < size_n*(size/2+(size_n-size)); l++)
        {
            constant_value = vmov_n_s16 (pmat1[index_input2++]);
            for(k = 0 ; k < size_n/4 ; k++)
            {
                data1 = vld1_s16 (&pmat2[index_input]);
                MAC4 (&MAC_addvalue[k], &constant_value, &data1,&mac_output[k]);
                MAC_addvalue[k] = mac_output[k];
                index_input +=4;
            }
	    index_input+=MAT_SIZE-size_n;
            if ((l + 1) % size_n == 0 )
            {
                index_input = 0;
		index_input2 += MAT_SIZE-size_n;

                for(k = 0 ; k < size_n/4 ; k++)
                {
                    vst1q_s32(&pres[transfer_index],MAC_addvalue[k]);
                    transfer_index +=4;
                }
		transfer_index += MAT_SIZE-size_n;
                for(k = 0 ; k < size_n/4; k++)
                {
                    MAC_addvalue[k] = vmovq_n_s32(0);
                }
            }
        }

#ifdef DEBUG
        SYSTEM_0Print ("========== NEON Result ==========\n");
        print_new_matrix(pres, size_n, l, k);
#endif

        for (l = size/2;l < size; l++)
        {
          for (k = 0; k < size; k++)
          {
            prod[l][k] = pres[(l-size/2)*MAT_SIZE+k];
          }
        }

        //Without NEON!//
        //Do the multiplication on the GPP side!
        /*
        for (l = SIZE;l < MAT_SIZE; l++)
        {
            for (j = 0; j < MAT_SIZE; j++)
            {
                prod[l][j]=0;
                for(k=0;k<MAT_SIZE;k++)
                    prod[l][j] = prod[l][j]+mat1[l][k] * mat2[k][j];
            }
        }
        */
    }

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
