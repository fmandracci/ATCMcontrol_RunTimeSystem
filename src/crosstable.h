#ifndef CROSSTABLE_H
#define CROSSTABLE_H

// -------------------------------------------------------------------------------------------

#include <stdint.h>
#include <pthread.h>

#include "hmi_plc.h"
#include <modbus.h>

#define CROSSTABLE_CSV "/local/etc/sysconfig/Crosstable.csv"

#define DimCrossTable   5472
#define DimAlarmsCT     1152
#define LAST_RETENTIVE  192

#define MAX_SERVERS  5 // 3 RTU_SRV + 1 TCP_SRV + 1 TCPRTU_SRV (PLC in dataMain->dataNotifySet/Get)
#define MAX_DEVICES 16 // 3 RTU + n TCP + m TCPRTU + 2 CANOPEN + 1 RTUSRV + 1 TCPSRV + 1 TCPRTUSRV
#define MAX_NODES   64 //

#define MAX_VALUES  64 // 16
#define MAX_PRIORITY 3

#define MAX_IPADDR_LEN      17 // 123.567.901.345.
#define MAX_NUMBER_LEN      12 // -2147483648. -32768.
#define MAX_IDNAME_LEN      32 // abcdefghijklmno.
#define MAX_VARTYPE_LEN      9 // UDINTABCD.
#define MAX_PROTOCOL_LEN     9 // TCPRTUSRV.

enum FieldbusType {PLC = 0, RTU, TCP, TCPRTU, CANOPEN, MECT, RTU_SRV, TCP_SRV, TCPRTU_SRV};
enum UpdateType { Htype = 0, Ptype, Stype, Ftype, Vtype, Xtype};
enum EventAlarm { Event = 0, Alarm};

enum varTypes { vt_BIT = 0,     // BIT
                vt_BYTE_BIT,    // BYTE_BIT
                vt_WORD_BIT,    // WORD_BIT
                vt_DWORD_BIT,   // DWORD_BIT
                vt_UINT8,       // BYTE
                vt_UINT16,      // UINT     UINTAB
                vt_UINT16BA,    // UINTBA
                vt_INT16,       // INT      INTAB
                vt_INT16BA,     // INTBA
                vt_REAL,        // REAL     FABCD
                vt_REALDCBA,    // REALDCBA FDCBA
                vt_REALCDAB,    // REALCDAB FCDBA
                vt_REALBADC,    // REALBADC FBADC
                vt_UDINT,       // UDINT    UDINTABCD
                vt_UDINTDCBA,   // UDINTDCBA
                vt_UDINTCDAB,   // UDINTCDAB
                vt_UDINTBADC,   // UDINTBACD
                vt_DINT,        // DINT     DINTABCD
                vt_DINTDCBA,    // DINTDCBA
                vt_DINTCDAB,    // DINTCDAB
                vt_DINTBADC,    // DINTBADC
                UNKNOWN};

#define OPER_GREATER    41
#define OPER_GREATER_EQ 42
#define OPER_SMALLER    43
#define OPER_SMALLER_EQ 44
#define OPER_EQUAL      45
#define OPER_NOT_EQUAL  46
#define OPER_RISING     47
#define OPER_FALLING    48

#define COMP_UNSIGNED   77
#define COMP_SIGNED16   78
#define COMP_SIGNED32   79
#define COMP_FLOATING   80

// -------------------------------------------------------------------------------------------

char *strtok_csv(char *string, const char *separators, char **savedptr);
uint32_t str2ipaddr(const char *str);
char *ipaddr2str(uint32_t ipaddr, char *buffer);

// -------------------------------------------------------------------------------------------

struct CrossTableRecord {
    int16_t Enable;
    enum UpdateType Plc;
    char Tag[MAX_IDNAME_LEN];
    enum varTypes Types;
    uint16_t Decimal;
    enum FieldbusType Protocol;
    uint32_t IPAddress;
    uint16_t Port;
    uint8_t NodeId;
    uint32_t Offset;
    uint16_t Block;
    uint16_t BlockBase;
    uint16_t BlockSize;
    int Output;
    int16_t Counter;
    int32_t OldVal;
    int usedInAlarmsEvents;
    //
    uint16_t device;
    uint16_t node;
};

extern struct CrossTableRecord CrossTable[1 + DimCrossTable];	 // campi sono riempiti a partire dall'indice 1

// -------------------------------------------------------------------------------------------

extern HmiPlcBlock plcBlock;
extern HmiPlcBlock hmiBlock;
#define THE_DATA_SIZE sizeof(HmiPlcBlock)

#define VAR_VALUE(n) plcBlock.values[n].u32
#define VAR_STATE(n) plcBlock.states[n]

#define DATA_OK      varStatus_DATA_OK
#define DATA_WARNING varStatus_DATA_WARNING
#define DATA_ERROR   varStatus_DATA_ERROR

// -------------------------------------------------------------------------------------------

struct Alarms {
    enum EventAlarm ALType;
    uint16_t TagAddr;
    char ALSource[MAX_IDNAME_LEN];
    char ALCompareVar[MAX_IDNAME_LEN];
    uint16_t SourceAddr;
    uint16_t CompareAddr;
    varUnion  ALCompareVal;
    uint16_t ALOperator;
    uint16_t ALFilterTime;
    uint16_t ALFilterCount;
    uint16_t ALError;
    int comparison;
};

extern struct Alarms ALCrossTable[1 + DimAlarmsCT]; // campi sono riempiti a partire dall'indice 1
extern uint16_t lastAlarmEvent ;

// -------------------------------------------------------------------------------------------

#define RTIME uint64_t
extern uint64_t rt_timer_read();
#define TIMESPEC_FROM_RTIME(ts, rt) { ts.tv_sec = rt / UN_MILIARDO_ULL; ts.tv_nsec = rt % UN_MILIARDO_ULL; }

#define MAX_DEVICE_LEN      13 // /dev/ttyUSB0.
#define MAX_THREADNAME_LEN  42 // "srv(64)TCPRTU_SRV_101.102.103.104_65535"

enum threadStatus {NOT_STARTED = 0, RUNNING, EXITING};

// -------------------------------------------------------------------------------------------

enum ServerStatus {SRV_RUNNING0 = 0, SRV_RUNNING1, SRV_RUNNING2, SRV_RUNNING3, SRV_RUNNING4};

struct ServerStruct {
    // for serverThread
    enum FieldbusType protocol;
    uint32_t IPaddress;
    uint16_t port;
    //
    union ServerData {
        struct {
            uint16_t port;
            uint32_t baudrate;
            char parity;
            uint16_t databits;
            uint16_t stopbits;
        } serial;
        struct {
            uint32_t IPaddr;
            uint16_t port;
        } tcp_ip;
    } u;
    uint16_t silence_ms;
    uint16_t timeout_ms;
    uint16_t NodeId;
    //
    char name[MAX_THREADNAME_LEN]; // "(64)TCPRTUSRV_123.567.901.345_65535"
    enum ServerStatus status;
    pthread_t thread_id;
    enum threadStatus thread_status;
    pthread_mutex_t mutex;
    modbus_t * ctx;
    modbus_mapping_t *mb_mapping;
    uint8_t *can_buffer;
    uint16_t diagnosticAddr;
    RTIME idle_time_ns;
    RTIME busy_time_ns;
    RTIME last_time_ns;
    RTIME last_update_ns;
};

extern struct ServerStruct theServers[MAX_SERVERS];
extern uint16_t theServersNumber;

// -------------------------------------------------------------------------------------------

enum DeviceStatus {ZERO = 0, NOT_CONNECTED, CONNECTED, CONNECTED_WITH_ERRORS, DEVICE_BLACKLIST, NO_HOPE};

#define MaxLocalQueue 64
struct PLCwriteRequest {
    uint16_t Addr;
    uint16_t Number;
    uint32_t Values[MAX_VALUES];
};

struct ClientStruct {
    // for clientThread
    enum FieldbusType protocol;
    uint32_t IPaddress;
    uint16_t port;
    //
    uint16_t var_num;
    struct device_var {
        uint16_t addr;
        uint16_t active; // 1 {P,S,F,V,X}, 0/1 {H}
    } *device_vars;
    union ClientData {
        // no plc client
        struct {
            uint16_t port;
            uint32_t baudrate;
            char parity;
            uint16_t databits;
            uint16_t stopbits;
        } serial; // RTU, MECT
        struct {
            uint32_t IPaddr;
            uint16_t port;
        } tcp_ip; // TCP, TCPRTU
        struct {
            uint16_t bus;
            uint32_t baudrate;
        } can;
    } u;
    int16_t silence_ms;
    uint16_t timeout_ms;
    uint16_t max_block_size;
    //
    char name[MAX_THREADNAME_LEN]; // "(64)TCPRTUSRV_123.567.901.345_65535"
    enum DeviceStatus status;
    pthread_t thread_id;
    enum threadStatus thread_status;
    RTIME current_time_ns;
    RTIME elapsed_time_ns;
    RTIME idle_time_ns;
    RTIME busy_time_ns;
    RTIME last_time_ns;
    RTIME last_update_ns;
    uint16_t server; // for RTUSRV, TCPSRV, TCPRUSRV
    modbus_t * modbus_ctx; // for RTU, TCP, TCPRTU
    int mect_fd; // for MECT
    // local queue
    struct PLCwriteRequest PLCwriteRequests[MaxLocalQueue];
    uint16_t PLCwriteRequestNumber;
    uint16_t PLCwriteRequestGet;
    uint16_t PLCwriteRequestPut;
    uint16_t diagnosticAddr;
};

extern struct ClientStruct theDevices[MAX_DEVICES];
extern uint16_t theDevicesNumber;
extern uint16_t theTcpDevicesNumber;

// -------------------------------------------------------------------------------------------

enum NodeStatus   {NO_NODE = 0, NODE_OK, TIMEOUT, BLACKLIST, DISCONNECTED, NODE_DISABLED};

struct NodeStruct {
    uint16_t device;
    uint16_t NodeID;
    //
    enum NodeStatus status;
    int16_t retries;
    int16_t blacklist;
    uint16_t diagnosticAddr;
};

extern struct NodeStruct theNodes[MAX_NODES];
extern uint16_t theNodesNumber;

// -------------------------------------------------------------------------------------------

#define PLC_time             5390
#define PLC_timeMin          5391
#define PLC_timeMax          5392
#define PLC_timeWin          5393
#define PLC_Version          5394
#define PLC_EngineStatus     5395
#define PLC_ResetValues      5396
#define PLC_buzzerOn         5397
#define PLC_PLC_Version      5398 // UINT;3;[RW] viewable in hmi menu > info
#define PLC_HMI_Version      5399 // UINT;3;[RW] viewable in hmi menu > info

#define PLC_5400             5400 // TPLC100_01_AA/AB CH0_NETRUN
#define PLC_5401             5401 // TPLC100_01_AA/AB CH0_NETGOOD
#define PLC_5402             5402 // TPLC100_01_AA/AB CH0_NETERR
#define PLC_5403             5403 // TPLC100_01_AA/AB CH0_NETRST
#define PLC_5404             5404 // TPLC100_01_AA/AB CH0_NETDIS
#define PLC_5405             5405 // TPLC100_01_AA/AB CH0_01_NODERUN
#define PLC_5406             5406 // TPLC100_01_AA/AB CH0_01_NODEGOOD
#define PLC_5407             5407 // TPLC100_01_AA/AB CH0_01_NODEERR
#define PLC_5408             5408 // TPLC100_01_AA/AB CH0_01_NODERST
#define PLC_5409             5409 // TPLC100_01_AA/AB CH0_01_NODEDIS

#define PLC_Year             5410 // [RO] 2017
#define PLC_Month            5411 // [RO] 1..12
#define PLC_Day              5412 // [RO] 1..31
#define PLC_Hours            5413 // [RO] 0..23
#define PLC_Minutes          5414 // [RO] 0..59
#define PLC_Seconds          5415 // [RO] 0..59
#define PLC_UPTIME_s         5416 // UDINT;0;[RO] Uptime in seconds (wraps in 136 years)
#define PLC_UPTIME_cs        5417 // UDINT;0;[RO] Uptime in centiseconds = 10 ms (wraps in 497 days)
#define PLC_WATCHDOGEN       5418 // BIT;;[RW] Enable Watchdog
#define PLC_WATCHDOG_ms      5419 // UDINT;0;[RW] Reset Watchdog Timer

#define PLC_PRODUCT_ID       5420 // UDINT;0;[RO] 0x100803AC <--> TPAC1008_03_AC
#define PLC_SERIAL_NUMBER    5421 // UDINT;0;[RO] 2019014321 <--> 2019014321
#define PLC_HMI_PAGE         5422 // DINT;0;[RW] 0x100 <--> page100; -1 <--> menu; ...
#define PLC_MS_VERSION       5423 // UDINT;0;[RO] 0x03030A <--> Mect Suite 3.3.10
#define PLC_nBACKLIGHT       5424 // BYTE;0;[RW] 0..100 0=100% 100=0%
#define PLC_CPU_TEMP         5425 // INT;1;[RO] 0.0 °C

#define PLC_5430             5430

#define PLC_BEEP_VOLUME      5435 // BYTE;0[RW] when buzzerOn
#define PLC_TOUCH_VOLUME     5436 // BYTE;0[RW] when QEvent::MouseButtonPress
#define PLC_ALARM_VOLUME     5437 // BYTE;0[RW] when alarm
#define PLC_BUZZER           5438 // UDINT;0[RW] 0x44332211 up=0x11[%] on=0x22[cs] off=0x33[cs] rep=0x44[times]
#define PLC_FastIO_Ena       5439 // UDINT;0[RW] TPAC1008_03_AX=0x000000FF TPAC1005=0x0003FF01
#define PLC_FastIO_Dir       5440 // UDINT;0[RW] TPAC1008_03_AX=0x0000000F TPAC1005=0x00020000

#define PLC_FastIO_1         5441 // BIT;;[RW] GPIO 2,14 PIN  21 SSP1_DATA0  TPAC1005=T2 TPAC1008_03_AX=FastOUT_1 TPX10xx_03_x=FastIN_1
#define PLC_FastIO_2         5442 // BIT;;[RW] GPIO 0,17 PIN 131 GPMI_CE1N               TPAC1008_03_AX=FastOUT_2 TPX10xx_03_x=FastIN_2
#define PLC_FastIO_3         5443 // BIT;;[RW] GPIO 2,12 PIN  11 SSP1_SCK                TPAC1008_03_AX=FastOUT_3 TPX10xx_03_x=FastIN_3
#define PLC_FastIO_4         5444 // BIT;;[RW] GPIO 3,06 PIN  78 AUART1_CTS              TPAC1008_03_AX=FastOUT_4 TPX10xx_03_x=FastIN_4
#define PLC_FastIO_5         5445 // BIT;;[RW] GPIO 2,20 PIN   7 SSP2_SS1                TPAC1008_03_AX=FastIN_1  TPX10xx_03_x=FastOUT_1
#define PLC_FastIO_6         5446 // BIT;;[RW] GPIO 3,02 PIN  70 AUART0_CTS              TPAC1008_03_AX=FastIN_2  TPX10xx_03_x=FastOUT_2
#define PLC_FastIO_7         5447 // BIT;;[RW] GPIO 3,04 PIN  81 AUART1_RX               TPAC1008_03_AX=FastIN_3  TPX10xx_03_x=FastOUT_3
#define PLC_FastIO_8         5448 // BIT;;[RW] GPIO 3,05 PIN  65 AUART1_TX               TPAC1008_03_AX=FastIN_4  TPX10xx_03_x=FastOUT_4

#define PLC_FastIO_9         5449 // BIT;;[RW] GPIO 2,24 PIN 286 SSP3_SCK    TPAC1005=T1 TP*=PFO
#define PLC_FastIO_10        5450 // BIT;;[RW] GPIO 2,27 PIN  15 SSP3_SS0    TPAC1005=T3
#define PLC_FastIO_11        5451 // BIT;;[RW] GPIO 2,17 PIN   1 SSP2_MOSI   TPAC1005=T4 TP*=RTC:SSP2_MOSI
#define PLC_FastIO_12        5452 // BIT;;[RW] GPIO 2,18 PIN 288 SSP2_MISO   TPAC1005=T5 TP*=RTC:SSP2_MISO
#define PLC_FastIO_13        5453 // BIT;;[RW] GPIO 2,16 PIN 280 SSP2_SCK    TPAC1005=T6 TP*=RTC:SSP2_SCK
#define PLC_FastIO_14        5454 // BIT;;[RW] GPIO 2,19 PIN   4 SSP2_SS0    TPAC1005=T7 TP*=RTC:SSP2_S0
#define PLC_FastIO_15        5455 // BIT;;[RW] GPIO 2,21 PIN  18 SSP2_SS2    TPAC1005=T8 TP*=CS
#define PLC_FastIO_16        5456 // BIT;;[RW] GPIO 2,25 PIN   9 SSP3_MOSI   TPAC1005=T9 TPAC1008*=RESET_WIFI

#define PLC_FastIO_17        5457 // BIT;;[RW] GPIO 2,26 PIN   3 SSP3_MISO   TPAC1005=T10
#define PLC_FastIO_18        5458 // BIT;;[RW] GPIO 2, 9 PIN 275 SSP0_DETECT TPAC1005=GPIO_A
#define PLC_FastIO_19        5459 // BIT;;[RW] GPIO 4,20 PIN 230 JTAG_RTCK   TPAC1005=GPIO_B
#define PLC_FastIO_20        5460
#define PLC_FastIO_21        5461
#define PLC_FastIO_22        5462
#define PLC_FastIO_23        5463
#define PLC_FastIO_24        5464

#define PLC_FastIO_25        5465
#define PLC_FastIO_26        5466
#define PLC_FastIO_27        5467
#define PLC_FastIO_28        5468
#define PLC_FastIO_29        5469
#define PLC_FastIO_30        5470
#define PLC_FastIO_31        5471
#define PLC_FastIO_32        5472

// -------------------------------------------------------------------------------------------

extern int LoadXTable(void);

// -------------------------------------------------------------------------------------------

#endif // CROSSTABLE_H
