#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

/* Tables */
struct semaphore {
    semid_t id;             /* ranges from 1 - MAX_SEMAPHORES */
    uint16 init_count;      /* 0 - sychrnisation semaphore */
                            /* 1 - binary semaphore */
                            /* 2-10 counting semaphore */
    uint16 count;            
    uint16 flag;            /* SEM_COUNTDOWN | SEM_SYNCHRONISATION */
    struct tcbp *wtcbp;     /* Tasks waiting on the semaphore */
    struct tcbp *atcbp;     /* Task that have acquired the semaphore */
    struct tcbp *owner;     /* Task that created the semaphore */
}

/* MACROS */
#define MAX_SEMAPHORES  16
#define MAX_SEM_COUNT   10

#define SEM_COUNTDOWN       1
#define SEM_SYNCHRONISATION SEM_COUNTDOWN << 1

#endif /* _SEMAPHORE_H_ */
