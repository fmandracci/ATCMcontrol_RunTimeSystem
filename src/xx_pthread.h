#ifndef XX_PTHREAD_H
#define XX_PTHREAD_H

#include <pthread.h>

#define XX_WRONG_THREAD 0xffffffff

#define XX_PRIO_HIGHEST 81
#define XX_PRIO_HIGH    80
#define XX_PRIO_MEDIUM  65
#define XX_PRIO_LOW     41
#define XX_PRIO_LOWEST  40

extern int xx_pthread_create(pthread_t *thread, /*const*/ pthread_attr_t *attr,
                      void *(*start_routine)(void *), void *arg,
                             const char *name, size_t stacksize);
extern void xx_pthread_setsched(int policy, int sched_priority);

extern int xx_pthread_mutex_init(pthread_mutex_t *mutex);
extern int xx_pthread_cond_init(pthread_cond_t *cond);

extern void xx_sleep_ms(int delay_ms);
extern void xx_clock_gettime_overflow_enable();
extern int xx_clock_gettime_overflow(clockid_t clk_id, struct timespec *tp);

#endif // XX_PTHREAD_H
