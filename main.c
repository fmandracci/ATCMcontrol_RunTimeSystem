/*
 * Copyright 2011 Mect s.r.l.
 *
 * This file is part of FarosPLC.
 *
 * FarosPLC is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * FarosPLC is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * FarosPLC. If not, see http://www.gnu.org/licenses/.
*/

#include "plc/plc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <ucontext.h>
#include <getopt.h>

#include "src/dataImpl.h"

// ----------------------------------------------------------------------------

#undef VERBOSE_DEBUG

// ----------------------------------------------------------------------------

static int do_exit = 0;

// ----------------------------------------------------------------------------

void termination_handler(int signum);
void pwrfail_handler(int signum, siginfo_t *siginfo, void *context);

// ----------------------------------------------------------------------------

static char short_options[] = "vxpo";
static struct option long_options[] = {
    {"version",  no_argument,        NULL, 'v'},
    {"xx_gpio",  no_argument,        NULL, 'x'},
    {"print",    no_argument,        NULL, 'p'},
    {"overflow", no_argument,        NULL, 'o'},
    {NULL,       no_argument,        NULL,  0}
};

static int application_options(int argc, char *argv[])
{
    int option_index = 0;
    int c = 0, n;

    if (argc <= 0)
        return 0;

    if (argv == NULL)
        return 1;

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
            case 'v':
                printf("%s version: v%d.%03d GPL\n", argv[0], REVISION_HI, REVISION_LO);
                exit(0);

            case 'x':
                fprintf(stderr, "xx_gpio testing (%s printing):\n", verbose_print_enabled?"with":"without");
                xx_pthread_setsched(SCHED_FIFO, XX_PRIO_HIGH); // main, gpio testing
                xx_gpio_init();
                fprintf(stderr, "    xx_gpio_config(");
                for (n = XX_GPIO_MIN_TEST; n < XX_GPIO_MAX_TEST; ++n) {
                    fprintf(stderr, " (%02d, 1", n);
                    xx_gpio_enable(n);
                    xx_gpio_config(n, 1);
                    fprintf(stderr, ")");
                }
                fprintf(stderr, " )\n");
                while (1) {
                    if (verbose_print_enabled)
                        fprintf(stderr, "\txx_gpio_set(");
                    xx_gpio_tac_set();
                    for (n = XX_GPIO_MIN_TEST; n < XX_GPIO_MAX_TEST; ++n) {
                        if (verbose_print_enabled)
                            fprintf(stderr, " %02d", n);
                        xx_gpio_tic_set();
                        xx_gpio_set(n);
                        xx_gpio_tic_clr();
                    }
                    if (verbose_print_enabled)
                        fprintf(stderr, " ), xx_gpio_clr(");
                    for (n = XX_GPIO_MIN_TEST; n < XX_GPIO_MAX_TEST; ++n) {
                        if (verbose_print_enabled)
                            fprintf(stderr, " %02d", n);
                        xx_gpio_tic_set();
                        xx_gpio_clr(n);
                        xx_gpio_tic_clr();
                    }
                    xx_gpio_tac_clr();
                    if (verbose_print_enabled)
                        fprintf(stderr, " )\n");
                }
                // never reached if Ctrl+C
                xx_gpio_close();
                exit(0);

        case 'p':
            printf("enabling verbose printing\n");
            dataEnableVerbosePrint();
            break;

        case 'o':
            printf("enabling timer overflow simulation\n");
            dataEnableTimerOverflow();
            break;

            default:
                break;
        }
    }
    return 0;
}

// ----------------------------------------------------------------------------

int setup()
{
    struct rlimit rlimit;

    rlimit.rlim_cur = rlimit.rlim_max = 1024 * 1024;
    if (setrlimit(RLIMIT_STACK, &rlimit)) {
        fprintf(stderr, "[%s] error in setrlimit(): %s\n", __func__, strerror(errno));
        return -1;
    }
    bzero(&rlimit, sizeof(rlimit));
    if (getrlimit(RLIMIT_MSGQUEUE, &rlimit)) {
        fprintf(stderr, "[%s]: getrlimit RLIMIT_MSGQUEUE: cur=%lu max=%lu, %s)\n", __func__, rlimit.rlim_cur, rlimit.rlim_max, strerror(errno));
        return -1;
    }
    rlimit.rlim_cur = 8192*8*52;
    rlimit.rlim_max = rlimit.rlim_cur;
    if (setrlimit(RLIMIT_MSGQUEUE, &rlimit)) {
        fprintf(stderr, "[%s]: setrlimit RLIMIT_MSGQUEUE: cur=%lu max=%lu, %s\n", __func__, rlimit.rlim_cur, rlimit.rlim_max, strerror(errno));
        return -1;
    }
    bzero(&rlimit, sizeof(rlimit));
    if (getrlimit(RLIMIT_NOFILE, &rlimit)) {
        fprintf(stderr, "[%s]: getrlimit RLIMIT_NOFILE: cur=%lu max=%lu, %s\n", __func__, rlimit.rlim_cur, rlimit.rlim_max, strerror(errno));
        return -1;
    }
#if 0
    rlimit.rlim_cur = 2048;
    retval = setrlimit(RLIMIT_NOFILE, &rlimit);
    fprintf(stderr, "[%s]: setrlimit RLIMIT_NOFILE: cur=%lu max=%lu, retval=%d\n", __func__, rlimit.rlim_cur, rlimit.rlim_max, retval);
#endif
    mlockall(MCL_CURRENT | MCL_FUTURE);

    /* Enable Core Dumps */
    if (FALSE)
    {
        struct rlimit infinit = {RLIM_INFINITY, RLIM_INFINITY};
        struct rlimit curr;

        if (getrlimit(RLIMIT_CORE, &curr)) {
            fprintf(stdout, "getrlimit failed, %s\n", strerror(errno));
            return -1;
        }
        if (setrlimit(RLIMIT_CORE, &infinit)) {
            fprintf(stdout, "setrlimit failed, %s\n", strerror(errno));
            return -1;
        }
    }

    /* number of  processes  and  threads for  a real user ID */
    struct rlimit limit;

    if (getrlimit(RLIMIT_NPROC, &limit)) {
        fprintf(stdout, "getrlimit failed, %s\n", strerror(errno));
        return -1;
    }
    fprintf(stdout, "getrlimit RLIMIT_NPROC: cur=%lu max=%lu\n", limit.rlim_cur, limit.rlim_max);

    struct sigaction new_action;
    struct sigaction old_action;

    new_action.sa_flags = 0;
    if (sigemptyset(&new_action.sa_mask)) {
        fprintf(stdout, "sigemptyset failed, %s\n", strerror(errno));
        return -1;
    }

    /* Disable SIGHUP (ignore detaching of a possible connected user's terminal) */
    new_action.sa_handler = SIG_IGN;
    if (sigaction(SIGHUP, &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGHUP) failed, %s\n", strerror(errno));
        return -1;
    }

    /* Install Termination Handler */
    new_action.sa_handler = termination_handler;
    if (sigaction(SIGINT, NULL, &old_action)) {
        fprintf(stdout, "sigaction (SIGINT) failed, %s\n", strerror(errno));
        return -1;
    }
    if (old_action.sa_handler != SIG_IGN) {
        if (sigaction(SIGINT, &new_action, NULL)) {
            fprintf(stdout, "sigaction (SIGINT) failed, %s\n", strerror(errno));
            return -1;
        }
    }
    if (sigaction(SIGTERM, NULL, &old_action)) {
        fprintf(stdout, "sigaction (SIGTERM) failed, %s\n", strerror(errno));
        return -1;
    }
    if (old_action.sa_handler != SIG_IGN) {
        if (sigaction(SIGTERM, &new_action, NULL)) {
            fprintf(stdout, "sigaction (SIGTERM) failed, %s\n", strerror(errno));
            return -1;
        }
    }

    /* Install Power Fail Handler */
    new_action.sa_sigaction = pwrfail_handler;
    if (sigaction(SIGPWR, &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGPWR) failed, %s\n", strerror(errno));
        return -1;
    }

    /* Install Crash Handler
     * ------------------------------------------------------------------------
     */
    new_action.sa_sigaction = crash_handler;
    new_action.sa_flags = SA_SIGINFO | SA_NOMASK;
    if (sigemptyset(&new_action.sa_mask)) {
        fprintf(stdout, "sigemptyset failed, %s\n", strerror(errno));
        return -1;
    }
    if (sigaction(SIGILL,  &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGILL) failed, %s\n", strerror(errno));
        return -1;
    }
    if (sigaction(SIGFPE,  &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGFPE) failed, %s\n", strerror(errno));
        return -1;
    }
    if (sigaction(SIGSEGV, &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGSEGV) failed, %s\n", strerror(errno));
        return -1;
    }
    if (sigaction(SIGBUS,  &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGBUS) failed, %s\n", strerror(errno));
        return -1;
    }
    if (sigaction(SIGTRAP, &new_action, NULL)) {
        fprintf(stdout, "sigaction (SIGTRAP) failed, %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

// ----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (application_options(argc, argv) != 0) {
        fprintf(stderr, "[%s]: command line option error.\n", __func__);
        return EXIT_FAILURE;
    }    
    if (setup()) {
        return EXIT_FAILURE;
    }

    // set realtime scheduling
#if ! defined(_POSIX_PRIORITY_SCHEDULING)
    #error Posix scheduling not defined in actual Linux kernel
#endif
    xx_pthread_setsched(SCHED_FIFO, XX_PRIO_HIGHEST);

    // start the core thread
    xx_gpio_init();
    dataEngineStart();

    // start the plc thread
    pthread_t thePlcThread_id = XX_WRONG_THREAD;
    enum threadStatus thePlcThreadStatus = NOT_STARTED;
    if (xx_pthread_create(&thePlcThread_id, NULL, &plcEngineThread, &thePlcThreadStatus, "plcThread", 0) == 0) {
        do {
            xx_sleep_ms(THE_CONFIG_DELAY_ms);
        } while (thePlcThreadStatus != RUNNING);
    }

    // idle loop
    while (! do_exit) {
        xx_sleep_ms(THE_IDLE_DELAY_ms);
    }

    // stop the plc thread
    plcEngineStop();
    if (thePlcThread_id != XX_WRONG_THREAD) {
        void *retval;

        pthread_join(thePlcThread_id, &retval);
        thePlcThread_id = XX_WRONG_THREAD;
        fprintf(stderr, "joined plc\n");
    }

    // stop the core thread
    dataEngineStop();

    return EXIT_SUCCESS;
}

/* ---------------------------------------------------------------------------- */
/**
 * termination_handler
 *
 */
volatile sig_atomic_t term_handler_active	  = 0;
void termination_handler(int signum)
{
    struct sigaction new_action;

#ifdef VERBOSE_DEBUG
    printf("%s!\n", __func__);
#endif
    /* Avoid recursive call of this handler
     */
    if (term_handler_active == 0) {
        term_handler_active = 1;

        do_exit = 1;
    }

    /* Forward the signal
     */
    new_action.sa_flags = 0;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_handler = SIG_DFL;

    sigaction(signum, &new_action, NULL);
    raise(signum);
}

/* ---------------------------------------------------------------------------- */

void pwrfail_handler(int signum, siginfo_t *siginfo, void *context)
{
    (void)signum;
    (void)siginfo;
    (void)context;

    // immediately block the engine and sync the retentive file
    dataEnginePwrFailStop();
}

/* ---------------------------------------------------------------------------- */
