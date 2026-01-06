/*
 * Copyright 2024 Mect s.r.l
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
 * Filename: dataHmiPlc.c
 */

#include "dataImpl.h"

/* ---------------------------------------------------------------------------- */

#define THE_IDLE_TIMEOUT_s	2
#define THE_WAIT_TIMEOUT_ms	 100 // NB: < 1 s

/* ---------------------------------------------------------------------------- */

void *fastIOThread(void *statusAdr)
{
    int threadInitOK = FALSE;
    enum threadStatus *threadStatusPtr = (enum threadStatus *)statusAdr;

    // thread init
    xx_pthread_setsched(SCHED_FIFO, XX_PRIO_MEDIUM); // fastIOThread

    threadInitOK = TRUE;

    // run
    *threadStatusPtr = RUNNING;
    while (engineStatus != enExiting) {

        // trivial scenario
        if ((engineStatus != enRunning && engineStatus != enError) || ! threadInitOK || ! xx_gpio_has_inputs() ) {
            xx_sleep_ms(THE_IDLE_TIMEOUT_s * 1000);
            continue;
        }

        // wait for input events
        int err = xx_gpio_wait_for_events(THE_WAIT_TIMEOUT_ms * 1E6L);

        if (err < 0) {
            xx_sleep_ms(THE_IDLE_TIMEOUT_s * 1000);
            continue;
        } else if (err == 0) {
            // this is actually a polling
            xx_gpio_readall();
        } else {
            // input values were already refreshed in xx_gpio_wait_for_events()
        }

        pthread_mutex_lock(&theCrosstableClientMutex);
        {
            engineFastIO_refresh_no_lock();
        }
        pthread_mutex_unlock(&theCrosstableClientMutex);
    }

    // thread clean

    // exit
    fprintf(stderr, "[%s]: EXITING\n", __func__);
    *threadStatusPtr = EXITING;
    return NULL;
}

/* ---------------------------------------------------------------------------- */
