/* $Header$ */


/* don't include twice */

#ifndef _PROCQ_H_
#define _PROCQ_H_

#include "storage/shmem.h"


typedef struct procQueue {
  SHM_QUEUE	links;
  int		size;
} PROC_QUEUE;

#endif _PROCQ_H_
