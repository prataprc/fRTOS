#include <data_type.h>
#include <task.h>
#include <sem.h>

#define MAX_SELF_BH 10

/* This task is used to handle the bottom halves. The priority of this this
 * task should be one (highest) and uses synchronisation semaphore to 
 * synchronise with ISRs.
 */
void bh_task(void)
{
    semid_t sem_id;
    rc_t rc;
    cpuirq_t mask;
    bhalf_t bh_count;

    create_semaphore(SEM_SYNC_BHALF, SYNC_CDOWN_VALUE, SEM_SYNCHRONISATION);
    while(1) {

        bh_count = 0;
        rc = sem_take(SEM_SYNC_BHALF);
        tm_non_preemptive(NULL);
        while(active_bh) {
            bhalf_t bh;

            bh = bh_count % MAX_BHALF;
            if (active_bh | (BH_MASK << bh)) {
                active_bh &= ~(BH_MASK << bh);

                mask=irq_save();
                bh_arr[bh].flag |= BH_HANDLING;
                irq_restore(mask);

                if (bh_arr[bh].handler) {
                    (*bh_arr[bh].handler)(bh_arr[bh].param);
                }

                bh_arr[bh].flag &= ~BH_HANDLING;
                if (bh_arr[bh].flag |= BH_UNSUBSCRIBE) {
                    bh_arr[bh].handler = bh_arr[bh].param = NULL;
                    bh_arr[bh].unsubscribe = 0;
                }
            }
            if (bh_count++ > (MAX_BHALF * 10)) {
                /* FIXME : Could be fatal to performance, there could be 
                 *         something wrong.
                 */
            }
            
        }
        tm_preemptive(NULL);
    }
}
