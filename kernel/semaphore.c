#include <data_type.h>
#include <semaphore.h>

static struct sem_table[MAX_SEMAPHORES];

/* Parameter    : None
 * Return       : None
 * Description  : Should called only by the init code.
 */
void sem_init(void)
{
    uint16 i;

    for (i = 0; i < MAX_SEMAPHORES; i++) {
        sem_table[i].id = sem_table[i].init_count = sem_table[i].count = 0;
        sem_table[i].flag = 0;
        sem_table[i].wtcbp = sem_table[i].atcbp = sem_table[i].owner = NULL;
    }
    return;
}

/* Parameter    : semaphore id, ranges from 1 - MAX_SEMAPHORES
 *                initial count
 *                flag (either SEM_COUNTDOWN or SEM_SYNCHRONISATION)
 * Return       : return code
 * Description  : This call should be made to creat a semaphore. Only tasks
 *                can creat semaphore and only the same task can release them.
 *                The semaphore id is used for subsequent usage between tasks.
 *                For synchronisation semaphore the init_count should be ZERO.
 */
rc_t create_semaphore(semid_t id, uint16 init_count, uint16 flag)
{
    uint16 i;
    cpuirq_t mask;
    rc_t rc = ERR_SEM_RESOURCE;
    struct semaphore *sem = NULL;
 
    mask = irq_save();
    i = MAX_SEM_COUNT;
    if (flag | SEM_SYNCHRONISATION) {
        i = 0;
    }
    if (init_count > i) {
        rc = ERR_SEM_COUNT;
        goto ret;
    }

    for (i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_table[i].id == id) {        /* already created */
            rc = ERR_SEM_DUPLICATE;
            goto ret;
        } else if (sem_table[i].id == 0) {  /* Free */
            rc = 0;
            sem = &sem_table[i];
            sem->id = id;
            sem->init_count = init_count;
            sem->count = init_count;
            sem->flag = flag;
            sem->wtcbp = sem->atcbp = NULL;
            sem->owner = tm_get_tcbptr();
            break;
        }
    }
ret:
    irq_restore(mask);
    return rc; 
}
    

/* Parameter    : semaphore id ranges from 1 - MAX_SEMAPHORES
 * Return       : return code
 * Description  : releasing a semaphore that is being currently used is not 
 *                allowed.
 */
rc_t release_semaphore(semid_t id)
{
    rc_t rc = 0;
    cpuirq_t mask;
    struct semaphore *sem = NULL;

    mask = irq_save();
    for (i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_table[i].id == id) {    /* Found the semaphore to release */
            sem = &sem_table[i];
            break;
        }
    }
    if ((sem->owner != current_task) || sem->wtcbp || sem->atcbp) {
        rc = ERR_SEM_RELEASE_FAIL;
        goto ret;
    }
    sem->id = sem->init_count = sem->count = sem->flag = 0;
    sem->wtcbp = sem->atcbp = sem->owner = NULL;
ret:
    irq_restore(mask);

    return rc;
}

/* Parameter    : semaphore id, ranges from 1 - MAX_SEMAPHORES
 * Return       : return code
 * Description  : This call can be made only by a task.
 */
rc_t sem_take(semid_t id, msec_t tvalue)
{
    cpuirq_t mask;
    rc_t rc = ERR_SEM_UNABLE_TO_ACQUIRE;
    struct tcb *next, *prev;
    struct semaphore *sem = NULL;
    uint16 iter = 2;
    timid_t id;
    
    tm_non_preemptive();
    for (i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_table[i].id == id) {    
            sem = &sem_table[i];
            break;
        }
    }
    tm_preemptive();

    mask = irq_save();
    if (!sem) {
        rc = ERR_SEM_INVALID_ID;
        goto ret;
    }
    current_task->awsem = sem;
    while (iter--) {
        if (sem->count) {   /* Semaphore acquired */
            sem->count--;
            /* FIXME: waitq list is used to maintain the list of task that 
             *        has acquired the semaphore.
             */
            link_waitq(&sem->atcbp, current_task); 
            rc = 0;
            goto ret;
        } 
        if (iter == 0) {
            current_task->awsem = NULL;
            goto ret;
        }
        link_waitq(&sem->wtcbp, current_task);
        timeout(&id, sem_timeout_handler, current_task, tvalue, SEM_WAIT);
        tm_running_to_wait();
        tm_schedule(1);
    }
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : semaphore id, ranges from 1 - MAX_SEMAPHORES
 * Return       : return code
 * Description  : 
 */
rc_t sem_give(semid_t id)
{
    cpuirq_t mask;
    rc_t rc;
    struct tcb *wtcbp;
    struct semaphore *sem = NULL;
    
    mask = irq_save();
    for (i = 0; i < MAX_SEMAPHORES; i++) {
        if (sem_table[i].id == id) {    
            sem = &sem_table[i];
            break;
        }
    }
    if (!sem) {
        rc = ERR_SEM_INVALID_ID;
        goto ret;
    }
    if (!(sem->flags | SEM_SYNCHRONISATION) && (sem->atcbp != current_task)) {
        rc = ERR_SEM_OPERATION_NOT_ALLOWED;
        goto ret;
    }
    sem->count++;
    sem->atcbp = NULL;
    current_task->awsem = NULL;
    wtcbp = sem->wtcbp;
    if (sem->count == 1) {
        while (wtcbp) {
            unlink_waitq(&sem->wtcbp, wtcbp); 
            tm_wait_to_ready(wtcbp);
            /* Check if the task has subscribed for the timeout */
            if (wtcbp->tim) {
                untimeout(wtcbp->tim->id);
                wtcbp->tim = NULL;
            }
            if (!(sem->flag | SEM_SYNCHRONISATION)) {
                break;
            }
            wtcbp = sem->wtcbp;
        }     
    }
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : None 
 * Return       : None
 * Description  : 
 */
static void sem_timeout_handler(void *param)
{
    struct tcb *tcbp;
    cpuirq_t mask;

    mask = irq_save();
    tcbp = (struct *)param;
    tcbp->tim = NULL;
    tm_wait_to_ready(tcbp); 
    unlink_waitq(&sem->wtcbp, current_task);
    irq_restore(mask);
}
