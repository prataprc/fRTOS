#include <data_type.h>
#include <task.h>

/* Local Definitions */
static struct tcb tasks[NO_OF_TASKS];
static struct tcb *current_task;
static struct tcb *waitq;
static uint16 num_ready;
static uint16 num_wait;
static struct tcb *readyq_priority[MAX_PRIORITY];
    /* About readyq_priority[] array:
     *      The dyn_priority field of the TCB is the index to the array. The 
     * array is maintained only for ready task (if there are no task in ready 
     * list, then all the element in the array should show NULL).
     *  - For each priority level the ready tasks will be linked in circular 
     *    fashion.
     *  - The first task of the same priority (say n) in the ready list will be
     *    stored in readyq_priority[n].
     *  - If a new task is added to the ready list, it will be added at the 
     *    end of the same priority.
     *  - If the task is moved from ready list to running state, then it will 
     *    be removed from the ready list for that priority level.
     */
static uint16 hi_index;
static uint8 user_stack[TOTAL_STACK];
static uint8 *ustack_ptr = user_stack;

/* Parameter    : None
 * Return       : None
 * Description  : Should be called only by the init code
 */
void tm_init(void)
{
    uint16 i;
    uint8 *block = (uint8 *)tasks;
    
    for (i = 0; i < sizeof(tasks); i++) {
        block[i] = 0;
    }
    current_task = 0;
    waitq= 0;
    for (i = 0 ; i < MAX_PRIORITY; i++) {
        readyq_priority[i] = NULL;
    }
    for (i = 0; i < TOTAL_STACK; i++) {
        user_stack[i] = 0;
    }
    ustack_ptr = user_stack + TOTAL_STACK - 1;
    num_ready = num_wait = 0;
    hi_index = 0;

    /* FIXME: The below task initialisation is a must for handling bottom 
     *        halves. Later find a suitable place to do the task init.
     */
    tm_task_init(bh_task, 1, 10, 256);
    return;
}
   
/* Parameter    : task entry point
 *                priority of the task 
 *                time slice for Round robin scheduling.
 *                stack size of the task
 * Return       : return code
 * Description  : This function should be called in the init code, once for
 *                every task.
 *                reentrant. nonblocking. criticalsection used.
 */
rc_t tm_task_init(void (*task_fn)(void), prio_t priority, uint16 time_slice, 
                    ssize_t stack_size)
{
    tc_t rc = 0;
    uint16 i;
    struct tcb *new_task;

    mask = irq_save();
    for (i = 0; i < NO_OF_TASKS; i++) {
        if (tasks[i].id == 0) {                 /* task table is not free */
            break;
        }
    }
    new_task->id = i + 1;

    new_task = &tasks[i];
    new_task->state = TS_READY;
    new_task->stateq_next = new_task->stateq_prev = NULL;
    new_task->waitq_next = new_task->waitq_prev= NULL;
    new_task->priority = new_task->dyn_priority = priority;
    new_task->time_slice = new_task->slice_count = time_slice;
    new_task->flag = 0;
    new_task->ustack = alloc_user_stack(stack_size);
    if (new_task->ustack == NULL) {
        rc = ERR_TASK_USTACK_ALLOC;
        goto ret;
    } 
    new_task->ssize = stack_size;
    /* FIXME : context is to be stored at the bottom of user stack */

    /* new_task->context = */
    new_task->awsem = new_task->tim = NULL;
    new_task->sem_tstamp = 0;
    new_task->wswitch_count = new_task->rswitch_count = 0
    new_task->semswitch_count = new_task->ready_count = 0
    new_task->run_count = new_task->wait_count = new_task->state_tstamp=0;

    /* Add it to the readyq */
    link_readyq(new_task);
    irq_restore(mask);
ret:
    return rc;
}

/* Parameter    : stack size to be allocated.
 * Return       : user stack pointer.
 * Description  : Allocates the stack space for tasks.
 */
/* FIXME : The allocation of user stack is dependant on final memory map.
 *         So, the implementation of the following function might change.
 */
static uint8 * alloc_user_stack(uint16 size)
{
    uint8 *task_stack;

    if ((ustack_ptr - user_stack) < size) {
        return NULL;
    }
    task_stack = ustack_ptr;
    ustack_ptr -= size;
    return task_stack;
} 

/* Parameter    : tcb pointer of a task.
 *                priority to be set for the task, range 1 - MAX_PRIORITY. 
 *                  Priority value less than 8 is reserved for kernel.
 * Return       : return code
 * Description  : Sets the priority of the desired task. If tcbp is NULL then
 *                priority is set for the current_task. Only dyn_priority
 *                value is modified.
 *                reentrant. nonblocking. criticalsection.
 */
rc_t tm_set_priority(struct tcb *tcbp, prio_t priority)
{
    rc_t rc = 0;
    cpuirq_t mask;

    mask = irq_save(); 
    if (priority < 8 || priority >= MAX_PRIORITY) {
        rc = ERR_TASK_PRIORITY;
        goto ret;
    }
    if (!tcbp) {
        tcbp = current_task;
    }
    if (tcbp->state != TS_READY) {
        tcbp->dyn_priority = priority; 
        goto ret;
    }
    /* if the task is in the ready list then the readyq_priority[]
     * should be re-evaluated.
     */
    unlink_readyq(tcbp)
    tcbp->dyn_priority = priority; 
    link_readyq(tcbp)
ret:
    irq_restore(mask); 
    return rc;
}


/* Parameter    : tcb pointer of a task.
 * Return       : None
 * Description  : restores the dyn_priority of the task to priority
 */
void tm_restore_priority(struct tcb *tcbp)
{
    if (!tcbp) {
        tcbp = current_task;
    }
    mask = irq_save(); 
    if (tcbp->state != TS_READY) {
        tcbp->dyn_priority = tcbp->priority; 
        goto ret;
    }
    /* if the task is in the ready list then the list and the readyq_priority
     * should be re-evaluated.
     */
    unlink_readyq(tcbp)
    tcbp->dyn_priority = tcbp->priority; 
    link_readyq(tcbp)
ret:
    irq_restore(mask); 
    return;
}

/* Parameter    : tcb pointer of a task.
 * Return       : priority value. Can range from 1 - MAX_PRIORITY 
 * Description  : gets the dynamic priority of the desired task. If tcbp is 
 *                NULL then priority is obtained for the current_task.
 */
prio_t tm_get_priority(struct tcb *tcbp)
{
    if (!tcbp) {
        tcbp = current_task
    }
    return tcbp->dyn_priority;
}

/* Parameter    : None
 * Return       : task id
 * Description  : returns the task id of the current task
 */
taskid_t tm_get_curr_tid(void)
{
    return current_task->id;
}

/* Parameter    : None
 * Return       : pointer to the task structure
 * Description  : returns the pointer to the task structure of the current task
 */
struct tcb* tm_get_tcbptr(void)
{
    return current_task;
}

/* Parameter    : pointer to the task structure 
 * Return       : remaining time slice
 * Description  : returns the remaining time slice of the tcbp task. If tcbp
 *                is NULL then remaining time slice is returned for the current
 *                task
 */
tslice_t tm_get_slice(struct tcb *tcbp)
{
    if (!tcbp) {
        tcbp = current_task;
    }
    return tcbp->slice_count;
}

/* Parameter    : pointer to the task structure 
 *                time slice value
 * Return       : None
 * Description  : set the time slice of the tcbp task. If tcbp is NULL then 
 *                time slice is set for the current task
 */
void tm_set_slice(struct tcb *tcbp, tslice_t time_slice)
{
    cpuirq_t mask;

    if (!tcbp) {
        tcbp = current_task;
    }
    mask = irq_save();
    tcbp->time_slice = time_slice;
    tcbp->slice_count = time_slice;
    irq_restore(mask);
    return;
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : moves the specified task from wait list to ready list.
 *                can be called in task context or in isr context.
 */
bool tm_is_semacquired(struct tcb *tcbp)
{
    if (!tcbp) {
        tcbp = current_task;
    }
    if (tcbp->flags | SEM_ACQUIRED) {
        return 1;
    } else {
        return 0;
    }
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : moves the specified task from wait list to ready list.
 *                can be called in task context or in isr context.
 */
bool tm_is_preemptive(struct tcb *tcbp)
{
    if (!tcbp) {
        tcbp = current_task;
    }
    if (tcbp->flags | NON_PREEMPTIVE) {
        return 0;
    } else {
        return 1; 
    }
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : moves the specified task from wait list to ready list.
 *                can be called in task context or in isr context.
 */
void tm_non_preemptive(struct tcb *tcbp)
{
    cpuirq_t mask;

    mask = irq_save();
    if (!tcbp) {
        tcbp = current_task;
    }
    tcbp->flags |= NON_PREEMPTIVE;
    irq_restore(mask);
    return;
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : moves the specified task from wait list to ready list.
 *                can be called in task context or in isr context.
 */
void tm_preemptive(struct tcb *tcbp)
{
    cpuirq_t mask;

    mask = irq_save();
    if (!tcbp) {
        tcbp = current_task;
    }
    tcbp->flags &= ~NON_PREEMPTIVE;
    irq_restore(mask);
    return;
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : moves the specified task from wait list to ready list.
 *                can be called in task context or in isr context.
 */
rc_t tm_wait_to_ready(struct tcb *tcbp)
{
    rc_t rc = 0;
    cpuirq_t mask;
    msec_t msecs;

    mask = irq_save();
    if (!tcbp) {
        rc = ERR_TASK_TCB_NULL;
        goto ret;
    }
    unlink_stateq(&waitq, tcbp);
    num_wait--;
    tcbp->state = TS_READY;
    tcbp->rswitch_count++;      /* profile */
    msecs = get_msectick() + (get_sectick() * 1000); 
    tcbp->wait_count += (msecs - tcbp->state_tstamp);
    tcbp->state_tstamp = msecs;

    link_readyq(tcbp);     
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : None
 * Return       : return code
 * Description  : Moves the current task from the running state to wait state.
 */
rc_t tm_running_to_wait(void)
{
    cpuirq_t mask;
    msec_t msecs;
    rc_t = rc;

    mask = irq_save();
    link_stateq(&waitq, current_task);

    current_task->state = TS_WAIT;
    current_task->wswitch_count++;  /* profile */
    msecs = get_msectick() + (get_sectick() * 1000); 
    current_task->run_count += (msecs - current_task->state_tstamp);
    current_task->state_tstamp = msecs;
    num_wait++;
ret:
    irq_restore(mask);
    return rc; 
}

/* Parameter    : force_schedule to indicate a forcible task switch. 
 * Return       : None
 * Description  : This is the scheduler. 
 */
void tm_schedule(uint16 force_schedule)
{
    struct tcb *task = NULL;
    cpuirq_t mask;
    msec_t msecs;

    mask = irq_save();

    /* highest priority task */
    task = readyq_priority[hi_index];
    if (!task && force_schedule) { /* No other task in the ready list */
        enable_irq();
        /* FIXME : It is not good to have potentially unbreakable loops. */
        while (!hi_index);
        task = readq_priority[hi_index];
        disable_irq();
    } 
    if (force_schedule) {    
        goto switch_task;
    }
    if (current_task->flag | NON_PREEMPTIVE) {  /* task is non preemptive */
        goto exit_scheduler;
    }
    if (task->dyn_priority < current_task->dyn_priority) {
        goto sched_ready;
    }
    if (current_task->slice_count) {
        goto exit_scheduler;
    }
    if (task->dyn_priority != current_task->dyn_priority) {
        goto exit_scheduler;
    }
    /* Reload the slice value before moving to the ready list */
    current_task->slice_count = current_task->time_slice;
sched_ready:
    current_task->state = TS_READY;
    msecs = get_msectick() + (get_sectick() * 1000); 
    current_task->run_count += (msecs - current_task->state_tstamp);
    current_task->state_tstamp = msecs;
    link_readyq(current_task);
switch_task:
    unlink_readyq(task);
    task->state = TS_RUNNING;
    msecs = get_msectick() + (get_sectick() * 1000); 
    task->ready_count += (msecs - task->state_tstamp);
    task->state_tstamp = msecs;
    tm_switch(current_task, task);
exit_scheduler:
    irq_restore(mask);
    return;
}

/* Parameter    : pointer to the task structure 
 * Return       : returns to the new application.
 * Description  : tm_switch should check whether irq_path is set, if so then
 *                use kstack and save the context of current_task such that
 *                when the task is switched back it returns to the interrupted
 *                instruction.
 *          
 *                In other cases, just save the current cpu context into the 
 *                current task, so that when it is switched back, it returns
 *                to tm_schedule.
 */
void tm_switch(struct tcb *current_task, struct tcb *next_task)
{
}

/* Parameter    : None
 * Return       : None
 * Description  : Evaluates and updates hi_index.
 */
static void eval_hi_index(void)
{
    hi_index = 0;
    for (i = 1; i < MAX_PRIORITY; i++) {
        if (readyq_priority[i]) {
            hi_index = i;
            break;
        }
    }
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : the task is added to the readyq. The priority array is used
 *                to link the task into the ready list (readyq).
 */
static rc_t link_readyq(struct tcb *tcbp)
{
    rc_t rc = 0;
    cpuirq_t mask;
    struct tcb **list;

    mask = irq_save();
    if (!tcbp) {
        rc = ERR_TASK_RLINK_FAIL;
        goto ret;
    }
    list = &readyq_priority[tcbp->dyn_priority];
    if (link_stateq(list, tcbp)) {
        rc = ERR_TASK_RLINK_FAIL;
        goto ret;
    }
    if (hi_index > tcbp->dyn_priority) {
        hi_index = tcbp->dyn_priority;
    }
    num_ready++;
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : pointer to the task structure 
 * Return       : return code
 * Description  : the task is removed from the ready list
 */
static rc_t unlink_readyq(struct tcb *tcbp)
{

    rc_t rc = 0;
    cpuirq_t mask;
    struct tcb **list;

    mask = irq_save();
    if (!tcbp) {
        rc = ERR_TASK_UNLINK_READYQ_FAIL;
        goto ret;
    }
    list = &readyq_priority[tcbp->dyn_priority];
    if (unlink_stateq(list, tcbp)){
        rc = ERR_TASK_UNLINK_READYQ_FAIL;
        goto ret;
    }
    num_ready--;
    eval_hi_index();
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : pointer to pointer of tcb
 * Return       : return code
 * Description  : Links to the end of the doubly linked circular list
 */
static rc_t link_stateq(struct tcb **list, struct *tcbp)
{
    struct tcb *next, *prev;
    rc_t rc = 0;
    
    if (list == NULL || tcbp == NULL) {
        rc = ERR_TASK_INVALID_PARAM;
        goto ret;
    } 

    if (*list) {
        next = *list;
        prev = *list->stateq_prev;
        tcbp->stateq_prev = prev;
        tcbp->stateq_next = next;
        next->stateq_prev = tcbp;
        prev->stateq_next= tcbp; 
    } else {
        *list = tcbp;
        tcbp->stateq_prev = tcbp;
        tcbp->stateq_next = tcbp;
    }
ret:
    return rc;
}


/* Parameter    : pointer to pointer of tcb
 * Return       : return code
 * Description  : 
 */
static rc_t unlink_stateq(struct tcb **list, struct *tcbp)
{
    struct tcb *next, *prev;
    rc_t rc = 0;
    
    if (list == NULL || tcbp == NULL) {
        rc = ERR_TASK_INVALID_PARAM;
        goto ret;
    } 

    prev = tcbp->waitq_prev;
    next = tcbp->waitq_next;
    prev->waitq_next = next;
    next->waitq_prev = prev;

    tcbp->waitq_next = tcbp->waitq_prev = NULL;
    if (*list == next) {
        *list = NULL;
    } else if (*list == tcbp) {
        *list = next;
    }
ret:
    return rc;
}

/* Parameter    : pointer to pointer of tcb
 * Return       : return code
 * Description  : This function links the node at the end of the doubly
 *                linked circular list.
 */
rc_t link_waitq(struct tcb **list, struct *tcbp)
{
    struct tcb *next, *prev;
    rc_t rc = 0;
    
    if (*list == NULL || tcbp == NULL) {
        rc = ERR_TASK_INVALID_PARAM;
        goto ret;
    } 
    if (*list) {
        next = *list;
        prev = *list->waitq_prev;
        tcbp->waitq_prev = prev;
        tcbp->waitq_next = next;
        next->waitq_prev = tcbp;
        prev->waitq_next= tcbp; 
    } else {
        *list = tcbp;
        tcbp->waitq_prev = tcbp;
        tcbp->waitq_next = tcbp;
    }
ret:
    return rc;
}


/* Parameter    : pointer to pointer of tcb
 * Return       : return code
 * Description  : Unlinks the given done from the list.
 */
rc_t unlink_waitq(struct tcb **list, struct *tcbp)
{
    struct tcb *next, *prev;
    rc_t rc = 0;
    
    if (*list == NULL || tcbp == NULL) {
        rc = ERR_TASK_INVALID_PARAM;
        goto ret;
    } 

    prev = tcbp->waitq_prev;
    next = tcbp->waitq_next;
    prev->waitq_next = next;
    next->waitq_prev = prev;

    tcbp->waitq_next = tcbp->waitq_prev = NULL;
    if (*list == next) {
        *list = NULL;
    } else if (*list == tcbp) {
        *list = next;
    }
ret:
    return rc;
}

/* Parameter    : No of milli second to sleep
 * Return       : return code
 * Description  : 
 */
rc_t tm_sleep(msec_t tvalue)
{
    rc_t rc;
    timid_t id;
    

    mask = irq_save();
    /* subscribe to the timeout */
    if (rc = timeout(&id, tm_timeout_handler, current_task, tvalue, BLOCKED)) {
        goto ret;
    }
    /* move the task to wait list */
    tm_running_to_wait();
    /* force the scheduler */
    tm_schedule(1);                 /* force a task switch */
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : parameter that was subscribed before
 * Return       : None
 * Description  : timeout handler to implement tm_sleep
 */
static void tm_timeout_handler(void *param)
{
    struct tcb *tcbp;

    tcbp->tim = NULL;
    tm_wait_to_ready(tcbp);
    return;
}
