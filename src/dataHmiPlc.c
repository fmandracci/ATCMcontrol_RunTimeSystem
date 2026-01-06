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
 * Filename: dataHmiPlc.c
 */

#include "dataImpl.h"

/* ---------------------------------------------------------------------------- */

#define THE_UDP_TIMEOUT_ms	500

static void PLCsync_clearHvars(void);
static void PLCsync_do_read(uint16_t addr);
static unsigned PLCsync_do_write(uint16_t addr);
static void PLCsync();

/* ---------------------------------------------------------------------------- */

void *datasyncThread(void *statusAdr)
{
    PlcServer *plcServer = NULL;

    // datasync
    int threadInitOK = FALSE;
    enum threadStatus *threadStatusPtr = (enum threadStatus *)statusAdr;

    // thread init (datasync)
    xx_pthread_setsched(SCHED_FIFO, XX_PRIO_LOW); // datasyncThread
    plcServer = newPlcServer();
    threadInitOK = (plcServer != NULL);

    // run (datasync)
    *threadStatusPtr = RUNNING;
    XX_GPIO_UDP_SETUP();
    XX_GPIO_UDP_SET();
    while (engineStatus != enExiting) {

        // trivial scenario
        if ((engineStatus != enRunning && engineStatus != enError) || !threadInitOK) {
            XX_GPIO_UDP_CLR();
            xx_sleep_ms(THE_UDP_TIMEOUT_ms);
            XX_GPIO_UDP_SET();
            continue;
        }

        // (1) recv
        XX_GPIO_UDP_CLR();
        if (plcServerWait(plcServer, &hmiBlock, THE_UDP_TIMEOUT_ms) <= 0) {
            XX_GPIO_UDP_SET();
            continue;
        }

        // (2) compute
        pthread_mutex_lock(&theCrosstableClientMutex);
        {
            XX_GPIO_UDP_SET();
            if (engineStatus != enExiting) {
                PLCsync();
            }
            XX_GPIO_UDP_CLR();
        }
        pthread_mutex_unlock(&theCrosstableClientMutex);

        XX_GPIO_UDP_SET();
        // (3) copy the received seqnum
        plcBlock.seqnum = hmiBlock.seqnum;

        // (4) reply
        plcServerReply(plcServer, &plcBlock);
    }

    // thread clean (data)
    deletePlcServer(plcServer);

    // exit
    XX_GPIO_UDP_CLR();
    fprintf(stderr, "[%s]: EXITING\n", __func__);
    *threadStatusPtr = EXITING;
    return NULL;
}

/* ---------------------------------------------------------------------------- */

static void PLCsync_clearHvars(void)
{
    uint16_t addr;
    int d, n;

    // clear H variables status for each device
    for (d = 0; d < theDevicesNumber; ++d) {
        for (n = 0; n < theDevices[d].var_num; ++n) {
            addr = theDevices[d].device_vars[n].addr;
            if (CrossTable[addr].Plc == Htype) {
                theDevices[d].device_vars[n].active = 0;
            }
        }
    }
}

static void PLCsync_do_read(uint16_t addr)
{
    int d, n;

    // set H variables status
    if (CrossTable[addr].Plc == Htype && CrossTable[addr].device != 0xffff) {
        d = CrossTable[addr].device;
        for (n = 0; n < theDevices[d].var_num; ++n) {
            if (theDevices[d].device_vars[n].addr == addr) {
                theDevices[d].device_vars[n].active = 1;
                break;
            }
        }
    }
    switch (CrossTable[addr].Protocol) {
    case PLC:
        // immediate read: no fieldbus
        break;
    case RTU:
    case TCP:
    case TCPRTU:
    case CANOPEN:
    case MECT:
    case RTU_SRV:
    case TCP_SRV:
    case TCPRTU_SRV:
        // consider only "H" variables, because the "P,S,F" are already managed by clientThread
        if (CrossTable[addr].Plc == Htype && CrossTable[addr].device != 0xffff) {
            // activate semaphore only the first time
            sem_post(&newOperations[CrossTable[addr].device]);
        } else {
            // already done in PLC_sync(): plcBlock.states[addr] = varStatus_DO_READ;
        }
        break;
    default:
        ;
    }
}

static unsigned PLCsync_do_write(uint16_t addr)
{
    unsigned written = 0;

    switch (CrossTable[addr].Protocol) {
    case PLC: {
        // immediate write: no fieldbus
        uint32_t value = hmiBlock.values[addr].u32;
        writeQdataRegisters(addr, value, DATA_OK);
        written = 1;
    }   break;
    case RTU:
    case TCP:
    case TCPRTU:
    case CANOPEN:
    case MECT:
    case RTU_SRV:
    case TCP_SRV:
    case TCPRTU_SRV:
        if (CrossTable[addr].device != 0xffff) {
            // search for consecutive writes
            register int i;

            for (i = 1; i < MAX_VALUES && (addr + i) <= DimCrossTable; ++i) {
                register uint16_t addr_i = addr + i;
                register enum varStatus requestedStatus_i = hmiBlock.states[addr_i];

                if (requestedStatus_i == varStatus_DO_WRITE
                    && CrossTable[addr].device == CrossTable[addr_i].device)
                    continue;
                else
                    break;
            }

            // trying multiple writes
            i = i - 1; // may be 0
            written = doWriteVariable(addr, hmiBlock.values[addr].u32, (uint32_t *)hmiBlock.values, NULL, addr + i);
        }
        break;
    default:
        ;
    }
    return written;
}

static void PLCsync()
{
    uint16_t addr;
    uint16_t first = 1;
    uint16_t last = DimCrossTable;
    unsigned written = 0;

    // protocol check and optimization
    if (hmiBlock.first >= 1 && hmiBlock.first <= DimCrossTable) {
        first = hmiBlock.first;
    }
    if (hmiBlock.last >= 1 && hmiBlock.last <= DimCrossTable) {
        last = hmiBlock.last;
    }
    if (first > last) {
        // should not happen
        first = 1;
        last = DimCrossTable;
    }

    // already in pthread_mutex_lock(&theCrosstableClientMutex)
    PLCsync_clearHvars();
    for (addr = first; addr <= last; ++addr) {
        register enum varStatus requestedStatus = hmiBlock.states[addr];

        switch (requestedStatus) {

        case varStatus_NOP:
            break;

        case varStatus_DO_READ:
            PLCsync_do_read(addr);
            break;

        case varStatus_PREPARING:
            break;

        case varStatus_DO_WRITE:
            written = PLCsync_do_write(addr);
            // check for multiple writes
            if (written > 1) {
                addr += (written - 1);
            }
            break;

        case varStatus_DATA_OK:
        case varStatus_DATA_WARNING:
        case varStatus_DATA_ERROR:
            // mhhh, it should not happen
            break;
        default:
            ;
        }
    }
}

/* ---------------------------------------------------------------------------- */
