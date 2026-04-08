#ifndef MON_HOOKS_H
#define MON_HOOKS_H

/* mon_hooks.h — sync-step function monitor hooks
 * Include in any C interpreter to get ENTER/EXIT events on every function.
 * Each participant has two FIFOs: evt (writes) and ack (blocks reading).
 * Controller reads one event from each participant, compares, sends G or S.
 * G = continue, S = stop (divergence or timeout).
 */

void mon_open(const char *evt_fifo, const char *ack_fifo);
void mon_enter(const char *fn);
void mon_exit(const char *fn, const char *result);
void mon_close(void);

#define MON_ENTER(name)         mon_enter(name)
#define MON_EXIT(name, res)     mon_exit(name, res)
#define MON_EXIT_INT(name, v)   do { char _b[32]; \
    snprintf(_b,sizeof(_b),"%d",(int)(v)); mon_exit(name,_b); } while(0)

/* No-op when MON_DISABLED is defined (production builds) */
#ifdef MON_DISABLED
#  define MON_ENTER(name)        ((void)0)
#  define MON_EXIT(name,res)     ((void)0)
#  define MON_EXIT_INT(name,v)   ((void)0)
#endif

#endif /* MON_HOOKS_H */
