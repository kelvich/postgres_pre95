/*
 * execipc.h --
 *	parallel executor inter-process communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	EXECIPCIncluded	/* Include this file only once */
#define EXECIPCIncluded	1

/* ----------------------------
 * struct for locks that support one producer and multiple consumer
 * ----------------------------
 */
struct m1lock {
    int		count;
#ifdef HAS_TEST_AND_SET
    slock_t	waitlock;
#endif
};
typedef struct m1lock M1Lock;

void SetParallelExecutorEnabled();
void SetNumberSlaveBackends();
void ExecImmediateReleaseSemaphore();
void Exec_I();
void Exec_P();
void Exec_V();
void I_Start();
void P_Start();
void V_Start();
void I_Finished();
void V_Finished();
void P_Finished();
void P_FinishedAbort();
void V_FinishedAbort();
void I_SharedMemoryMutex();
void P_SharedMemoryMutex();
void V_SharedMemoryMutex();
void I_Abort();
void P_Abort();
void V_Abort();
void ExecInitExecutorSemaphore();
void ExecSemaphoreOnExit();
void ExecCreateExecutorSharedMemory();
void ExecSharedMemoryOnExit();
void ExecLocateExecutorSharedMemory();
void ExecAttachExecutorSharedMemory();
void InitMWaitOneLock();
void MWaitOne();
void OneSignalM();
#endif	/* !defined(EXECIPCIncluded) */
