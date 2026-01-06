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
 * Filename: dataEngine.c
 */

#include "dataImpl.h"

#include "../plc/plc.h"

#undef VERBOSE_DEBUG

/* ---------------------------------------------------------------------------- */

enum EngineStatus engineStatus = enIdle;
pthread_mutex_t theCrosstableClientMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t theAlarmsEventsCondvar;
pthread_cond_t theClockCondvar;
sem_t newOperations[MAX_DEVICES];

int do_flush_retentives = FALSE;

/* ---------------------------------------------------------------------------- */

static pthread_t theClockThread_id = XX_WRONG_THREAD;
static enum threadStatus theClockThreadStatus;

/* ---------------------------------------------------------------------------- */

struct ServerStruct theServers[MAX_SERVERS];
uint16_t theServersNumber = 0;

struct ClientStruct theDevices[MAX_DEVICES];
uint16_t theDevicesNumber = 0;
uint16_t theTcpDevicesNumber = 0;

/* ---------------------------------------------------------------------------- */

static pthread_t theDataSyncThread_id = XX_WRONG_THREAD;
static enum threadStatus theDataSyncThreadStatus = NOT_STARTED;

/* ---------------------------------------------------------------------------- */

static pthread_t theUsrFastIOThread_id = XX_WRONG_THREAD;
static enum threadStatus theUsrFastIOThreadStatus;

/* ---------------------------------------------------------------------------- */

static int system_ini_ok;

static const char *fieldbusName[] = {"PLC", "RTU", "TCP", "TCPRTU", "CANOPEN", "MECT", "RTU_SRV", "TCP_SRV", "TCPRTU_SRV" };

static int checkServersDevicesAndNodes();
static void initNodeDiagnostic(uint16_t n);

static inline void setAlarmEvent(int i);
static inline void clearAlarmEvent(int i);
static inline void checkAlarmEvent(int i, int condition);
static void AlarmMngr(void);

static unsigned plc_product_id(unsigned *msVersion);
static unsigned plc_serial_number();

/* ---------------------------------------------------------------------------- */

void engineInit()
{
    // initialize
    theClockThread_id = XX_WRONG_THREAD;
    theUsrFastIOThread_id = XX_WRONG_THREAD;
    theDataSyncThread_id = XX_WRONG_THREAD;
    theClockThreadStatus = NOT_STARTED;
    theUsrFastIOThreadStatus = NOT_STARTED;
    theDataSyncThreadStatus = NOT_STARTED;
    int s, d, n;

    // read the configuration file
    if (app_config_load(&system_ini)) {
        fprintf(stderr, "[%s]: Error loading config file.\n", __func__);
        system_ini_ok = FALSE;
    } else {
        system_ini_ok = TRUE;
    }
    // non null default values
    for (n = 0; n < MAX_SERIAL_PORT; ++n) {
        if (system_ini.serial_port[n].max_block_size <= 0 || system_ini.serial_port[n].max_block_size > MAX_VALUES)
            system_ini.serial_port[n].max_block_size = MAX_VALUES;
    }
    if (system_ini.tcp_ip_port.max_block_size <= 0 || system_ini.tcp_ip_port.max_block_size > MAX_VALUES)
        system_ini.tcp_ip_port.max_block_size = MAX_VALUES;
    for (n = 0; n < MAX_CANOPEN; ++n) {
        if (system_ini.canopen[n].max_block_size <= 0 || system_ini.canopen[n].max_block_size> MAX_VALUES)
            system_ini.canopen[n].max_block_size = MAX_VALUES;
    }

    if (verbose_print_enabled) {
        app_config_dump(&system_ini);
    }

    // cleanup variables
    resetHmiPlcBlocks(&hmiBlock, &plcBlock);

    // default values
    VAR_VALUE(PLC_Version) = REVISION_HI * 1000 + REVISION_LO;
    VAR_VALUE(PLC_BEEP_VOLUME) = 0x00000064; // duty=100%
    VAR_VALUE(PLC_TOUCH_VOLUME) = 0x00000064; // duty=100%
    VAR_VALUE(PLC_ALARM_VOLUME) = 0x00000064; // duty=100%

    // P/N and S/N and MS VERSION
    unsigned msVer = 0;
    VAR_VALUE(PLC_PRODUCT_ID) = plc_product_id(&msVer);
    VAR_VALUE(PLC_SERIAL_NUMBER) = plc_serial_number();
    VAR_VALUE(PLC_MS_VERSION) = msVer;

    // retentive variables
    xx_retentives_init(LAST_RETENTIVE * 4);
    if (xx_retentives_ptr == NULL) {
        fprintf(stderr, "Missing or wrong retentive file.\n");
    } else {
        memcpy(&VAR_VALUE(1), xx_retentives_ptr, LAST_RETENTIVE * 4);
    }

    // initialize data array
    xx_pthread_mutex_init(&theCrosstableClientMutex);
    xx_pthread_cond_init(&theAlarmsEventsCondvar);
    xx_pthread_cond_init(&theClockCondvar);
    for (s = 0; s < MAX_SERVERS; ++s) {
        theServers[s].thread_id = XX_WRONG_THREAD;
    }
    for (d = 0; d < MAX_DEVICES; ++d) {
        theDevices[d].thread_id = XX_WRONG_THREAD;
    }
}

void *engineThread(void *statusAdr)
{
    enum threadStatus *threadStatusPtr = (enum threadStatus *)statusAdr;
    int allOK = FALSE;

    // thread init
    plc_events_setup();
    xx_pthread_setsched(SCHED_FIFO, XX_PRIO_HIGH); // engineThread
    setEngineStatus(enInitialized);

    XX_GPIO_TIC_SET();
    pthread_mutex_lock(&theCrosstableClientMutex);
    {
        int s, d, n;

        // load configuration
        if (LoadXTable()) {
            goto exit_initialization;
        }
        if (!system_ini_ok) {
            goto exit_initialization;
        }
        if (checkServersDevicesAndNodes()) {
            goto exit_initialization;
        }

        if (verbose_print_enabled) {
            fprintf(stderr, "engineThread(): %u servers, %u devices, %u nodes\n", theServersNumber, theDevicesNumber, theNodesNumber);
            for (s = 0; s < theServersNumber; ++s) {
                fprintf(stderr, "\t0x%02x: %s\n", s, theServers[s].name);
            }
            for (d = 0; d < theDevicesNumber; ++d) {
                fprintf(stderr, "\t0x%02x: %s\n", d, theDevices[d].name);
                for (n = 0; n < theNodesNumber; ++n) {
                    if (theNodes[n].device == d) {
                        fprintf(stderr, "\t\tnode#%2d NodeID=%u\n", n+1, theNodes[n].NodeID);
                    }
                }
            }
        }

#if defined(KIT_IMX28)
        // i.MX28 workaround
        xx_pwm3_set(0);
        xx_pwm3_enable();

#elif defined(KIT_RPI4)
        xx_pwm3_enable();
        xx_pwm3_disable();

#else
#error unknown platform
#endif

        // create clock
        if (xx_pthread_create(&theClockThread_id, NULL, &clockThread, &theClockThreadStatus, "Clock", 0) == 0) {
            do {
                xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
            } while (theClockThreadStatus != RUNNING);
            if (1 || verbose_print_enabled)
                fprintf(stderr, "[%s]: the clock thread is up and running.\n", __func__);
        } else {
            fprintf(stderr, "[%s]: ERROR creating the clock thread: %s.\n", __func__, strerror(errno));
        }
        // create servers
        for (s = 0; s < theServersNumber; ++s) {
            void *arg = (void *)s;

            theServers[s].thread_status = NOT_STARTED;
            if (xx_pthread_create(&theServers[s].thread_id, NULL, &serverThread, arg, theServers[s].name, 0) == 0) {
                do {
                    xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
                } while (theServers[s].thread_status != RUNNING);
                if (verbose_print_enabled)
                    fprintf(stderr, "[%s]: server thread %s is up and running.\n", __func__, theServers[s].name);
            } else {
                fprintf(stderr, "[%s]: ERROR creating server thread %s: %s.\n", __func__, theServers[s].name, strerror(errno));
            }
        }
        // create clients
        for (d = 0; d < theDevicesNumber; ++d) {
            void *arg = (void *)d;

            theDevices[d].thread_status = NOT_STARTED;
            if (xx_pthread_create(&theDevices[d].thread_id, NULL, &clientThread, arg, theDevices[d].name, 0) == 0) {
                do {
                    xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
                } while (theDevices[d].thread_status != RUNNING);
                if (verbose_print_enabled)
                    fprintf(stderr, "[%s]: device thread %s: is up and running.\n", __func__, theDevices[d].name);
            } else {
                fprintf(stderr, "[%s]: ERROR creating device thread %s: %s.\n", __func__, theDevices[d].name, strerror(errno));
            }
        }
        // create udp server
        if (xx_pthread_create(&theDataSyncThread_id, NULL, &datasyncThread, &theDataSyncThreadStatus, "datasync", 0) == 0) {
            do {
                xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
            } while (theDataSyncThreadStatus != RUNNING);
            if (1 || verbose_print_enabled)
                fprintf(stderr, "[%s]: the udp server thread is up and running.\n", __func__);
        } else {
            fprintf(stderr, "[%s]: ERROR creating the udp server thread: %s.\n", __func__, strerror(errno));
        }
        // create the usr fast i/o manager
#ifdef XX_GPIO_INTERRUPT
        if (xx_pthread_create(&theUsrFastIOThread_id, NULL, &fastIOThread, &theUsrFastIOThreadStatus, "usrFastIO", 0) == 0) {
            do {
                xx_sleep_ms(THE_CONFIG_DELAY_ms); // not sched_yield();
            } while (theUsrFastIOThreadStatus != RUNNING);
            if (1 || verbose_print_enabled)
                fprintf(stderr, "[%s]: the usr fastio thread is up and running.\n", __func__);
        } else {
            fprintf(stderr, "[%s]: ERROR creating the usr fastio thread: %s.\n", __func__, strerror(errno));
        }
#else
        if (verbose_print_enabled)
            fprintf(stderr, "[%s]: using usr fastio polling.\n", __func__);
#endif
        // ok, all done
        allOK = TRUE;

    exit_initialization:
        buzzer_periods = 0;
        buzzer_tic = 0;
        xx_pwm3_disable();
    }
    pthread_mutex_unlock(&theCrosstableClientMutex);
    XX_GPIO_TIC_CLR();

    if (allOK) {
#ifdef VERBOSE_DEBUG
        fprintf(stderr, "[%s]: PLC engine is running\n", __func__);
#endif
        setEngineStatus(enRunning);
    } else {
        fprintf(stderr, "**********************************************************\n");
        fprintf(stderr, "* PLC engine is in error status: application won't work! *\n");
        fprintf(stderr, "**********************************************************\n");
        setEngineStatus(enError);
    }
    // run
    struct timespec abstime;
    clock_gettime(CLOCK_MONOTONIC, &abstime); // pthread_cond_timedwait + pthread_condattr_setclock

    // NO default Fast I/O config PLC_FastIO_Dir

    int tic = 0;
    RTIME tic_ns;
    struct timespec tic_ts;

    if (timer_overflow_enabled) {
        xx_clock_gettime_overflow(CLOCK_MONOTONIC, &tic_ts);
        tic_ns = tic_ts.tv_sec * UN_MILIARDO_ULL + tic_ts.tv_nsec;
    } else {
        tic_ns = rt_timer_read();
    }
    VAR_VALUE(PLC_UPTIME_cs) = tic_ns / DIECI_MILIONI_UL;
    VAR_VALUE(PLC_UPTIME_s) = tic_ns / UN_MILIARDO_ULL;

    pthread_mutex_lock(&theCrosstableClientMutex);
    *threadStatusPtr = RUNNING;
    XX_GPIO_TAC_CLR();
    while (engineStatus != enExiting) {

        XX_GPIO_TIC_SET();

        // trivial scenario
        if (engineStatus != enRunning) {
            XX_GPIO_TIC_CLR();
            pthread_mutex_unlock(&theCrosstableClientMutex);
            xx_sleep_ms(THE_ENGINE_DELAY_ms);
            pthread_mutex_lock(&theCrosstableClientMutex);
            XX_GPIO_TIC_SET();
            continue;
        }

        // ldiv_t x = ldiv(THE_ENGINE_DELAY_ms, 1000);
        // abstime.tv_sec += x.quot;
        // abstime.tv_sec += x.rem * 1E6;
        abstime.tv_nsec = abstime.tv_nsec + (THE_ENGINE_DELAY_ms * 1E6);
        if (abstime.tv_nsec >= 1E9) {
            abstime.tv_sec += 1;
            abstime.tv_nsec -= 1E9;
        }
        while (TRUE) {
            int e;
            XX_GPIO_TIC_CLR();
            e = pthread_cond_timedwait(&theAlarmsEventsCondvar, &theCrosstableClientMutex, &abstime);
            XX_GPIO_TIC_SET();

            if (timer_overflow_enabled) {
                xx_clock_gettime_overflow(CLOCK_MONOTONIC, &tic_ts);
                tic_ns = tic_ts.tv_sec * UN_MILIARDO_ULL + tic_ts.tv_nsec;
            } else {
                tic_ns = rt_timer_read();
            }
            VAR_VALUE(PLC_UPTIME_cs) = tic_ns / DIECI_MILIONI_UL;
            VAR_VALUE(PLC_UPTIME_s) = tic_ns / UN_MILIARDO_ULL;

            // NB: no test CrossTable[PLC_UPTIME_*].usedInAlarmsEvents, vedi clockThread
            if (e == ETIMEDOUT) {
                break;
            }
            AlarmMngr();
        }

        tic = (tic + 1) % 10;
        if (tic == 1) {
            XX_GPIO_TAC_SET();
            // NB: no pthread_cond_signal(&theClockCondvar);
        } else {
            XX_GPIO_TAC_CLR();
        }

        if (tic == 1 || tic == 6) {
            // TICtimer  NB no writeQdataRegisters();

            float plc_time, plc_timeMin, plc_timeMax, plc_timeWin;
            RTIME tic_ms = tic_ns / UN_MILIONE_UL;

            tic_ms = tic_ms % (86400 * 1000); // 1 day overflow
            plc_time = tic_ms / 1000.0;
            memcpy(&plc_timeWin, &VAR_VALUE(PLC_timeWin), sizeof(uint32_t));

            if (plc_timeWin < 5.0) {
                plc_timeWin = 5.0;
            }
            if (plc_time <= plc_timeWin) {
                plc_timeMin = 0;
                plc_timeMax = plc_timeWin;
            } else {
                plc_timeMin = plc_time - plc_timeWin;
                plc_timeMax = plc_time;
            }
            memcpy(&VAR_VALUE(PLC_time), &plc_time, sizeof(uint32_t));
            memcpy(&VAR_VALUE(PLC_timeMin), &plc_timeMin, sizeof(uint32_t));
            memcpy(&VAR_VALUE(PLC_timeMax), &plc_timeMax, sizeof(uint32_t));
            memcpy(&VAR_VALUE(PLC_timeWin), &plc_timeWin, sizeof(uint32_t));
        }

        if (VAR_VALUE(PLC_ResetValues)) {
            uint16_t addr;

            for (addr = 5000; addr < 5160; addr += 10) {
                VAR_VALUE(addr + 3) = 0; // READS
                VAR_VALUE(addr + 4) = 0; // WRITES
                VAR_VALUE(addr + 5) = 0; // TIMEOUTS
                VAR_VALUE(addr + 6) = 0; // COMM_ERRORS
                VAR_VALUE(addr + 7) = 0; // LAST_ERROR
            }
            VAR_VALUE(PLC_ResetValues) = 0;
        }

        // PLC_FastIO, both In and Out
#ifdef XX_GPIO_INTERRUPT
        // vedi fastIOThread
#else
        xx_gpio_readall();
        engineFastIO_refresh_no_lock();
#endif
        unsigned addr = PLC_WATCHDOG_ms;
        unsigned value = xx_watchdog_get();
        if (value != VAR_VALUE(addr)) {
            VAR_VALUE(addr) = value; // NB no writeQdataRegisters();
        }

        // BUZZER
        if (buzzer_periods > 0) {
            ++buzzer_tic;

            if (buzzer_tic > buzzer_period_tics) {
                ++buzzer_periods;

                if (buzzer_periods > buzzer_replies) {
                    xx_pwm3_disable();
                    buzzer_tic = 0;
                    buzzer_periods = 0;
                } else {
                    xx_pwm3_enable();
                    buzzer_tic = 1;
                }
            } else if (buzzer_tic == buzzer_on_cs) {
                xx_pwm3_disable();
            }
        }

        if (do_flush_retentives) {
            do_flush_retentives = FALSE;
            // the mutex was locked by pthread_cond_timedwait()
            pthread_mutex_unlock(&theCrosstableClientMutex);
            {
                // syncing the retentive file but without holding the mutex
                xx_retentives_sync();
            }
            pthread_mutex_lock(&theCrosstableClientMutex);
        }

    }
    pthread_mutex_unlock(&theCrosstableClientMutex);

    // thread clean
    {
        void *retval;
        unsigned n;

        if (theClockThread_id != XX_WRONG_THREAD) {
            pthread_join(theClockThread_id, &retval);
            theClockThread_id = XX_WRONG_THREAD;
            fprintf(stderr, "joined clock");
        }
        for (n = 0; n < theDevicesNumber; ++n) {
            if (theDevices[n].thread_id != XX_WRONG_THREAD) {
                pthread_join(theDevices[n].thread_id, &retval);
                theDevices[n].thread_id = XX_WRONG_THREAD;
                fprintf(stderr, "joined dev(%d)\n", n);
            }
        }
        for (n = 0; n < theServersNumber; ++n) {
            if (theServers[n].thread_id != XX_WRONG_THREAD) {
                pthread_join(theServers[n].thread_id, &retval);
                theServers[n].thread_id = XX_WRONG_THREAD;
            fprintf(stderr, "joined srv(%d)\n", n);
            }
        }
        if (theDataSyncThread_id != XX_WRONG_THREAD) {
            pthread_join(theDataSyncThread_id, &retval);
            theDataSyncThread_id = XX_WRONG_THREAD;
            fprintf(stderr, "joined datasync\n");
        }
        if (theUsrFastIOThread_id != XX_WRONG_THREAD) {
            pthread_join(theUsrFastIOThread_id, &retval);
            theUsrFastIOThread_id = XX_WRONG_THREAD;
            fprintf(stderr, "joined usr fastio");
        }
    }

    // exit
    XX_GPIO_TIC_CLR();
    fprintf(stderr, "[%s]: EXITING\n", __func__);
    *threadStatusPtr = EXITING;
    return NULL;
}

/* ---------------------------------------------------------------------------- */

void engineFastIO_refresh_no_lock()
{
    unsigned addr;
    unsigned value;

    for (addr = PLC_FastIO_1; addr < (PLC_FastIO_1 + XX_GPIO_MAX); ++addr) {
        register unsigned n = addr - PLC_FastIO_1;

        if (xx_gpio_is_enabled(n)) {
            value = xx_gpio_get(n);

            if (value != VAR_VALUE(addr)) {
                writeQdataRegisters(addr, value, DATA_OK);
            }
        }
    }
}

/* ---------------------------------------------------------------------------- */

static int checkServersDevicesAndNodes()
{
    int retval = 0;
    int disable_all_nodes = FALSE;
    FILE *hmi_ini;

    // init tables
    theDevicesNumber = 0;
    theNodesNumber = 0;
    bzero(&theDevices[0], sizeof(theDevices));
    bzero(&theNodes[0], sizeof(theNodes));

    // check hmi.ini: search for "disable_all_nodes=true"
    hmi_ini = fopen(HMI_INI, "r");
    if (hmi_ini) {
        char *disable_all_nodes_true = "disable_all_nodes=true";
        char row[1024];

        while (fgets(row, 1024, hmi_ini)) {
            char *p = &row[0];
            unsigned i;

            // trim
            for (i = 0; i < strlen(row); ++i) {
                if (isblank(row[i])) {
                    continue;
                } else {
                    p = &row[i];
                    break;
                }
            }
            // check
            if (strncmp(p, disable_all_nodes_true, strlen(disable_all_nodes_true)) == 0) {
                disable_all_nodes = TRUE;
                fprintf(stderr, "[%s]: disabling all nodes as requested\n", __func__);
                break;
            }
        }
        fclose(hmi_ini);
    }

    // for each enabled variable
    uint16_t i, base, block;
    fprintf(stderr, "[%s]: \n", __func__);
    for (i = 1, base = 1, block = 0; i <= DimCrossTable; ++i) {

        // find block base addresses
        if (CrossTable[i].Block != block) {
            base = i;
            block = CrossTable[i].Block;
        }
        CrossTable[i].BlockBase = base;

        if (CrossTable[i].Enable > 0) {
            uint16_t s = MAX_SERVERS;
            uint16_t d;
            uint16_t n;
            uint16_t p;

            // server variables =---> enable the server thread
            switch (CrossTable[i].Protocol) {
            case PLC:
                // no plc server
                break;
            case RTU:
            case TCP:
            case TCPRTU:
            case CANOPEN:
            case MECT:
                // nothing to do for server
                break;
            case RTU_SRV:
                // add unique variable's server
                for (s = 0; s < theServersNumber; ++s) {
                    if (RTU_SRV == theServers[s].protocol && CrossTable[i].Port == theServers[s].port) {
                        // already present
                        if (theServers[s].IPaddress != CrossTable[i].IPAddress) {
                            char str1[42];
                            char str2[42];
                            fprintf(stderr,
                                "[%s]: WARNING in variable #%u wrong 'IP Address' %s (should be %s)\n",
                                __func__, i, ipaddr2str(CrossTable[i].IPAddress, str2), ipaddr2str(theServers[s].IPaddress, str1));
                        }
                        if (theServers[s].NodeId != CrossTable[i].NodeId) {
                            fprintf(stderr,
                                "[%s]: WARNING in variable #%u wrong 'Node ID' %u (should be %u)\n",
                                __func__, i, CrossTable[i].NodeId, theServers[s].NodeId);
                        }
                        break;
                    }
                }
                goto add_server;
                // no break;
            case TCP_SRV:
            case TCPRTU_SRV:
                // add unique variable's server
                for (s = 0; s < theServersNumber; ++s) {
                    if (CrossTable[i].Protocol == theServers[s].protocol) {
                        // already present
                        if (theServers[s].IPaddress != CrossTable[i].IPAddress) {
                            char str1[42];
                            char str2[42];
                            fprintf(stderr,
                                "[%s]: WARNING in variable #%u wrong 'IP Address' %s (should be %s)\n",
                                __func__, i, ipaddr2str(CrossTable[i].IPAddress, str2), ipaddr2str(theServers[s].IPaddress, str1));
                        }
                        if (theServers[s].port != CrossTable[i].Port) {
                            fprintf(stderr,
                                "[%s]: WARNING in variable #%u wrong 'Port' %d (should be %u)\n",
                                __func__, i, CrossTable[i].Port, theServers[s].port);
                        }
                        if (theServers[s].NodeId != CrossTable[i].NodeId) {
                            fprintf(stderr,
                                "[%s]: WARNING in variable #%u wrong 'Node ID' %u (should be %u)\n",
                                __func__, i, CrossTable[i].NodeId, theServers[s].NodeId);
                        }
                        break;
                    }
                }
            add_server:
                if (s < theServersNumber) {
                    // ok already present
                } else if (theServersNumber >= MAX_SERVERS) {
                    fprintf(stderr, "[%s]: too many servers (max=%d)\n", __func__, MAX_SERVERS);
                    retval = -1;
                } else {
                    // new server entry
                    ++theServersNumber;
                    theServers[s].protocol = CrossTable[i].Protocol;
                    theServers[s].IPaddress = CrossTable[i].IPAddress;
                    theServers[s].port = CrossTable[i].Port;
                    switch (theServers[s].protocol) {
                    case PLC:
                    case RTU:
                    case TCP:
                    case TCPRTU:
                    case CANOPEN:
                    case MECT:
                        // FIXME: assert
                        break;
                    case RTU_SRV: {
                        uint16_t port = CrossTable[i].Port;
                        switch (port) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                            if (system_ini.serial_port[port].baudrate == 0) {
                                fprintf(stderr, "[%s]: missing port %u in system.ini for RTU_SRV variable #%u\n", __func__, port, i);
                                retval = -1;
                            } else {
                                theServers[s].u.serial.port = port;
                                theServers[s].u.serial.baudrate = system_ini.serial_port[port].baudrate;
                                theServers[s].u.serial.parity = system_ini.serial_port[port].parity;
                                theServers[s].u.serial.databits = system_ini.serial_port[port].databits;
                                theServers[s].u.serial.stopbits = system_ini.serial_port[port].stopbits;
                                theServers[s].silence_ms = system_ini.serial_port[port].silence_ms;
                                theServers[s].timeout_ms = system_ini.serial_port[port].timeout_ms;
                            }
                            break;
                        default:
                            fprintf(stderr, "[%s]: bad RTU_SRV port %u for variable #%u\n", __func__, port, i);
                            retval = -1;
                        }
                        theServers[s].ctx = NULL;
                    }   break;
                    case TCP_SRV:
                        theServers[s].u.tcp_ip.IPaddr = CrossTable[i].IPAddress;
                        theServers[s].u.tcp_ip.port = CrossTable[i].Port;
                        theServers[s].silence_ms = system_ini.tcp_ip_port.silence_ms;
                        theServers[s].timeout_ms = system_ini.tcp_ip_port.timeout_ms;
                        theServers[s].ctx = NULL;
                        break;
                    case TCPRTU_SRV:
                        theServers[s].u.tcp_ip.IPaddr = CrossTable[i].IPAddress;
                        theServers[s].u.tcp_ip.port = CrossTable[i].Port;
                        theServers[s].silence_ms = system_ini.tcp_ip_port.silence_ms;
                        theServers[s].timeout_ms = system_ini.tcp_ip_port.timeout_ms;
                        theServers[s].ctx = NULL;
                        break;
                    default:
                        ;
                    }
                    theServers[s].NodeId = CrossTable[i].NodeId;
                    theServers[s].thread_id = XX_WRONG_THREAD;
                    theServers[s].status = SRV_RUNNING0;
                    theServers[s].thread_status = NOT_STARTED;
                    // theServers[s].serverMutex = PTHREAD_MUTEX_INITIALIZER;
                    snprintf(theServers[s].name, MAX_THREADNAME_LEN, "srv[%d]%s_0x%08x_%d", s, fieldbusName[theServers[s].protocol], theServers[s].IPaddress, theServers[s].port);
                    initServerDiagnostic(s);
                    theServers[s].idle_time_ns = 0;
                    theServers[s].busy_time_ns = 0;
                    theServers[s].last_time_ns = 0;
                    theServers[s].last_update_ns = 0;
                }
                break;
            default:
                break;
            }

            // client variables =---> link to the server and add unique devices and nodes
            switch (CrossTable[i].Protocol) {
            case PLC:
                // no plc client
                CrossTable[i].device = 0xffff;
                CrossTable[i].node = 0xffff;
                break;
            case RTU:
            case TCP:
            case TCPRTU:
            case CANOPEN:
            case MECT:
                // add unique variable's device (Protocol, IPAddress, Port, --)
                for (d = 0; d < theDevicesNumber; ++d) {
                    if (CrossTable[i].Protocol == theDevices[d].protocol
                     && CrossTable[i].IPAddress == theDevices[d].IPaddress
                     && CrossTable[i].Port == theDevices[d].port) {
                        // already present
                        break;
                    }
                }
                goto add_device;
                // no break
            case RTU_SRV:
                // add unique variable's device (Protocol, --, Port, --)
                for (d = 0; d < theDevicesNumber; ++d) {
                    if (CrossTable[i].Protocol == theDevices[d].protocol
                     && CrossTable[i].Port == theDevices[d].port) {
                        // already present
                        break;
                    }
                }
                goto add_device;
                // no break
            case TCP_SRV:
            case TCPRTU_SRV:
                // add unique variable's device (Protocol, --, --, --)
                for (d = 0; d < theDevicesNumber; ++d) {
                    if (CrossTable[i].Protocol == theDevices[d].protocol) {
                        // already present
                        break;
                    }
                }
            add_device:
                if (d < theDevicesNumber) {
                    CrossTable[i].device = d; // found
                    theDevices[d].var_num += 1; // this one, also Htype
                } else if (theDevicesNumber >= MAX_DEVICES) {
                    CrossTable[i].device = 0xffff; // FIXME: error
                    fprintf(stderr, "[%s]: too many devices (max=%d)\n", __func__, MAX_DEVICES);
                    retval = -1;
                } else {
                    // new device entry
                    CrossTable[i].device = theDevicesNumber;
                    ++theDevicesNumber;
                    theDevices[d].protocol = CrossTable[i].Protocol;
                    theDevices[d].IPaddress = CrossTable[i].IPAddress;
                    theDevices[d].port = p = CrossTable[i].Port;
                    theDevices[d].var_num = 1; // this one, also Htype
                    theDevices[d].device_vars = NULL; // calloc later on
                    theDevices[d].server = 0xffff;
                    switch (theDevices[d].protocol) {
                    case PLC:
                        // FIXME: assert
                        break;
                    case RTU:
                    case MECT:
                        switch (p) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                            if (system_ini.serial_port[p].baudrate == 0) {
                                fprintf(stderr, "[%s]: missing port %u in system.ini for RTU variable #%u\n", __func__, p, i);
                                retval = -1;
                            } else {
                                theDevices[d].u.serial.port = p;
                                theDevices[d].u.serial.baudrate = system_ini.serial_port[p].baudrate;
                                theDevices[d].u.serial.parity = system_ini.serial_port[p].parity;
                                theDevices[d].u.serial.databits = system_ini.serial_port[p].databits;
                                theDevices[d].u.serial.stopbits = system_ini.serial_port[p].stopbits;
                                theDevices[d].silence_ms = system_ini.serial_port[p].silence_ms;
                                theDevices[d].timeout_ms = system_ini.serial_port[p].timeout_ms;
                                theDevices[d].max_block_size = system_ini.serial_port[p].max_block_size;
                            }
                            break;
                        default:
                            fprintf(stderr, "[%s]: bad %s port %u for variable #%u\n", __func__,
                                    (theDevices[d].protocol == RTU ? "RTU" : "MECT"), p, i);
                            retval = -1;
                        }
                        break;
                    case TCP:
                        ++theTcpDevicesNumber;
                        theDevices[d].u.tcp_ip.IPaddr = CrossTable[i].IPAddress;
                        theDevices[d].u.tcp_ip.port = CrossTable[i].Port;
                        theDevices[d].silence_ms = system_ini.tcp_ip_port.silence_ms;
                        theDevices[d].timeout_ms = system_ini.tcp_ip_port.timeout_ms;
                        theDevices[d].max_block_size = system_ini.tcp_ip_port.max_block_size;
                        break;
                    case TCPRTU:
                        theDevices[d].u.tcp_ip.IPaddr = CrossTable[i].IPAddress;
                        theDevices[d].u.tcp_ip.port = CrossTable[i].Port;
                        theDevices[d].silence_ms = system_ini.tcp_ip_port.silence_ms;
                        theDevices[d].timeout_ms = system_ini.tcp_ip_port.timeout_ms;
                        theDevices[d].max_block_size = system_ini.tcp_ip_port.max_block_size;
                        break;
                    case CANOPEN:
                        switch (p) {
                        case 0:
                        case 1:
                            theDevices[d].u.can.bus = p;
                            theDevices[d].u.can.baudrate = system_ini.canopen[p].baudrate;
                            theDevices[d].silence_ms = 0;
                            theDevices[d].timeout_ms = 0;
                            theDevices[d].max_block_size = system_ini.canopen[p].max_block_size;
                            break;
                        default:
                            fprintf(stderr, "[%s]: bad CANOPEN port %u for variable #%u", __func__, p, i);
                            retval = -1;
                        }
                        break;
                    case RTU_SRV:
                        theDevices[d].server = s; // searched before
                        theDevices[d].silence_ms = system_ini.serial_port[p].silence_ms;
                        theDevices[d].timeout_ms = system_ini.serial_port[p].timeout_ms;
                        theDevices[d].max_block_size = system_ini.serial_port[p].max_block_size;
                        break;
                    case TCP_SRV:
                    case TCPRTU_SRV:
                        theDevices[d].server = s; // searched before
                        theDevices[d].silence_ms = system_ini.tcp_ip_port.silence_ms;
                        theDevices[d].timeout_ms = system_ini.tcp_ip_port.timeout_ms;
                        theDevices[d].max_block_size = system_ini.tcp_ip_port.max_block_size;
                        break;
                    default:
                        ;
                    }
                    snprintf(theDevices[d].name, MAX_THREADNAME_LEN, "dev(%d)%s_0x%08x_%u", d, fieldbusName[theDevices[d].protocol], theDevices[d].IPaddress, theDevices[d].port);
                    if (theDevices[d].timeout_ms == 0 && theDevices[d].protocol == RTU) {
                        theDevices[d].timeout_ms = 300;
                        fprintf(stderr, "[%s]: TimeOut of device '%s' forced to %u ms\n", __func__, theDevices[d].name, theDevices[d].timeout_ms);
                    }
                    if (i == base && CrossTable[i].BlockSize > theDevices[d].max_block_size) {
                        fprintf(stderr, "[%s]: warning: variable #%u block #%u size %u, exceeding max_block_size %u (%s)\n",
                                __func__, i, block, CrossTable[i].BlockSize, theDevices[d].max_block_size, theDevices[d].name);
                    }
                    theDevices[d].elapsed_time_ns = 0;
                    theDevices[d].idle_time_ns = 0;
                    theDevices[d].busy_time_ns = 0;
                    theDevices[d].last_time_ns = 0;
                    theDevices[d].last_update_ns = 0;
                    theDevices[d].status = ZERO;
                    // theDevices[d].thread_id = 0;
                    theDevices[d].thread_status = NOT_STARTED;
                    sem_init(&newOperations[d], 0, 0);
                    theDevices[d].PLCwriteRequestNumber = 0;
                    theDevices[d].PLCwriteRequestGet = 0;
                    theDevices[d].PLCwriteRequestPut = 0;
                    // theDevices[d].modbus_ctx .last_good_ms, PLCwriteRequests, PLCwriteRequestNumber, PLCwriteRequestGet, PLCwriteRequestPut
                    initDeviceDiagnostic(d);
                }
                // add unique variable's node
                for (n = 0; n < theNodesNumber; ++n) {
                     if (CrossTable[i].device == theNodes[n].device) {
                         if (CrossTable[i].Protocol == RTU_SRV || CrossTable[i].Protocol == TCP_SRV  || CrossTable[i].Protocol == TCPRTU_SRV) {
                             // already present (any NodeId)
                             break;
                         } else if (CrossTable[i].NodeId == theNodes[n].NodeID) {
                             // already present
                             break;
                         }
                    }
                }
            //add_node:
                if (n < theNodesNumber) {
                    CrossTable[i].node = n; // found
                } else if (theNodesNumber >= MAX_NODES) {
                    CrossTable[i].node = 0xffff;
                    fprintf(stderr, "[%s]: too many nodes (max=%d)\n", __func__, MAX_NODES);
                    retval = -1;
                } else {
                    // new node entry
                    CrossTable[i].node = theNodesNumber;
                    ++theNodesNumber;
                    theNodes[n].device = CrossTable[i].device;
                    theNodes[n].NodeID = CrossTable[i].NodeId;
                    if (disable_all_nodes) {
                        theNodes[n].status = NODE_DISABLED;
                    } else {
                        theNodes[n].status = NODE_OK;
                    }
                    // theNodes[n].RetryCounter .JumpRead
                    initNodeDiagnostic(n);
                }
                break;
            default:
                break;
            }
        }
    }

  {
    uint16_t d;
    uint16_t var_max[MAX_DEVICES];

    for (i = 1; i <= DimCrossTable; ++i) {
        if (CrossTable[i].Enable > 0) {

            // client variables =---> create and fill the variables addresses array
            switch (CrossTable[i].Protocol) {
            case PLC:
                // no plc client
                break;
            case RTU:
            case TCP:
            case TCPRTU:
            case CANOPEN:
            case MECT:
            case RTU_SRV:
            case TCP_SRV:
            case TCPRTU_SRV:
                d = CrossTable[i].device;
                if (d != 0xffff) {
                    if (theDevices[d].device_vars == NULL) {
                        theDevices[d].device_vars = calloc(theDevices[d].var_num, sizeof(struct device_var));
                        var_max[d] = 0;
                    }
                    if (theDevices[d].device_vars == NULL) {
                        fprintf(stderr, "[%s]: memory full\n", __func__);
                        retval = -1;
                        break;
                    }
                    if (var_max[d] < theDevices[d].var_num) {
                        theDevices[d].device_vars[var_max[d]].addr = i;
                        if (CrossTable[i].Plc > Htype) {
                            theDevices[d].device_vars[var_max[d]].active = 1;
                        }
                        ++var_max[d];
                    }
                }
                break;
            default:
                break;
            }
        }
    }
  }
    return retval;
}

static void initNodeDiagnostic(uint16_t n)
{
    uint16_t addr = 0;
    uint32_t value;

    addr = 5172 + 2 * n;
    theNodes[n].diagnosticAddr = addr;
    value = (theNodes[n].device << 16) + theNodes[n].NodeID;
#if 0
    writeQdataRegisters(addr + DIAGNOSTIC_DEV_NODE, value, DATA_OK);
    writeQdataRegisters(addr + DIAGNOSTIC_NODE_STATUS, theNodes[n].status, DATA_OK);
#else
    VAR_VALUE(addr + DIAGNOSTIC_DEV_NODE) = value;
    VAR_STATE(addr + DIAGNOSTIC_DEV_NODE) = DATA_OK;
    VAR_VALUE(addr + DIAGNOSTIC_NODE_STATUS) = theNodes[n].status;
    VAR_STATE(addr + DIAGNOSTIC_NODE_STATUS) = DATA_OK;
#endif
}

/* ---------------------------------------------------------------------------- */

static inline void setAlarmEvent(int i)
{
    uint16_t addr = ALCrossTable[i].TagAddr;

    if (VAR_VALUE(addr) == 0) {
        // set alarm and call tasks only if currently clear
        writeQdataRegisters(addr, 1, DATA_OK);
        if (ALCrossTable[i].ALType == Alarm) {
            plc_events_raise_alarm();
        } else {
            plc_events_raise_event();
        }
    }
}

static inline void clearAlarmEvent(int i)
{
    writeQdataRegisters(ALCrossTable[i].TagAddr, 0, DATA_OK);
    ALCrossTable[i].ALFilterCount = ALCrossTable[i].ALFilterTime;
}

static inline void checkAlarmEvent(int i, int condition)
{
    if (condition) {
        if (ALCrossTable[i].ALFilterCount == 0) {
            // setting alarm/event
            setAlarmEvent(i);
        } else {
            // filtering alarm/event
            ALCrossTable[i].ALFilterCount = ALCrossTable[i].ALFilterCount - 1;
        }
    } else {
        // clearing alarm/event
        clearAlarmEvent(i);
    }
}

static void AlarmMngr(void)
{
    register uint16_t i;

    // already in pthread_mutex_lock(&theCrosstableClientMutex)
    for (i = 1; i <= lastAlarmEvent; ++i) {

        // fprintf(stderr, "AlarmMngr: Checking Variable Tag Adr: [%d] Source:[ %s]\n", ALCrossTable[i].TagAddr, ALCrossTable[i].ALSource);

        register uint16_t SourceAddr = ALCrossTable[i].SourceAddr;
        register uint16_t Operator = ALCrossTable[i].ALOperator;

        if (VAR_STATE(SourceAddr) != DATA_OK) {
            // unreliable values
	    fprintf(stderr, "AlarmMngr: VAR_STATE of SourceAddr Variable [%d] is not Ok, Skip", SourceAddr);
            continue;
        }

        if (Operator == OPER_RISING || Operator == OPER_FALLING) {
            // checking against old value
            register int32_t SourceValue = VAR_VALUE(SourceAddr);
            register int32_t CompareVal = CrossTable[i].OldVal;

            if (Operator == OPER_RISING) {
                if (CompareVal == 0) {
                    // checking rising edge if currently low
                    checkAlarmEvent(i, SourceValue != 0);
                } else if (SourceValue == 0){
                    // clearing alarm/event only at falling edge
                    clearAlarmEvent(i);
                }
            } else if (Operator == OPER_FALLING)  {
                if (CompareVal != 0) {
                    // checking falling edge if currently high
                    checkAlarmEvent(i, SourceValue == 0);
                } else if (SourceValue != 0){
                    // clearing alarm/event only at rising edge
                    clearAlarmEvent(i);
                }
            }
            // saving the new "old value" :)
            CrossTable[i].OldVal = SourceValue;

        } else {
            register uint16_t CompareAddr = ALCrossTable[i].CompareAddr;
            varUnion SourceValue, CompareVal;

            SourceValue = plcBlock.values[SourceAddr]; // VAR_VALUE(SourceAddr);

            // checking either against fixed value or against variable value
            if (CompareAddr == 0) {
                // fixed value
                CompareVal = ALCrossTable[i].ALCompareVal;
            } else if (VAR_STATE(CompareAddr) != DATA_OK) {
                // unreliable values
        fprintf(stderr, "AlarmMngr: VAR_STATE of CompareAddr Variable [%d] is not Ok, Skip", CompareAddr);
                continue;
            } else {
                CompareVal = plcBlock.values[CompareAddr]; // VAR_VALUE(CompareAddr);
                // FIXME: align decimals and types
            }

            // comparison types
            switch (ALCrossTable[i].comparison) {
            case COMP_UNSIGNED:
                switch (Operator) {
                case OPER_EQUAL     : checkAlarmEvent(i, SourceValue.u32 == CompareVal.u32); break;
                case OPER_NOT_EQUAL : checkAlarmEvent(i, SourceValue.u32 != CompareVal.u32); break;
                case OPER_GREATER   : checkAlarmEvent(i, SourceValue.u32 >  CompareVal.u32); break;
                case OPER_GREATER_EQ: checkAlarmEvent(i, SourceValue.u32 >= CompareVal.u32); break;
                case OPER_SMALLER   : checkAlarmEvent(i, SourceValue.u32 <  CompareVal.u32); break;
                case OPER_SMALLER_EQ: checkAlarmEvent(i, SourceValue.u32 <= CompareVal.u32); break;
                default             : ;
                }
                break;
            case COMP_SIGNED16:
                switch (Operator) {
                case OPER_EQUAL     : checkAlarmEvent(i, SourceValue.i16 == CompareVal.i16); break;
                case OPER_NOT_EQUAL : checkAlarmEvent(i, SourceValue.i16 != CompareVal.i16); break;
                case OPER_GREATER   : checkAlarmEvent(i, SourceValue.i16 >  CompareVal.i16); break;
                case OPER_GREATER_EQ: checkAlarmEvent(i, SourceValue.i16 >= CompareVal.i16); break;
                case OPER_SMALLER   : checkAlarmEvent(i, SourceValue.i16 <  CompareVal.i16); break;
                case OPER_SMALLER_EQ: checkAlarmEvent(i, SourceValue.i16 <= CompareVal.i16); break;
                default             : ;
                }
                break;
            case COMP_SIGNED32:
                switch (Operator) {
                case OPER_EQUAL     : checkAlarmEvent(i, SourceValue.i32 == CompareVal.i32); break;
                case OPER_NOT_EQUAL : checkAlarmEvent(i, SourceValue.i32 != CompareVal.i32); break;
                case OPER_GREATER   : checkAlarmEvent(i, SourceValue.i32 >  CompareVal.i32); break;
                case OPER_GREATER_EQ: checkAlarmEvent(i, SourceValue.i32 >= CompareVal.i32); break;
                case OPER_SMALLER   : checkAlarmEvent(i, SourceValue.i32 <  CompareVal.i32); break;
                case OPER_SMALLER_EQ: checkAlarmEvent(i, SourceValue.i32 <= CompareVal.i32); break;
                default             : ;
                }
                break;
            case COMP_FLOATING:
                switch (Operator) {
                case OPER_EQUAL     : checkAlarmEvent(i, SourceValue.f == CompareVal.f); break;
                case OPER_NOT_EQUAL : checkAlarmEvent(i, SourceValue.f != CompareVal.f); break;
                case OPER_GREATER   : checkAlarmEvent(i, SourceValue.f >  CompareVal.f); break;
                case OPER_GREATER_EQ: checkAlarmEvent(i, SourceValue.f >= CompareVal.f); break;
                case OPER_SMALLER   : checkAlarmEvent(i, SourceValue.f <  CompareVal.f); break;
                case OPER_SMALLER_EQ: checkAlarmEvent(i, SourceValue.f <= CompareVal.f); break;
                default             : ;
                }
                break;
            default:
                ;
            }
        }
    }
}

/* ---------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------- */

static unsigned plc_product_id(unsigned *msVersion)
{
    unsigned retval = 0xFFFFffff;
    FILE *f;

    *msVersion = 0;
    f = fopen(ROOTFS_VERSION, "r");
    if (f) {
        char buf[42];
        char str[42];
        unsigned x = 0, y = 0, z = 0;

        if (fgets(buf, 42, f) == NULL)
            goto close_file;
        if (sscanf(buf, "Release: %6s", str) != 1)
            goto close_file;
        else {
            // Get MS Version
            if (sscanf(str, "%d.%d.%d", &x, &y, &z) == 3)  {
                *msVersion = z | (y << 8) | (x << 16);;
            }
        }

        if (fgets(buf, 42, f) == NULL)
            goto close_file;

        // the order of following tests is important

        // TP1043_01_A TP1043_01_B TP1043_02_A TP1043_02_B
        // TP1057_01_A TP1057_01_B
        // TP1070_01_A TP1070_01_B TP1070_01_C
        // TP1070_02_E
        if (sscanf(buf, "Target: TP%x_%x_%x", &x, &y, &z) == 3)
            retval = ((x & 0xFFFF) << 16) + ((y & 0xFF) << 8) + (z & 0xFF);

        // TPX1043_03_C
        // TPX1070_03_D TPX1070_03_E
        // TPX4100_01_A
        // TPX4120_01_A
        // TPX4150_01_A
        // TPX4190_01_A
        else if (sscanf(buf, "Target: TPX%x_%x_%x", &x, &y, &z) == 3)
            retval = ((x & 0xFFFF) << 16) + ((y & 0xFF) << 8) + (z & 0xFF);

        // TPAC1007_04_AA TPAC1007_04_AB TPAC1007_04_AC TPAC1007_04_AD TPAC1007_04_AE
        // TPAC1008_02_AA TPAC1008_02_AB TPAC1008_02_AD TPAC1008_02_AE TPAC1008_02_AF
        // TPAC1008_03_AC TPAC1008_03_AD
        else if (sscanf(buf, "Target: TPAC%x_%x_%x", &x, &y, &z) == 3)
            retval = ((x & 0xFFFF) << 16) + ((y & 0xFF) << 8) + (z & 0xFF);

        // TPAC1007_03 TPAC1008_01
        else if (sscanf(buf, "Target: TPAC%x_%x", &x, &y) == 2)
            retval = ((x & 0xFFFF) << 16) + ((y & 0xFF) << 8);

        // TPAC1007_LV
        else if (strcmp(buf, "Target: TPAC1007_LV") == 0)
            retval = 0x10075500;

        // TPAC1005 TPAC1006
        else if (sscanf(buf, "Target: TPAC%x", &x) == 1)
            retval = ((x & 0xFFFF) << 16);

        // TPLC050_01_AA TPLC100_01_AA TPLC100_01_AB
        else if (sscanf(buf, "Target: TPLC%x_%x_%x", &x, &y, &z) == 3)
            retval = ((x & 0xFFFF) << 16) + ((y & 0xFF) << 8) + (z & 0xFF);

    close_file:
        fclose(f);
    }

    return retval;
}

static unsigned plc_serial_number()
{
    unsigned retval = 0xFFFFffff;
    FILE *f;

    f = fopen(SERIAL_CONF, "r");
    if (f) {
        char buf[80]; // YYYYMM1234

        if (fgets(buf, (4+2+4+1), f)) {
            retval = strtoul(buf, NULL, 10);
        }
        fclose(f);
    }

    return retval;
}
