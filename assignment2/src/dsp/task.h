#if !defined (TASK_)
#define TASK_

/*  ----------------------------------- DSP/BIOS Headers            */
#include <sem.h>

/*  ----------------------------------- Sample Headers              */
#include <pool_notify_config.h>


typedef struct Task_TransferInfo_tag {
    Uint16 *        dataBuf ;
    SEM_Obj         notifySemObj ;
    Uint32          bufferSize ;
} Task_TransferInfo ;

Int Task_create (Task_TransferInfo ** transferInfo) ;

Int Task_execute (Task_TransferInfo * transferInfo) ;

Int Task_delete (Task_TransferInfo * transferInfo) ;

#define MSG_DSP_INITIALIZED 	((Uint32)1)
#define MSG_DSP_DONE			((Uint32)2)
#define MSG_DSP_MEMORY_ERROR	((Uint32)3)


#endif /* !defined (Task_) */
