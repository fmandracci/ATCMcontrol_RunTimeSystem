#include "xx_pthread.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>

#include <stdio.h>
#include <errno.h>
#include <limits.h> // PTHREAD_STACK_MIN

// ---------------------------------------------------------------------------

static unsigned threads_num = 0;

int xx_pthread_create(pthread_t *thread, /*const*/ pthread_attr_t *attr,
                    void *(*start_routine)(void *), void *arg,
                    const char *name, size_t stacksize)
{
    int retval;
    pthread_attr_t *attr_p = attr;
    pthread_attr_t attr_x;
    size_t pthread_stack_min = PTHREAD_STACK_MIN;

    threads_num += 1;

    if (attr_p == NULL) {
        attr_p = &attr_x;

        pthread_attr_init(attr_p);
    }
    if (stacksize < pthread_stack_min) {
        stacksize = pthread_stack_min;
    }
    if (stacksize < 131072) {
        stacksize = 131072;
    }

    pthread_attr_setdetachstate(attr_p, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(attr_p, stacksize);
    pthread_attr_setinheritsched(attr_p, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setscope(attr_p, PTHREAD_SCOPE_PROCESS);

    retval = pthread_create(thread, attr_p, start_routine, arg);
    if (retval) {
        fprintf(stderr, "[%s]: ERROR creating %s thread: ", __func__, name);
        switch (retval) {
        case EAGAIN: fputs("EAGAIN", stderr); break;
        case EINVAL: fputs("EINVAL", stderr); break;
        case EPERM: fputs("EPERM", stderr); break;
        default: fprintf(stderr, "%d", retval);
        }
        fprintf(stderr, ", %s.\n", strerror(errno));
        *thread = XX_WRONG_THREAD;
    } else {
#ifdef VERBOSE_DEBUG
        fprintf(stderr, "[%s]: success creating %s thread\n", __func__, name);
#endif
    }
    return retval;
}

// ---------------------------------------------------------------------------

void xx_pthread_setsched(int policy, int sched_priority)
{
    struct sched_param sp;

    if (policy != SCHED_OTHER) {
        sp.sched_priority = sched_priority;
    }
    if (pthread_setschedparam(pthread_self(), policy, &sp)) {
        fprintf(stderr, "[%s]: pthread_setschedparam() failed, %s", __func__, strerror(errno));
    }
}

// ---------------------------------------------------------------------------

int xx_pthread_mutex_init(pthread_mutex_t *mutex)
{
    pthread_mutexattr_t mattr;

    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
    return pthread_mutex_init(mutex, &mattr);
}

// ---------------------------------------------------------------------------

int xx_pthread_cond_init(pthread_cond_t *cond)
{
    pthread_condattr_t attr;

    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    return pthread_cond_init(cond, &attr);
}

// ---------------------------------------------------------------------------

void xx_sleep_ms(int delay_ms)
{
    struct timespec rqtp, rmtp;
    ldiv_t q;

    q = ldiv(delay_ms, 1000); // ms -> s
    rqtp.tv_sec = q.quot;
    rqtp.tv_nsec = q.rem * 1E6; // ms -> ns

    while (clock_nanosleep(CLOCK_MONOTONIC, 0, &rqtp, &rmtp) == EINTR) {
        rqtp.tv_sec = rmtp.tv_sec;
        rqtp.tv_nsec = rmtp.tv_nsec;
    }
}

// ---------------------------------------------------------------------------

// 4294967ul // ~ 0xFFFFffff ms = 49 giorni 17 ore 02 minuti 47 secondi
// 4294920ul //                   49 giorni 17 ore 02 minuti 00 secondi
// 2147483ul // ~ 0x7FFFffff ms = 24 giorni 20 ore 31 minuti 23 secondi
// 2147460ul //                   24 giorni 20 ore 31 minuti 00 secondi
#define XX_CLOCK_OFFSET_s 4294920ul

static unsigned xx_clock_offset_s = 0ul;

void xx_clock_gettime_overflow_enable()
{
    xx_clock_offset_s = XX_CLOCK_OFFSET_s;
}

int xx_clock_gettime_overflow(clockid_t clk_id, struct timespec *tp)
{
    int retval;

    retval = clock_gettime(clk_id, tp);
    if (tp && (retval == 0)) {
        tp->tv_sec += xx_clock_offset_s;
    }
    return retval;
}

// ---------------------------------------------------------------------------
