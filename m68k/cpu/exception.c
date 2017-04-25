#include <data_type.h>
#include <exception.h>
#include <sem.h>

/* Global Definitions */
struct bhalf bh_arr[MAX_BHALF];
uint32  active_bh;
uint16 irq_path;
    /* FIXME : This variable is set in return_from_isr() to indicate 
     *         tm_switch(). tm_switch() will reset this variable.
     */

/* Function Definitions */

/* Parameter    : None
 * Return       : None
 * Description  : To be called only by the kernel init code.
 */
void exception_init(void)
{
    uint16 i;
    
    for (i = 0; i < MAX_BHALF ;i++) {
        bh_arr[i].param = 0;
        bh_arr[i].handler = 0;
        bh_arr[i].flag = 0;
    }
    active_bh = 0;
    irq_path = 0;
    return;
}

/* Parameter    : irq number, ranges from 1-7
 * Return       : None
 * Description  : Every IRQ handler should call this function
 *                at the end of the handler.
 *                It is assumed that the ISR does not change the 
 *                stack frame on interrupt entry.
 * Note         : Nested interrupts are not allowed.
 */
void return_from_isr(irq_t irq)
{
    /* FIXME: Push all the register into the kernel stack and invoke the 
     *        scheduler. If the scheduler returns, then the execution can
     *        return to the interrupted application. If the scheduler does
     *        not return then it means a task switch has occured.
     */
    irq_path = 1;
    tm_schedule();
    /* FIXME :  Steps to return from interrupt */
}

/* Parameter    : None
 * Return       : current cpu mask value.
 * Description  : This function uses the mask provision in the CPU. Not the
 *                mask function of the interrupt controller.
 *                Calling this function will mask all the interrupts, except
 *                NMI. Can be used to implement critical section.
 *                Re-entrant. Non blocking.
 */
cpuirq_t irq_save(void)
{
    /* FIXME: The implementation of this function might eventually be moved to
     *        ASM.
     */
}

/* Parameter    : previous mask value
 * Return       : None
 * Descfiption  : This function uses the mask function of the CPU. Not the
 *                mask function of the interrupt controller.
 *                It restores the previous mask value. Can be used to implement
 *                critical section.
 *                Re-entrant. Non blocking.
 */
void irq_restore(cpuirq_t orig_value)
{
    /* FIXME: The implementation of this function might eventually be moved to
     *        ASM.
     */
}

/* Parameter    : None
 * Return       : None
 * Description  : Disable all the interrupts
 */
void diable_irq(void)
{
    /* FIXME: The implementation of this function might eventually be moved to
     *        ASM
     */
}

/* Parameter    : None
 * Return       : None
 * Description  : Enable all the interrupts
 */
void enable_irq(void)
{
    /* FIXME: The implementation of this function might eventually be moved to
     *        ASM
     */
}

/* Parameter    : irq number, ranges from 1-7
 *                irq handler
 * Return       : Return code
 * Description  : Subscribe a handler to the desired IRQ. Subscription is 
 *                directly made to the vector table.
 *                The ISR should call return_from_isr in the end and can invoke
 *                bottom half handlers.
 *                Reentrant. nonblocking. criticalsection used.
 */ 
rc_t subscribe_irq(irq_t irq, void (*handler)(void))
{
    uint32 *vector_address;
    rc_t rc = 0;
    cpuirq_t mask;

    if ((irq < 1) || (irq > 7)) {
        rc = ERR_EXCP_INVALID_IRQ;
        goto ret;
    }
    /* FIXME: While unit testing check for little endian and big endian
     *        issues 
     */
    mask = irq_save();

    vector_address = (uint32 *)VECTOR_BASE + (AUTOVECT(irq) << 2); 
    *vector_address = handler;

    irq_restore(mask);

ret:
    return rc;
}

/* Parameter    : irq number, ranges from 1-7
 * Return       : return code
 * Description  : Unsubscribe the existing handler for the given irq number
 *                Reentrant. nonblocking. criticalsection used.
 */
rc_t unsubscribe_irq(irq_t irq)
{
    uint32 *vector_address;
    rc_t rc = 0;
    cpuirq_t mask;

    if ((irq < 1) || (irq > 7)) {
        rc = ERR_EXCP_INVALID_IRQ;
        goto ret;
    }
    /* FIXME: While unit testing check for little endian and big endian
     *        issues 
     */
    mask = irq_save();

    vector_address = (uint32 *)VECTOR_BASE + (AUTOVECT(irq) << 2); 
    *vector_address = 0x00000000;

    irq_restore(mask);
ret:
    return rc;
}

/* Parameter    : bottom half number. Ranges from 0-15
 *                bottom half handler function
 *                parameter to be passed to the handler
 * Return       : return code
 * Description  : Subcribe the handler for bottom half handling. 
 *                Reentrant. nonblocking. criticalsection used.
 */
rc_t subscribe_bhalf(bhalf_t bhalf, void (*handler)(void *param), void *param)
{
    rc_t rc = 0;
    cpuirq_t mask;

    mask = irq_save();
    if ((bhalf < 0) || (bhalf >= MAX_BHALF)) {
        rc = ERR_EXCP_INVALID_BHALF;
        goto ret;
    }
    if (bh_arr[bhalf].handler != NULL) {
        rc = ERR_EXCP_BHALF_CONFLICT;
        goto ret;
    }
    
    bh_arr[bhalf].param = param;
    bh_arr[bhalf].handler = handler;
    bh_arr[bhalf].flag = 0
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : bottom half number. Ranges from 0-15
 * Return       : return code
 * Description  : unsubscibe the handler for the specified bottom half
 *                Reentrant. nonblocking. criticalsection used.
 */
rc_t unsubscribe_bhalf(bhalf_t bhalf)
{
    rc_t rc = 0;
    cpuirq_t mask;

    mask = irq_save();
    if ((bhalf < 0) || (bhalf >= MAX_BHALF)) {
        rc = ERR_EXCP_INVALID_BHALF;
        goto ret;
    }
    bh_arr[bhalf].flag |= BH_UNSUBSCRIBE;
    if (!(bh_arr[bhalf].flag | BH_HANDLING)) { /* BH is actively handled */
        bh_arr[bhalf].param = 0;
        bh_arr[bhalf].handler = 0;
    }
ret:
    irq_restore(mask);
    return rc;
}

/* Parameter    : bottom half number. Ranges from 0-15
 * Return       : return code
 * Description  : Activate the specified bottom half number.
 *                The bottom half handler will be invoked by the highest 
 *                priority task (priority 1).
 *                If the bottom half is activated again before the first
 *                activation is handled, then it will not be remembered.
 *                Reentrant. nonblocking.
 */
rct_t bh_activate(bhalf_t bhalf)
{
    rc_t rc = 0;
    
    if ((bhalf < 0) || (bhalf >= MAX_BHALF) || 
            (bh_arr[bhalf].handler == NULL)) {
        rc = ERR_EXCP_BHALF_ACTIVATE;
        goto ret;
    }
    active_bh |= (1 << bhalf);
    sem_give(SEM_SYNC_BHALF);
ret:
    return rc;
}

/* FIXME : subcribing/unsubscribing to TRAPs and user interrupt vector can
 *         also be added when there is a need for it
 */
