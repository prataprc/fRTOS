#ifdef _TASK_H_
#define _TASK_H_

/* Tables */
struct tcb {
    taskid_t id;                /* Unique task id 1 - 31 */
    tstate_t state;             /* TS_READY, TS_RUN, TS_WAIT */
    struct tcb *stateq_next;    /* Links either to the readyq or waitq */
    struct tcb *stateq_prev;
    struct tcb *waitq_next;     /* Links to the tasks blocked on the same */
    struct tcb *waitq_prev;     /* entity */
    prio_t priority;            /* Assigned priority 1 - 31*/
    prio_t dyn_priority;        /* Same as 'priority' */
    tslice_t time_slice;        /* Assigned time slice value */
    tslice_t slice_count;       /* count down on time_slice */
    flag_t flag;                /* NON-PREEMPTIVE, SEM_ACQUIRED, SEM_WAITING,
                                 * TIM_WAITING, EVENT_WAITING, EVENT_POSTED */
    void *ustack;               /* Pointer to base of user stack */
    ssize_t ssize;              /* Size of user stack */
    void *context;              /* Saved context area */
    struct semaphore *awsem;    /* Semaphore on which waiting or acquired */
    timeout *tim;               /* timer table for SEM_ACQUIRED, SEM_WAITING, 
                                 * TIM_WAITING */
    time_t sem_tstamp;          /* To measure the ticks the semaphore is kept
                                 * acquired or kept waiting */
    /* Event Management 
    struct event *event_list;   
    uint32 pending_events;      
       Event Management */

    /* Per task profiling */
    uint32 wswitch_count;       /* no of times switched to waitq */
    uint32 rswitch_count;       /* no of times switched to readq */
    uint32 semswitch_count;     /* switched out while SEM_ACQUIRED */
    uint32 ready_count;         /* time (ms) in TS_READY state */
    uint32 run_count;           /* time (ms) in TS_RUN state */
    uint32 wait_count;          /* time (ms) in TS_WAIT state */
    uint32 state_tstamp;        /* This will be updated with the running time
                                 * stamp when the task is switched to a a new
                                 * state */
    /* Per task profiling */
} 

struct tcb_list {
    struct tcb *next;
    struct tcb *prev;
}

/* Macros */
#define MAX_PRIORITY    32      /* 1-7 : Reserved, 8-31 : normal priority */
#define NO_OF_TASKS     16 
#define TOTAL_STACK     512 * NO_OF_TASKS

#define TS_READY        1
#define TS_RUN          TS_READY << 1
#define TS_WAIT         TS_RUN << 1

#define NON_PREEMPTIVE  1
#define SEM_ACQUIRED    NON_PREEMPTIVE << 1
#define SEM_WAITING     SEM_ACQUIRED << 1
#define TIM_WAITING     TIM_ACQUIRED << 1
#define EVENT_WAITING   TIM_WAITING << 1
#define EVENT_POSTED    EVENT_WAITING << 1

#endif /* _TASK_H_ */
