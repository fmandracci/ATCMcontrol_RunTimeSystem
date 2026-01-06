/*
 * Copyright 2021 Mect s.r.l
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
 * Filename: dataImpl.h
 */

#ifndef DATAIMPL_H
#define DATAIMPL_H

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "crosstable.h"
#include "system_ini.h"
#include "hmi_plc.h"

#include "xx_gpio.h"
#include "xx_pthread.h"
#include "xx_retentives.h"

#include <modbus/modbus.h>
#include "xx_mect.h"
#include "xx_canopen.h"

#include "../main.h"

#define UN_MILIARDO_ULL  1000000000ull
#define DIECI_MILIONI_UL   10000000ul
#define UN_MILIONE_UL       1000000ul
#define UN_MILIONE_ULL      1000000ull

#if defined(KIT_IMX28)
#define SERIAL_DEVNAME "rtser%u"
#define SERIAL_DEVNUM(n) (n)

#elif defined(KIT_RPI4)
#define SERIAL_DEVNAME "/dev/ttyAMA%u"
#define SERIAL_DEVNUM(n)   ((n)==0 ? 3 : ((n)==1 ?  2 : ((n)==3 ?  4 : (n))))

#else
#error unknown platform

#endif

// -------------------------------------------------------------------------------------------

#if 0

// enabling FGPIO debug output

#define XX_GPIO_INIT()          xx_gpio_init()
#define XX_GPIO_ENABLE(n)       xx_gpio_enable(n)
#define XX_GPIO_CONFIG(n, o)    xx_gpio_config(n, o)
#define XX_GPIO_SET(n)          xx_gpio_set(n)
#define XX_GPIO_CLR(n)          xx_gpio_clr(n)
#define XX_GPIO_CLOSE()         xx_gpio_close()

#define XX_GPIO_TIC_SET()       xx_gpio_tic_set()
#define XX_GPIO_TAC_SET()       xx_gpio_tac_set()
#define XX_GPIO_TIC_CLR()       xx_gpio_tic_clr()
#define XX_GPIO_TAC_CLR()       xx_gpio_tac_clr()

// moreover, enabling FGPIO debug output (for server 0=4; for clients 0=5, 1=6; for upd *=7)

#define XX_GPIO_SRV_SETUP(n) // { if (n == 0) { XX_GPIO_ENABLE(4); XX_GPIO_CONFIG(4, 1); } }
#define XX_GPIO_SRV_SET(n)   // { if (n == 0) { XX_GPIO_SET(4); } }
#define XX_GPIO_SRV_CLR(n)   // { if (n == 0) { XX_GPIO_CLR(4); } }

#define XX_GPIO_DEV_SETUP(n) // { if (n < 1) { XX_GPIO_ENABLE(4 + n); XX_GPIO_CONFIG(4 + n, 1); } }
#define XX_GPIO_DEV_SET(n)   // { if (n < 1) { XX_GPIO_SET(4 + n); } }
#define XX_GPIO_DEV_CLR(n)   // { if (n < 1) { XX_GPIO_CLR(4 + n); } }

#define XX_GPIO_UDP_SETUP()  // { XX_GPIO_ENABLE(7); XX_GPIO_CONFIG(7, 1); }
#define XX_GPIO_UDP_SET()    // { XX_GPIO_SET(7); }
#define XX_GPIO_UDP_CLR()    // { XX_GPIO_CLR(7); }

#else

// disabling all FGPIO debug output

#define XX_GPIO_INIT()
#define XX_GPIO_ENABLE(n)
#define XX_GPIO_CONFIG(n, o)
#define XX_GPIO_SET(n)
#define XX_GPIO_CLR(n)
#define XX_GPIO_CLOSE()

#define XX_GPIO_TIC_SET()
#define XX_GPIO_TAC_SET()
#define XX_GPIO_TIC_CLR()
#define XX_GPIO_TAC_CLR()

#define XX_GPIO_SRV_SETUP(n)
#define XX_GPIO_SRV_SET(n)
#define XX_GPIO_SRV_CLR(n)

#define XX_GPIO_DEV_SETUP(n)
#define XX_GPIO_DEV_SET(n)
#define XX_GPIO_DEV_CLR(n)

#define XX_GPIO_UDP_SETUP()
#define XX_GPIO_UDP_SET()
#define XX_GPIO_UDP_CLR()

#endif

// -------------------------------------------------------------------------------------------

#define HMI_INI        "/local/root/hmi.ini"
#define ROOTFS_VERSION "/rootfs_version"
#define SERIAL_CONF    "/etc/serial.conf"

#define THE_CONFIG_DELAY_ms       10
#define THE_ENGINE_DELAY_ms       10
#define THE_IDLE_DELAY_ms        100
#define THE_SERVER_DELAY_ms     1000
#define THE_CONNECTION_DELAY_ms 1000

#define THE_DEVICE_BLACKLIST_ns  4000000000LL //  4s
#define THE_DEVICE_SILENCE_ns   20000000000LL // 20s = tpac boot time

#define THE_MAX_CLIENT_SLEEP_ns 1E9


extern struct system_ini system_ini;

// -------- ALL SERVERS (RTU_SRV, TCP_SRV, TCPRTU_SRV) ------------
#define REG_SRV_NUMBER      4096
#define THE_SRV_SIZE        (REG_SRV_NUMBER * sizeof(uint16_t)) // 0x00002000 8kB
#define	THE_SRV_MAX_CLIENTS	10

enum fieldbusError {NoError = 0, CommError, TimeoutError, ConnReset};

enum EngineStatus { enIdle = 0, enInitialized, enRunning, enError, enExiting };

/* ----  Global Variables:	 -------------------------------------------------- */

extern int verbose_print_enabled;
extern int timer_overflow_enabled;

extern unsigned buzzer_beep_ms;
extern unsigned buzzer_on_cs;
extern unsigned buzzer_off_cs;
extern unsigned buzzer_replies;

extern unsigned buzzer_period_tics;
extern unsigned buzzer_tic;
extern unsigned buzzer_periods;

/* ------- dataImpl.c -------- */

void dataEngineStart(void);
void dataEngineStop(void);
void dataEnginePwrFailStop(void);
void dataEnableVerbosePrint(void);
void dataEnableTimerOverflow(void);

/* ------- dataEngine.c -------- */

extern uint32_t *xx_retentives_ptr;
extern int do_flush_retentives;

extern enum EngineStatus engineStatus;
extern pthread_mutex_t theCrosstableClientMutex;
extern pthread_cond_t theAlarmsEventsCondvar;
extern pthread_cond_t theClockCondvar;
extern sem_t newOperations[MAX_DEVICES];

void engineInit();
void *engineThread(void *statusAdr);
void engineFastIO_refresh_no_lock();

void *clockThread(void *statusAdr); // dataClock.c
void *clientThread(void *statusAdr); // dataClients.c
void *serverThread(void *statusAdr); // dataServers.c
void *fastIOThread(void *statusAdr); // dataFastIO.c

void *datasyncThread(void *statusAdr); // dataHmiPlc.c

/* ------- dataUtils.c -------- */

void setEngineStatus(enum EngineStatus status);

unsigned doWriteVariable(unsigned addr, unsigned value, uint32_t *values, uint32_t *flags, unsigned addrMax);
void writeQdataRegisters(uint16_t addr, uint32_t value, uint8_t status);

/* ------- dataDiagnostics.c -------- */

#define DIAGNOSTIC_TYPE_PORT    0
#define DIAGNOSTIC_BAUDRATE     1 // IP_ADDRESS/BAUDRATE
#define DIAGNOSTIC_STATUS       2
#define DIAGNOSTIC_READS        3
#define DIAGNOSTIC_WRITES       4
#define DIAGNOSTIC_TIMEOUTS     5
#define DIAGNOSTIC_COMM_ERRORS  6
#define DIAGNOSTIC_LAST_ERROR   7
#define DIAGNOSTIC_WRITE_QUEUE  8
#define DIAGNOSTIC_BUS_LOAD     9

//1;P;RTU0_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_BAUDRATE   ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_STATUS     ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_READS      ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_WRITES     ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_TIMEOUTS   ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_COMM_ERRORS;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_LAST_ERROR ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_WRITE_QUEUE;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO]
//1;P;RTU0_BUS_LOAD   ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5000;10  ;[RO] ex RTU0_READ_QUEUE

//1;P;RTU2_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5010;10  ;[RO]
//1;P;RTU3_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5020;10  ;[RO]
//1;P;CAN0_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5030;10  ;[RO]
//1;P;CAN1_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5040;10  ;[RO]
//1;P;TCPS_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5050;10  ;[RO]
//1;P;TCP0_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5060;10  ;[RO]
//1;P;TCP1_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5070;10  ;[RO]
//1;P;TCP2_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5080;10  ;[RO]
//1;P;TCP3_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5080;10  ;[RO]
//1;P;TCP4_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5100;10  ;[RO]
//1;P;TCP5_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5110;10  ;[RO]
//1;P;TCP6_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5120;10  ;[RO]
//1;P;TCP7_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5130;10  ;[RO]
//1;P;TCP8_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5140;10  ;[RO]
//1;P;TCP9_TYPE_PORT  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5150;10  ;[RO]

#define DIAGNOSTIC_DEV_NODE     0
#define DIAGNOSTIC_NODE_STATUS  1
//1;P;NODE_01_DEV_NODE;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5172;2   ;[RO]
//1;P;NODE_01_STATUS  ;UDINT    ;0   ;PLC       ;               ;    ;    ;    ;5172;2   ;[RO]

void initServerDiagnostic(uint16_t s);
void initDeviceDiagnostic(uint16_t d);
void setDiagnostic(uint16_t addr, uint16_t offset, uint32_t value);
void incDiagnostic(uint16_t addr, uint16_t offset);

/* ------- dataRetentives.c -------- */

#endif // DATAIMPL_H
