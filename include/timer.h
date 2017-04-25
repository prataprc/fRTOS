#ifdef _TIMER_H_
#define _TIMER_H_

/* MACROS */
#define MAX_SW_TIMERS   10
#define MSEC_PERSEC     1000

#define ONE_TIME        1
#define CYCLIC          ONE_TIME << 1
#define BLOCKED         CYCLIC << 1
#define SEM_WAIT        SEM_WAIT << 1

/* Table */
struct timeout {
    timid_t id;             /* Unique ID per timeout */
    msec_t tvalue;          /* Subscribed timeout value. */
                            /* tvalue == ZERO means, the table is free */
    msec_t count;           /* count down on the subscribed value */
    uint8 flags;            /* ONE_TIME / CYCLIC / BLOCKED / SEM_WAIT */
    struct tcb *tcb;        /* pointer to tcb table for BLOCKED and SEM_WAIT
                             * type handlers */
    void *param;            /* could be data or could be address */
    void (*handler)(void *param);
    struct timeout *next;
}
typedef struct timeout timeout_t, *timeout_p;

/* Interface functions */
extern void timer_init( void );
extern msec_t get_msectick(void)
extern sec_t get_sectick(void)
#endif /* _TIMER_H_ */
