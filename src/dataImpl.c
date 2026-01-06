/*
 * Copyright 2011 Mect s.r.l
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

/*
 * Filename: dataImpl.c
 */

#include "dataImpl.h"

/* ----  Local Functions:	--------------------------------------------------- */

/* ----  Global Variables:	 -------------------------------------------------- */

int verbose_print_enabled = 0;
int timer_overflow_enabled = 0;

struct system_ini system_ini;

static pthread_t theEngineThread_id = XX_WRONG_THREAD;
static enum threadStatus theEngineThreadStatus = NOT_STARTED;

/* ----  Implementations:	--------------------------------------------------- */

uint64_t rt_timer_read()
{
    uint64_t retval;
    struct timespec t;

    if (clock_gettime(CLOCK_MONOTONIC, &t) == 0) {
        retval = t.tv_sec * UN_MILIARDO_ULL + t.tv_nsec;
    } else {
        retval = 0ull;
    }
    return retval;
}

void dataEnableVerbosePrint(void)
{
   verbose_print_enabled = 1;
}

void dataEnableTimerOverflow(void)
{
   timer_overflow_enabled = 1;
   xx_clock_gettime_overflow_enable();
}

/* ---------------------------------------------------------------------------- */

void dataEngineStart(void)
{
    // start the engine thread
    engineInit();
    if (xx_pthread_create(&theEngineThread_id, NULL, &engineThread, &theEngineThreadStatus, "engine", 0) == 0) {
        do {
            xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
        } while (theEngineThreadStatus != RUNNING);
    }
}

void dataEngineStop(void)
{
    void *retval;

    if (engineStatus == enIdle) {
        // SIGINT arrived before initialization
        return;
    }
    setEngineStatus(enExiting);
    if (theEngineThread_id != XX_WRONG_THREAD) {
        pthread_join(theEngineThread_id, &retval);
        theEngineThread_id = XX_WRONG_THREAD;
        fprintf(stderr, "joined engine\n");
    }
    xx_retentives_dump();
}

void dataEnginePwrFailStop(void)
{
    // in case o power failure we have no time for waiting the threads,
    // so we only block the variables writes
    // NB: there is no unlock, it's correct

    pthread_mutex_lock(&theCrosstableClientMutex);

    xx_retentives_dump();
}
