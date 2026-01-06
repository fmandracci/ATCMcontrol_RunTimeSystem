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
 * Filename: dataClock.c
 */

#include "dataImpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>		/* ioctl */

/* ---------------------------------------------------------------------------- */

#define THE_IDLE_TIMEOUT_s	1
#define THE_WAIT_TIMEOUT_s	1

#define DEV_VCIO "/dev/vcio"
#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)

/* ---------------------------------------------------------------------------- */

static void check_and_set(int addr, int value, int *do_signal, int addr_bis, int addr_ter)
{
    if (VAR_VALUE(addr) != (unsigned)value) {
        VAR_VALUE(addr) = value;
        if (CrossTable[addr].usedInAlarmsEvents) {
            *do_signal = TRUE;
        }
        if (addr_bis > 0 && CrossTable[addr_bis].usedInAlarmsEvents) {
            *do_signal = TRUE;
        }
        if (addr_ter > 0 && CrossTable[addr_ter].usedInAlarmsEvents) {
            *do_signal = TRUE;
        }
    }
}

void *clockThread(void *statusAdr)
{
    int threadInitOK = FALSE;
    enum threadStatus *threadStatusPtr = (enum threadStatus *)statusAdr;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int dev_vcio = open(DEV_VCIO, 0);

    if (dev_vcio < 0) {
       fprintf(stderr, "[%s]: error opening %s\n", __func__, DEV_VCIO);
    }

    // thread init
    xx_pthread_setsched(SCHED_FIFO, XX_PRIO_HIGH); // clockThread

    threadInitOK = TRUE;

    // run
    struct timespec abstime;
    int first_time = TRUE;

    *threadStatusPtr = RUNNING;
    while (engineStatus != enExiting) {

        // trivial scenario
        if ((engineStatus != enRunning && engineStatus != enError) || ! threadInitOK) {
            xx_sleep_ms(THE_IDLE_TIMEOUT_s * 1000);
            continue;
        }

        // wait for clock events
        clock_gettime(CLOCK_MONOTONIC, &abstime); // pthread_cond_timedwait + pthread_condattr_setclock
        abstime.tv_sec += THE_WAIT_TIMEOUT_s;
        pthread_mutex_lock(&mutex);
        int err = pthread_cond_timedwait(&theClockCondvar, &mutex, &abstime);
        pthread_mutex_unlock(&mutex);

        if (err < 0) {
            xx_sleep_ms(THE_IDLE_TIMEOUT_s * 1000);
            continue;
        } else if (err == 0) {
            // timeout
        } else {
            // signal
        }

        int cpu_temp;
        int do_update_cpu_temp = FALSE;

        // the first time and then each 5 seconds ...
        if (first_time || (abstime.tv_sec % 5) == 0) {

            // ... and check the cpu temperature
            if (dev_vcio >= 0) {
                #define DATASIZE_BYTES 80
                #define BUFSIZE_BYTES  (DATASIZE_BYTES + 7*4)
                #define BUFSIZE_WORDS  (BUFSIZE_BYTES / 4)
                unsigned buf[BUFSIZE_WORDS];

                buf[0] = BUFSIZE_BYTES;   // size
                buf[1] = 0x00000000;      // process request
                buf[2] = 0x00030080;      // GET_GENCMD_RESULT
                buf[3] = DATASIZE_BYTES;  // request size
                buf[4] = 0;               // reply size = request szie
                buf[5] = 0;               // retval
                strcpy((char *)&buf[6], "measure_temp");
                buf[BUFSIZE_WORDS - 1] = 0x00000000;

                if (ioctl(dev_vcio, IOCTL_MBOX_PROPERTY, buf) >= 0) {
                    char *reply = (char *)&buf[6];
                    int retval = buf[5];

                    if (retval == 0) {
                        int C = 0, dC = 0;

                        if (sscanf(reply, "temp=%d.%d'C", &C, &dC) == 2) {
                            cpu_temp = C * 10 + dC;
                            do_update_cpu_temp = TRUE;
                            // fprintf(stderr, "[%s]: cpu temperature is %d dC\n", __func__, cpu_temp);
                        } else {
                            fprintf(stderr, "[%s]: ioctl returns '%s'\n", __func__, reply);
                        }
                    } else {
                        fprintf(stderr, "[%s]: ioctl returns %d\n", __func__, retval);
                    }
                } else {
                    fprintf(stderr, "[%s]: ioctl failed\n", __func__);
                }
            }

        }

        // datetime   NB no writeQdataRegisters();
        struct timespec tv;
        struct tm datetime;
        int do_update_datetime = FALSE;

        xx_clock_gettime_overflow(CLOCK_REALTIME, &tv);
        if (localtime_r(&tv.tv_sec, &datetime)) {
            do_update_datetime = TRUE;
        }

        if (do_update_datetime || do_update_cpu_temp) {
            pthread_mutex_lock(&theCrosstableClientMutex);
            {
                int do_signal_alarm = FALSE;

                if (do_update_datetime) {
                    check_and_set(PLC_Seconds, datetime.tm_sec        , &do_signal_alarm, PLC_UPTIME_cs, PLC_UPTIME_s);
                    check_and_set(PLC_Seconds, datetime.tm_sec        , &do_signal_alarm, 0, 0);
                    check_and_set(PLC_Minutes, datetime.tm_min        , &do_signal_alarm, 0, 0);
                    check_and_set(PLC_Hours  , datetime.tm_hour       , &do_signal_alarm, 0, 0);
                    check_and_set(PLC_Day    , datetime.tm_mday       , &do_signal_alarm, 0, 0);
                    check_and_set(PLC_Month  , datetime.tm_mon + 1    , &do_signal_alarm, 0, 0);
                    check_and_set(PLC_Year   , 1900 + datetime.tm_year, &do_signal_alarm, 0, 0);
                }

                if (do_update_cpu_temp) {
                    check_and_set(PLC_CPU_TEMP , cpu_temp    , &do_signal_alarm, 0, 0);
                }

                if (do_signal_alarm) {
                    pthread_cond_signal(&theAlarmsEventsCondvar);
                }
            }
            pthread_mutex_unlock(&theCrosstableClientMutex);
        }
        first_time = FALSE;
    }

    // thread clean
    if (dev_vcio > 0)
        close(dev_vcio);

    // exit
    fprintf(stderr, "[%s]: EXITING\n", __func__);
    *threadStatusPtr = EXITING;
    return NULL;
}

/* ---------------------------------------------------------------------------- */
