#include "plc.h"

#include "../src/dataImpl.h"
#include <stdio.h>
#include <stdlib.h>

enum Status { IO_INIT, IO_CONFIG, IO_START, IO_RUNNING,
           IO_NOTIFY_SET, IO_NOTIFY_GET,
           IO_STOP, IO_TERMINATE, IO_EXIT };

static char *get_signal(int signum);
static void plc_setup();
static void plc_loop();

// #define IEC_UINT  uint16_t
// #define IEC_UDINT uint32_t

// IEC_UINT dataNotifyGet(IEC_UINT uIOLayer, SIOConfig *pIO, SIONotify *pNotify);
// IEC_UINT dataNotifySet(IEC_UINT uIOLayer, SIOConfig *pIO, SIONotify *pNotify);
// static inline void doWriteBytes(uint32_t *values, uint32_t *flags, unsigned ulOffs, unsigned uLen);
// static inline void doWriteBit(unsigned ulOffs, unsigned usBit, int bit);

// ---------------------------------------------------------------------------

static int do_exit = 0;
static enum Status status = IO_INIT;

void *plcEngineThread(void *statusAdr)
{
    enum threadStatus *threadStatusPtr = (enum threadStatus *)statusAdr;

    plc_setup();
    *threadStatusPtr = RUNNING;

    while (status != IO_EXIT) {
        xx_sleep_ms(100);
        plc_loop();
    }
    *threadStatusPtr = EXITING;
    return NULL;
}

void plcEngineStop()
{
    do_exit = 1;
}

// ---------------------------------------------------------------------------

void plc_setup()
{
    // vedi scheduling in vmKernel/vmmMain.c
}

void plc_loop()
{
    // IEC_UINT uRes = 0;
    // IEC_UINT uIOLayer = 0;
    // SIONotify theNotify;
    // SIOConfig		IO;
    //
    // uRes = osMain(argc, argv);
    // return (uRes ? EXIT_SUCCESS : EXIT_FAILURE);

    switch (status) {

    // setup
    case IO_INIT:
        // dataInitialize(uIOLayer)
        status = do_exit ? IO_STOP : IO_CONFIG;
        break;
    case IO_CONFIG:
        // uRes = dataNotifyConfig(uIOLayer, &IO);
        status = do_exit ? IO_STOP : IO_START;
        break;
    case IO_START:
        // uRes = dataNotifyStart(uIOLayer, &IO);
        status = do_exit ? IO_STOP : IO_RUNNING;
        break;

        // loop
    case IO_RUNNING:
        // vedi SIGINT -> termination_handler() -> ReleaseResources() -> dataEngineStop() -> pthread_join() + dumpRetentives();
        // vedi SIGPWR -> pwrfail_handler() -> dataEnginePwrFailStop() -> pthread_mutex_lock() + dumpRetentives();
        status = do_exit ? IO_STOP : IO_RUNNING;
        break;
    case IO_NOTIFY_SET:
        // uRes = dataNotifySet(uIOLayer, &IO, &theNotify);
        status = do_exit ? IO_STOP : IO_RUNNING;
        break;
    case IO_NOTIFY_GET:
        // uRes = dataNotifyGet(uIOLayer, &IO, &theNotify);
        status = do_exit ? IO_STOP : IO_RUNNING;
        break;

        // shutdown
    case IO_STOP:
        // uRes = dataNotifyStop(uIOLayer, &IO);
        status = IO_TERMINATE;
        break;
    case IO_TERMINATE:
        // uRes = dataFinalize(uIOLayer, &IO);
        status = IO_EXIT;
        break;
    case IO_EXIT:
    default:
        break;
    }
}

// ---------------------------------------------------------------------------

// #include "inc/stdInc.h"
// static STaskInfoVMM *pVMM = NULL;

void plc_events_setup()
{
//    pVMM = get_pVMM();
}

void plc_events_raise_alarm()
{
//    vmmSetEvent(pVMM, EVT_RESERVED_11); // 27u --> EVENT:='28'
}

void plc_events_raise_event()
{
//    vmmSetEvent(pVMM, EVT_RESERVED_10); // 26u --> EVENT:='27'
}

// ---------------------------------------------------------------------------

static char *get_signal(int signum)
{
    switch (signum)
    {
    case SIGILL:  return "SIGILL";
    case SIGFPE:  return "SIGFPE";
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS:  return "SIGBUS";
    case SIGTRAP: return "SIGTRAP";
    case SIGINT:  return "SIGINT";
    default :
    {
        static char szDummy[100];
        sprintf(szDummy, "%d", signum);
        return szDummy;
    }
    }
}

// ---------------------------------------------------------------------------

#include <sys/time.h>
#define LOG_FILE_1		"/local/root/crash_trace"
#define LOG_FILE_2		"/dev/console"

volatile sig_atomic_t crash_handler_active = 0;

void crash_handler(int signum, siginfo_t *siginfo, void *context)
{
    char buf[256];
    int fd,len;

    struct timeval	tv;
    struct tm		tm;

    (void)context;

    fd = open(LOG_FILE_1, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR);
    if (fd < 0)
    {
        fd = open(LOG_FILE_2, O_WRONLY|O_NOCTTY);
    }

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    len = sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d.%03d: ",
                  tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
                  tm.tm_hour,tm.tm_min,tm.tm_sec,(int)(tv.tv_usec/1000));

    len += sprintf(buf+len, "User Mode Exception - Signal: %s\n", get_signal(signum));
    if (write(fd,buf,len)) {}

    len = sprintf(buf, "APP: %s, TASK: %d, ERRNO: %d, CODE: %08X\n",
                  APPLICATION_NAME, getpid(), siginfo->si_errno, siginfo->si_code);
    if (write(fd,buf,len)) {}

    len = sprintf(buf, "--------------------------------------------------------------------------\n");
    if (write(fd,buf,len)) {}

    len += sprintf(buf+len, "\n\n");
    if (write(fd,buf,len)) {}
    close(fd);
    sync();

    /* Forward the signal
     */

    if (crash_handler_active != 0)
    {
        raise(signum);
    }

    crash_handler_active = 1;

    struct sigaction new_action;

    new_action.sa_flags = 0;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_handler = SIG_DFL;

    sigaction(signum, &new_action, NULL);
    raise(signum);
}

// // ----------------------------------------------------------------------------
//
// IEC_UINT dataNotifyStop(IEC_UINT uIOLayer, SIOConfig *pIO)
// {
//     IEC_UINT uRes = OK;
//     (void)uIOLayer;
//     (void)pIO;
//
// #ifdef VERBOSE_DEBUG
//     fprintf(stderr,"[%s]: called\n", __func__);
// #endif
//
//     RETURN(uRes);
// }

// // ----------------------------------------------------------------------------
//
// IEC_UINT dataNotifySet(IEC_UINT uIOLayer, SIOConfig *pIO, SIONotify *pNotify)
// {
//     IEC_UINT uRes = OK;
//
//     if (engineStatus != enExiting) {
//         if (engineStatus != enRunning) {
//             fprintf(stderr, "dataNotifySet: called with no engine running\n");
//             uRes = ERR_NOT_CONFIGURED;
//             RETURN(uRes);
//         }
//         pthread_mutex_lock(&theCrosstableClientMutex);
//         {
//             if (pNotify->uTask != 0xffffu) {
//                 // notify from a plc task
//
//                 // check the write copy regions
//                 SImageReg   *pIRs = (SImageReg *)pIO->C.pAdr;
//                 SImageReg   *pIR = &pIRs[pNotify->uTask];
//
//                 if (pIR->pSetQ[uIOLayer]) {
//                     // write from __%Q__ segment only if changed (using the %W write flags)
//                     void *pvQsegment = pIO->Q.pAdr + pIO->Q.ulOffs;
//                     void *pvWsegment = pIO->W.pAdr + pIO->W.ulOffs;
//                     uint32_t *values = (uint32_t *)pvQsegment;
//                     uint32_t *flags = (uint32_t *)pvWsegment;
//
//                     doWriteBytes(values, flags, 0, (1 + DimCrossTable)*sizeof(uint32_t));
//                 }
//             } else if (pNotify->usSegment != SEG_OUTPUT) {
//                 uRes = ERR_WRITE_TO_INPUT;
//             } else {
//                 // notify from others
//                 IEC_UDINT ulStart = vmm_max(pNotify->ulOffset, pIO->Q.ulOffs);
//                 IEC_UDINT ulStop = vmm_min(pNotify->ulOffset + pNotify->uLen, pIO->Q.ulOffs + pIO->Q.ulSize);
//
//                 if (pNotify->usBit == 0) {
//                     // write a byte region
//                     if (ulStart < ulStop) {
// #ifdef VERBOSE_DEBUG
//                         int i;
//                         unsigned char *p;
//
//                         fprintf(stderr, "calling doWriteBytes() uTask=0x%04x ulStart=%u ulOffs=%u len=%u data[]=",
//                                 pNotify->uTask, ulStart, pIO->Q.ulOffs, pNotify->uLen);
//                         p = pIO->Q.pAdr + (ulStart - pIO->Q.ulOffs);
//                         for (i = 0; i < (pNotify->uLen + 2); ++i) {
//                             fprintf(stderr, " %02x", p[i]);
//                         }
//                         fprintf(stderr, "\n");
// #endif
//                         doWriteBytes((uint32_t *)(pIO->Q.pAdr), NULL, (ulStart - pIO->Q.ulOffs), pNotify->uLen);
//                     }
//                 } else {
//                     // write a bit in a byte
//                     void * source = pIO->Q.pAdr + ulStart;
//                     IEC_UINT uM = (IEC_UINT)(1u << (pNotify->usBit - 1u));
//                     if ((*(IEC_DATA *)source) & uM) {
//                         doWriteBit(ulStart - pIO->Q.ulOffs, pNotify->usBit, 1);
//                     } else {
//                         doWriteBit(ulStart - pIO->Q.ulOffs, pNotify->usBit, 0);
//                     }
//                 }
//             }
//         }
//         pthread_mutex_unlock(&theCrosstableClientMutex);
//     }
//     RETURN(uRes);
// }
//
// // ----------------------------------------------------------------------------
//
// IEC_UINT dataNotifyGet(IEC_UINT uIOLayer, SIOConfig *pIO, SIONotify *pNotify)
// {
//     IEC_UINT uRes = OK;
//
//     if (engineStatus != enExiting) {
//         if (engineStatus != enRunning) {
//             fprintf(stderr, "dataNotifyGet: called with no running engine\n");
//             uRes = OK; // ERR_NOT_CONFIGURED;
//             RETURN(uRes);
//         }
//         pthread_mutex_lock(&theCrosstableClientMutex);
//         {
//             if (pNotify->uTask != 0xffffu) {
//                 // notify from a plc task
//
//                 // check the read copy regions
//                 SImageReg   *pIRs = (SImageReg *)pIO->C.pAdr;
//                 SImageReg   *pIR = &pIRs[pNotify->uTask];
//
//                 if (pIR->pGetQ[uIOLayer] == FALSE && pIR->pGetI[uIOLayer] == FALSE) {
//                     // nothing to do
//                     uRes = OK;
//                 } else {
//                     // search the read copy regions for requests to this IOLayer
//                     IEC_UINT	r;
//
//                     for (r = 0; uRes == OK && r < pIR->uRegionsRd; ++r) {
//                         if (pIR->pRegionRd[r].pGetQ[uIOLayer] == FALSE && pIR->pRegionRd[r].pGetI[uIOLayer] == FALSE) {
//                             continue;
//                         }
//                         IEC_UDINT	ulStart;
//                         IEC_UDINT	ulStop;
//                         void * source;
//                         void * dest;
//
//                         source = plcBlock.values;
//                         if (pIR->pRegionRd[r].usSegment == SEG_OUTPUT) {
//                             ulStart = vmm_max(pIR->pRegionRd[r].ulOffset, pIO->Q.ulOffs);
//                             ulStop	= vmm_min(pIR->pRegionRd[r].ulOffset + pIR->pRegionRd[r].uSize, pIO->Q.ulOffs + pIO->Q.ulSize);
//                             source += ulStart - pIO->Q.ulOffs;
//                             dest = pIO->Q.pAdr + ulStart;
//                         } else { // pIR->pRegionRd[r].usSegment == SEG_INPUT
//                             ulStart = vmm_max(pIR->pRegionRd[r].ulOffset, pIO->I.ulOffs);
//                             ulStop	= vmm_min(pIR->pRegionRd[r].ulOffset + pIR->pRegionRd[r].uSize, pIO->I.ulOffs + pIO->I.ulSize);
//                             source += ulStart - pIO->I.ulOffs;
//                             dest = pIO->I.pAdr + ulStart;
//                         }
//                         if (ulStart < ulStop) {
//                             OS_MEMCPY(dest, source, ulStop - ulStart);
//                         }
//                     }
//                 }
//             } else if (pNotify->usSegment != SEG_INPUT && pNotify->usSegment != SEG_OUTPUT){
//                 uRes = ERR_INVALID_PARAM;
//             } else {
//                 // notify from others
//                 IEC_UDINT	ulStart;
//                 IEC_UDINT	ulStop;
//                 void * source;
//                 void * dest;
//
//                 source = plcBlock.values;
//                 if (pNotify->usSegment == SEG_INPUT) {
//                     ulStart	= vmm_max(pNotify->ulOffset, pIO->I.ulOffs);
//                     ulStop	= vmm_min(pNotify->ulOffset + pNotify->uLen, pIO->I.ulOffs + pIO->I.ulSize);
//                     source += ulStart - pIO->I.ulOffs;
//                     dest = pIO->I.pAdr + ulStart;
//                 } else { // pNotify->usSegment == SEG_OUTPUT
//                     ulStart	= vmm_max(pNotify->ulOffset, pIO->Q.ulOffs);
//                     ulStop	= vmm_min(pNotify->ulOffset + pNotify->uLen, pIO->Q.ulOffs + pIO->Q.ulSize);
//                     source += ulStart - pIO->Q.ulOffs;
//                     dest = pIO->Q.pAdr + ulStart;
//                 }
//                 if (pNotify->usBit == 0) {
//                     if (ulStart < ulStop) {
//                         OS_MEMCPY(dest, source, ulStop - ulStart);
//                     }
//                 } else {
//                     IEC_UINT uM = (IEC_UINT)(1u << (pNotify->usBit - 1u));
//                     IEC_DATA byte = *(IEC_DATA *)dest & ~uM;
//                     byte |= *(IEC_DATA *)source & uM;
//                     *(IEC_DATA *)dest = byte;
//                 }
//             }
//         }
//         pthread_mutex_unlock(&theCrosstableClientMutex);
//     }
//     RETURN(uRes);
// }

// // ----------------------------------------------------------------------------
//
// static inline void doWriteBytes(uint32_t *values, uint32_t *flags, unsigned ulOffs, unsigned uLen)
// {
//     unsigned addrMin = ulOffs / 4;
//     unsigned addrMax = (ulOffs + uLen) / 4;
//     unsigned shiftMin = ulOffs % 4 ; // 0 1 2 3
//     unsigned shiftMax = (ulOffs + uLen) % 4 ; // 0 1 2 3
//     unsigned addr, written, n;
//
//     if (shiftMin > 0)
//         fprintf(stderr, "[%s]: called with %u shiftMin instead of 0\n", __func__, shiftMin);
//     if (shiftMax > 0) {
//         addrMax += 1;
//     }
//
//     for (addr = addrMin; addr < addrMax && addr <= DimCrossTable; ++addr) {
//
//         if (flags && flags[addr] == 0)
//             continue;
//
//         written = doWriteVariable(addr, values[addr], values, flags, addrMax);
//
//         if (flags) {
//             for (n = 0; n < written; ++n)
//                 flags[addr + n] = 0;
//         }
//
//         if (written > 1)
//             addr += written - 1;
//     }
// }
//
// static inline void doWriteBit(unsigned ulOffs, unsigned usBit, int bit)
// {
//     unsigned addr = ulOffs / 4;
//     unsigned shift = ulOffs % 4 * 8; // 0 8 16 24
//     uint32_t mask;
//     uint32_t value;
//     unsigned written;
//
//     mask = 1 << (usBit - 1); // 0x01 ... 0x80
//     mask = mask << shift;    // 0x00000001 ... 0x80000000
//
//     if (bit)
//         value = VAR_VALUE(addr) |= mask;
//     else
//         value = VAR_VALUE(addr) &= ~mask;
//
//     written = doWriteVariable(addr, value, NULL, NULL, addr);
//
//     if (written > 1)
//         fprintf(stderr, "[%s]: wrote %u variables instead of 1", __func__, written);
// }
