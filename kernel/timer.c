#include <data_type.h>
#include <timer.h>

static msec_t msec_tick = 0;
static sec_t sec_tick = 0;

static timeout_t timer_arr[MAX_SW_TIMERS];

/* The timeout node starts from free_list migrates to active_list then to 
 * expire_list and then back to the free_list. All three list are maintained
 * as circular list.
 */
static timeout_p active_list = NULL;
static timeout_p free_list = &timer_arr[0];
static timeout_p expire_list = NULL;

static void timer_isr(void);

/* Parameter    : None
 * Return       : None
 * Description  : Non-Blocking, Non-reentrant
 */
static void timer_isr(void)
{
    msec_tick++;
    sec_tick += msec_tick / MSEC_PERSEC;
    msec_tick = msec_tick % MSEC_PERSEC;
    bh_activate(BH_TIMEOUT);
    return;
}

/* Parameter    : None
 * Return       : None
 * Description  : Should be called only by init code. Or when ever the whole
 *                of timer subsystem needs to be initialised.
 *                Non-Blocking, Non-reentrant
 */
void timer_init( void )
{
    uint16 i;
    uint8 *block = (uint8 *)timer_arr;
    cpuirq_t mask;

    /* Initialising timer would require to block all the IRQs */
    mask = irq_save();

    /* FIXME : Machine specific initialisation. The machine specific MACROs
     *         and functions will be called here to do the initialisation 
     */

    msec_tick = 0;
    sec_tick = 0;
    for (i = 0; i < MAX_SW_TIMER; i++) {
        timer_arr[i].id = i;
        timer_arr[i].tvalue = timer_arr[i].count = timer_arr[i].flags = 0;
        timer_arr[i].tcbp = timer_arr[i].param = timer_arr[i].handler = NULL;
        timer_arr[i].next = &timer_arr[ (i + 1) % MAX_SW_TIMER ];
    } 

    active_list = NULL;
    free_list = &timer_arr;
    expire_list = NULL;

    /* FIXME: Subscribe the timer isr into the IVT */

    /* Subscribe handler to timeout bottom half */
    rc = subscribe_bhalf(BH_TIMEOUT, timeout_bh, NULL);
    irq_restore( mask );
}

/* Parameter    : None
 * Return       : millisecond count, the value can range from 0-999
 * Description  : Non Blocking, Re-entrant
 */
msec_t get_msectick(void)
{
    return msec_tick;
}

/* Parameter    : None
 * Return       : 32-bit second count. 
 * Description  : Non-Blocking, Re-entrant
 */
sec_t get_sectick(void)
{
    return sec_tick;
}

/* Parameter    : timeout id
 * Return       : pointer to timeout structure
 * Description  : Non Blocking, Re-entrant
 */
timeout_p get_timer_table(timid_t id)
{
    return &timer_arr[id];
}

/* Parameter    : handler, function to be called when the timeout expires
 *                param, parameter to be passed to the handler - can be anything
 *                tvalue, number of millisecond to wait.
 *                flags, ONE_TIME / CYCLIC / BLOCKED / SEM_WAIT 
 * Return       : return code
 * Description  : Reentrant, Non-Blocking
 *                
 */
rc_t timeout(timid_t *id, void (*handler)(void *param), void *param, msec_t tvalue, 
            uint8 flags)
{
    rc_t rc = E_SUCCESS;
    cpuirq_t mask = 0;
    timeout_p make_active;

    /* Exceeded the limit ? */
    if (!free_list) {
        rc = ERR_TIM_EXCEED_TIMERS
        goto ret;
    }

    /* Validate the parameters */ 
#ifdef _DEBUG_
    if (!handler) {
        rc = ERR_TIM_INVALID_HANDLER;
        goto ret;        
    }
    if (!tvalue) {
        rc = ERR_TIM_INVALID_TVALUE;
        goto ret;        
    }
    if (!flags) { 
        rc = ERR_TIM_INVALID_FLAGS;
        goto ret;
    } else if (flags & (flags-1)) { /* The bits are mutualy exclusive
                                     * check for power of two */
        rc = ERR_TIM_INVALID_FLAGS;
        goto ret;
    }
#endif /* _DEBUG_ */ 
         
    /* Critical Section : Take the first node and unlink */
    mask = irq_save();
    make_active = free_list; 
    free_list = make_active->next;
    irq_restore(mask);

    /* This node is now not in any list. Populate it */
    make_active->tvalue = tvalue;
    make_active->tcbp = tm_get_tcbptr();
    make_active->flags = flags;
    make_active->handler = handler;
    make_active->param = param;
    make_active->next = NULL;
    /* FIXME: It is better to move this code to task subsystem */
    if ((flag | BLOCKED) || (flag | SEM_WAIT)) {
        make_active->tcbp->tim = make_active;
    }
    /* Finally compute the count and add it to the active list */
    add_active_list(make_active);
    
    *id = make_active->id;
ret: 
    return rc;
}    

static void add_active_list(timeout_p make_active)
{
    uint32 count = 0;
    timeout_t **list;
    uint32 tvalue = make_active->tvalue;
    cpuirq_t mask;

    /* Critical section : Find and insert into the active list */
    mask = irq_save();
    list = &active_list;

    do {
        count += *list->count;
        if (count > tvalue) {
            count -= *list->count;
            break;
        } 
        list = &(*list)->next;
    } while (*list != active_list)

    make_active->count = tvalue - count;
    make_active->next = *list;
    if (make_active->next) {
        make_active->next->count -= (*list)->count;
    }
    *list = make_active;

    irq_restore(mask);        

    return;
}

/* Parameter    : timeout id.
 * Return       : return code
 * Description  : Exported
 *                Re entrant
 *                Non Blocking
 *                contains critical section code.
 *                
 */
rc_t untimeout(timid_t id)
{
    timeout_p make_free = NULL;
    timeout_t **list;
    cpuirq_t mask; 
    rc_t rc = E_SUCCESS;

    mask = irq_save();
    list = &active_list;
    while (*list) {
        if (*list->id == id) {
            make_free = *list;
            break;
        }
        list = &(*list)->next;
    } 
    if (make_free == NULL) {
        rc = TIM_INVALID_ID;
        goto ret;
    }
    *list = *list->next;
    *list->count += make_free->count;

    init_tim_table(make_free);
    make_free->next = free_list;
    free_list = make_free;

    irq_restore(mask);
ret:
    return rc;
}

static void init_tim_table(timeout_p tim) 
{
    tim->tvalue = 0;
    tim->count = 0;
    tim->flags = 0;
    tim->tcbp = NULL;
    tim->param = NULL;
    tim->handler = NULL;
    tim->next = NULL;
    return;
}
    
/* Parameter    : None
 * Return       : 16 bit millisecond count, the value can range from 0-999
 * Description  : Non Blocking
 */
void timeout_bh(void *param)
{
    cpuir_t mask;
    timeout_p work = NULL, prev = NULL;

    mask = irq_save();
    if(active_list && !active_list->count) {  /* Empty and expired */
        work = active_list; 
        do {
            if (active_list->count) {
                prev->next = NULL;
                break;
            }
            prev = active_list;
            active_list = active_list->next;
        } while (active_list);
    }
    irq_restore(mask);
        
    while (work) {  /* Handle Expired timeout */
        prev = work;
        work = work->next;
        prev->next = NULL;
        (*prev->handler)(param);
        if ((prev->flag | ONE_TIME) || (prev->flag | BLOCKED) || (prev->flag | SEM_WAIT)) {
            init_tim_table(prev);
            mask = irq_save();
            prev->next = free_list;
            free_list = prev;
            irq_restore(mask);
        }
        if (prev->flag | CYCLIC) {
            add_active_list(prev);
        }
    }

    mask = irq_save();
    if (active_list) {
        active_list->count--;
    }
    irq_restore(mask);
ret:
    return;
}
