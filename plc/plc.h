#ifndef PLC_H
#define PLC_H

extern void *plcEngineThread(void *statusAdr);
extern void plcEngineStop();

extern void plc_events_setup();
extern void plc_events_raise_alarm();
extern void plc_events_raise_event();

#include <bits/types/siginfo_t.h>
extern void crash_handler(int signum, siginfo_t *siginfo, void *context);

#endif // PLC_H
