#include "crosstable.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -------------------------------------------------------------------------------------------

struct CrossTableRecord CrossTable[1 + DimCrossTable];

struct Alarms ALCrossTable[1 + DimAlarmsCT];
uint16_t lastAlarmEvent = 0;

static int newAlarmEvent(int isAlarm, uint16_t addr, char *expr, size_t len);

struct NodeStruct theNodes[MAX_NODES];
uint16_t theNodesNumber = 0;

static uint16_t tagAddr(char *tag);

// -------------------------------------------------------------------------------------------

HmiPlcBlock plcBlock;
HmiPlcBlock hmiBlock;

// -------------------------------------------------------------------------------------------

int LoadXTable(void)
{
    uint32_t addr, indx;
    int ERR = FALSE;
    FILE *xtable = NULL;

    // init tables
    for (addr = 1; addr <= DimCrossTable; ++addr) {
        CrossTable[addr].Enable = 0;
        CrossTable[addr].Plc = FALSE;
        CrossTable[addr].Tag[0] = UNKNOWN;
        CrossTable[addr].Types = 0;
        CrossTable[addr].Decimal = 0;
        CrossTable[addr].Protocol = PLC;
        CrossTable[addr].IPAddress = 0x00000000;
        CrossTable[addr].Port = 0;
        CrossTable[addr].NodeId = 0;
        CrossTable[addr].Offset = 0;
        CrossTable[addr].Block = 0;
        CrossTable[addr].BlockSize = 0;
        CrossTable[addr].Output = FALSE;
        CrossTable[addr].OldVal = 0;
        CrossTable[addr].device = 0xffff;
        CrossTable[addr].node = 0xffff;
        VAR_STATE(addr) = DATA_ERROR; // in error until we actually read it
    }
    lastAlarmEvent = 0;
    for (addr = 0; addr <= DimAlarmsCT; ++addr) {
        ALCrossTable[addr].ALType = FALSE;
        ALCrossTable[addr].ALSource[0] = '\0';
        ALCrossTable[addr].ALCompareVar[0] = '\0';
        ALCrossTable[addr].TagAddr = 0;
        ALCrossTable[addr].SourceAddr = 0;
        ALCrossTable[addr].CompareAddr = 0;
        ALCrossTable[addr].ALCompareVal.u32 = 0;
        ALCrossTable[addr].ALOperator = 0;
        ALCrossTable[addr].ALFilterTime = 0;
        ALCrossTable[addr].ALFilterCount = 0;
        ALCrossTable[addr].comparison = 0;
    }

    // open file
    fprintf(stderr, "[%s]: loading '%s' ...", __func__, CROSSTABLE_CSV);
    xtable = fopen(CROSSTABLE_CSV, "r");
    if (xtable == NULL)  {
        ERR = TRUE;
        goto exit_function;
    }

    // read loop
    for (addr = 1; addr <= DimCrossTable; ++ addr) {
        char row[1024], *p, *r;

        if (fgets(row, 1024, xtable) == NULL) {
            // no ERR = TRUE;
            continue;
        }

        // Enable {0,1,2,3}
        p = strtok_csv(row, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].Enable = atoi(p);
        // skip empty or disabled variables
        if (CrossTable[addr].Enable == 0) {
            continue;
        }

        // Plc {H,P,S,F}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        switch (p[0]) {
        case 'H':
            CrossTable[addr].Plc = Htype;
            break;
        case 'P':
            CrossTable[addr].Plc = Ptype;
            break;
        case 'S':
            CrossTable[addr].Plc = Stype;
            break;
        case 'F':
            CrossTable[addr].Plc = Ftype;
            break;
        case 'V':
            CrossTable[addr].Plc = Vtype;
            break;
        case 'X':
            CrossTable[addr].Plc = Xtype;
            break;
        default:
            ERR = TRUE;
        }
        if (ERR) {
            break;
        }

        // Tag {identifier}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        strncpy(CrossTable[addr].Tag, p, MAX_IDNAME_LEN);

        // Types {UINT, UDINT, DINT, FDCBA, ...}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        if (strcmp(p, "BIT") == 0) {
            CrossTable[addr].Types = vt_BIT;
        } else if (strcmp(p, "BYTE") == 0) {
            CrossTable[addr].Types = vt_UINT8;
        } else if (strcmp(p, "BYTE_BIT") == 0) {
            CrossTable[addr].Types = vt_BYTE_BIT;
        } else if (strcmp(p, "WORD_BIT") == 0) {
            CrossTable[addr].Types = vt_WORD_BIT;
        } else if (strcmp(p, "DWORD_BIT") == 0) {
            CrossTable[addr].Types = vt_DWORD_BIT;
        } else if (strcmp(p, "UINT") == 0) {
            CrossTable[addr].Types = vt_UINT16;
        } else if (strcmp(p, "UINTBA") == 0) {
            CrossTable[addr].Types = vt_UINT16BA;
        } else if (strcmp(p, "INT") == 0) {
            CrossTable[addr].Types = vt_INT16;
        } else if (strcmp(p, "INTBA") == 0) {
            CrossTable[addr].Types = vt_INT16BA;
        } else if (strcmp(p, "UDINT") == 0) {
            CrossTable[addr].Types = vt_UDINT;
        } else if (strcmp(p, "UDINTDCBA") == 0) {
            CrossTable[addr].Types = vt_UDINTDCBA;
        } else if (strcmp(p, "UDINTCDAB") == 0) {
            CrossTable[addr].Types = vt_UDINTCDAB;
        } else if (strcmp(p, "UDINTBADC") == 0) {
            CrossTable[addr].Types = vt_UDINTBADC;
        } else if (strcmp(p, "DINT") == 0) {
            CrossTable[addr].Types = vt_DINT;
        } else if (strcmp(p, "DINTDCBA") == 0) {
            CrossTable[addr].Types = vt_DINTDCBA;
        } else if (strcmp(p, "DINTCDAB") == 0) {
            CrossTable[addr].Types = vt_DINTCDAB;
        } else if (strcmp(p, "DINTBADC") == 0) {
            CrossTable[addr].Types = vt_DINTBADC;
        } else if (strcmp(p, "REAL") == 0) {
            CrossTable[addr].Types = vt_REAL;
        } else if (strcmp(p, "REALDCBA") == 0) {
            CrossTable[addr].Types = vt_REALDCBA;
        } else if (strcmp(p, "REALCDAB") == 0) {
            CrossTable[addr].Types = vt_REALCDAB;
        } else if (strcmp(p, "REALBADC") == 0) {
            CrossTable[addr].Types = vt_REALBADC;

        } else if (strcmp(p, "UINTAB") == 0) {
            CrossTable[addr].Types = vt_UINT16; // backward compatibility
        } else if (strcmp(p, "INTAB") == 0) {
            CrossTable[addr].Types = vt_INT16; // backward compatibility
        } else if (strcmp(p, "UDINTABCD") == 0) {
            CrossTable[addr].Types = vt_UDINT; // backward compatibility
        } else if (strcmp(p, "DINTABCD") == 0) {
            CrossTable[addr].Types = vt_DINT; // backward compatibility
        } else if (strcmp(p, "FDCBA") == 0) {
            CrossTable[addr].Types = vt_REALDCBA; // backward compatibility
        } else if (strcmp(p, "FCDAB") == 0) {
            CrossTable[addr].Types = vt_REALCDAB; // backward compatibility
        } else if (strcmp(p, "FABCD") == 0) {
            CrossTable[addr].Types = vt_REAL; // backward compatibility
        } else if (strcmp(p, "FBADC") == 0) {
            CrossTable[addr].Types = vt_REALBADC; // backward compatibility

        } else {
            if (CrossTable[addr].Enable > 0) {
                CrossTable[addr].Types = UNKNOWN;
                ERR = TRUE;
                break;
            }
        }

        // Decimal {0, 1, 2, 3, 4, ...}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].Decimal = atoi(p);

        // Protocol {"", RTU, TCP, TCPRTU}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        if (strcmp(p, "PLC") == 0) {
            CrossTable[addr].Protocol = PLC;
            VAR_STATE(addr) = DATA_OK; // PLC variables are already ok at startup
        } else if (strcmp(p, "RTU") == 0) {
            CrossTable[addr].Protocol = RTU;
        } else if (strcmp(p, "TCP") == 0) {
            CrossTable[addr].Protocol = TCP;
        } else if (strcmp(p, "TCPRTU") == 0) {
            CrossTable[addr].Protocol = TCPRTU;
        } else if (strcmp(p, "CANOPEN") == 0) {
            CrossTable[addr].Protocol = CANOPEN;
        } else if (strcmp(p, "MECT") == 0) {
            CrossTable[addr].Protocol = MECT;
        } else if (strcmp(p, "RTU_SRV") == 0) {
            CrossTable[addr].Protocol = RTU_SRV;
        } else if (strcmp(p, "TCP_SRV") == 0) {
            CrossTable[addr].Protocol = TCP_SRV;
        } else if (strcmp(p, "TCPRTU_SRV") == 0) {
            CrossTable[addr].Protocol = TCPRTU_SRV;
        } else {
            CrossTable[addr].Protocol = PLC;
            ERR = TRUE;
            break;
        }

        // IPAddress {identifier}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].IPAddress = str2ipaddr(p);

        // Port {number}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].Port = atoi(p);

        // NodeId {number}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].NodeId = atoi(p);

        // Address {number}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].Offset = atoi(p);

        // Block {number}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].Block = atoi(p);

        // NReg {number}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        CrossTable[addr].BlockSize = atoi(p);

        // Handle ([RO]||[RW]|[AL|EV SOURCE OP COND]){text}
        p = strtok_csv(NULL, ";", &r);
        if (p == NULL) {
            ERR = TRUE;
            break;
        }
        if (strncmp(p, "[RW]", 4) == 0) {
            CrossTable[addr].Output = TRUE;
        } else if (strncmp(p, "[RO]", 4) == 0) {
            CrossTable[addr].Output = FALSE;
        } else if (strncmp(p, "[AL ", 4) == 0) {
            CrossTable[addr].Output = FALSE;
            if (strlen(p) < 10 || newAlarmEvent(1, addr, &(p[3]), strlen(p) - 3)) {
                ERR = TRUE;
                break;
            }
        } else if (strncmp(p, "[EV ", 4) == 0) {
            CrossTable[addr].Output = FALSE;
            if (strlen(p) < 10 || newAlarmEvent(0, addr, &p[3], strlen(p) - 3)) {
                ERR = TRUE;
                break;
            }
        }
    }
    if (ERR) {
        goto exit_function;
    }

    // check alarms and events
    fprintf(stderr, "\n[%s]: ... alarms/events:\n", __func__);
    for (indx = 1; indx <= lastAlarmEvent; ++indx) {
        // retrieve the source variable address
        addr = tagAddr(ALCrossTable[indx].ALSource);
        if (addr == 0) {
            ERR = TRUE;
            break;
        }
        ALCrossTable[indx].SourceAddr = addr;
        CrossTable[addr].usedInAlarmsEvents = TRUE;
        fprintf(stderr, "\t%2d: %s", indx, CrossTable[ALCrossTable[indx].TagAddr].Tag);
        fprintf(stderr, " = %s", CrossTable[ALCrossTable[indx].SourceAddr].Tag);

        // which comparison?
        switch (CrossTable[addr].Types) {
        case vt_BIT:
        case vt_BYTE_BIT:
        case vt_WORD_BIT:
        case vt_DWORD_BIT:
            ALCrossTable[indx].comparison = COMP_UNSIGNED;
            fprintf(stderr, " (u)");
            break;
        case vt_INT16:
        case vt_INT16BA:
            ALCrossTable[indx].comparison = COMP_SIGNED16;
            fprintf(stderr, " (s16)");
            break;
        case vt_DINT:
        case vt_DINTDCBA:
        case vt_DINTCDAB:
        case vt_DINTBADC:
            ALCrossTable[indx].comparison = COMP_SIGNED32;
            fprintf(stderr, " (s32)");
            break;
        case vt_UINT8:
        case vt_UINT16:
        case vt_UINT16BA:
        case vt_UDINT:
        case vt_UDINTDCBA:
        case vt_UDINTCDAB:
        case vt_UDINTBADC:
            ALCrossTable[indx].comparison = COMP_UNSIGNED;
            fprintf(stderr, " (u)");
            break;
        case vt_REAL:
        case vt_REALDCBA:
        case vt_REALCDAB:
        case vt_REALBADC:
            ALCrossTable[indx].comparison = COMP_FLOATING;
            fprintf(stderr, " (f)");
            break;
        default:
            ; // FIXME: assert
        }

        switch (ALCrossTable[indx].ALOperator)  {
        case OPER_RISING    : fprintf(stderr, " RISING"); break;
        case OPER_FALLING   : fprintf(stderr, " FALLING"); break;
        case OPER_EQUAL     : fprintf(stderr, " =="); break;
        case OPER_NOT_EQUAL : fprintf(stderr, " !="); break;
        case OPER_GREATER   : fprintf(stderr, " >" ); break;
        case OPER_GREATER_EQ: fprintf(stderr, " >="); break;
        case OPER_SMALLER   : fprintf(stderr, " <" ); break;
        case OPER_SMALLER_EQ: fprintf(stderr, " <="); break;
        default             : ;
        }

        if (ALCrossTable[indx].ALOperator != OPER_FALLING
            && ALCrossTable[indx].ALOperator != OPER_RISING) {

            // if the comparison is with a variable
            if (ALCrossTable[indx].ALCompareVar[0] != 0) {
                int compatible = TRUE;

                // then retrieve the compare variable address
                addr = tagAddr(ALCrossTable[indx].ALCompareVar);
                if (addr == 0) {
                    ERR = TRUE;
                    break;
                }
                ALCrossTable[indx].CompareAddr = addr;
                CrossTable[addr].usedInAlarmsEvents = TRUE;
                fprintf(stderr, " %s", CrossTable[addr].Tag);

                // check for incompatibles types
                switch (CrossTable[ALCrossTable[indx].SourceAddr].Types) {

                case vt_BIT:
                case vt_BYTE_BIT:
                case vt_WORD_BIT:
                case vt_DWORD_BIT:
                    switch (CrossTable[ALCrossTable[indx].CompareAddr].Types) {
                    case vt_BIT:
                    case vt_BYTE_BIT:
                    case vt_WORD_BIT:
                    case vt_DWORD_BIT:
                        compatible = TRUE;
                        break;
                    case vt_INT16:
                    case vt_INT16BA:
                    case vt_DINT:
                    case vt_DINTDCBA:
                    case vt_DINTCDAB:
                    case vt_DINTBADC:
                        compatible = FALSE; // only == 0 and != 0
                        break;
                    case vt_UINT8:
                    case vt_UINT16:
                    case vt_UINT16BA:
                    case vt_UDINT:
                    case vt_UDINTDCBA:
                    case vt_UDINTCDAB:
                    case vt_UDINTBADC:
                        compatible = FALSE; // only == 0 and != 0
                        break;
                    case vt_REAL:
                    case vt_REALDCBA:
                    case vt_REALCDAB:
                    case vt_REALBADC:
                        compatible = FALSE; // only == 0 and != 0
                        break;
                    default:
                        ; // FIXME: assert
                    }
                    break;

                case vt_INT16:
                case vt_INT16BA:
                case vt_DINT:
                case vt_DINTDCBA:
                case vt_DINTCDAB:
                case vt_DINTBADC:
                    switch (CrossTable[ALCrossTable[indx].CompareAddr].Types) {
                    case vt_BIT:
                    case vt_BYTE_BIT:
                    case vt_WORD_BIT:
                    case vt_DWORD_BIT:
                        compatible = FALSE;
                        break;
                    case vt_INT16:
                    case vt_INT16BA:
                    case vt_DINT:
                    case vt_DINTDCBA:
                    case vt_DINTCDAB:
                    case vt_DINTBADC:
                        compatible = (CrossTable[ALCrossTable[indx].SourceAddr].Decimal == CrossTable[ALCrossTable[indx].CompareAddr].Decimal);
                        break;
                    case vt_UINT8:
                    case vt_UINT16:
                    case vt_UINT16BA:
                    case vt_UDINT:
                    case vt_UDINTDCBA:
                    case vt_UDINTCDAB:
                    case vt_UDINTBADC:
                        // compatible = (CrossTable[ALCrossTable[indx].SourceAddr].Decimal == CrossTable[ALCrossTable[indx].CompareAddr].Decimal);
                        compatible = FALSE;
                        break;
                    case vt_REAL:
                    case vt_REALDCBA:
                    case vt_REALCDAB:
                    case vt_REALBADC:
                        compatible = FALSE;
                        break;
                    default:
                        ; // FIXME: assert
                    }
                    break;

                case vt_UINT8:
                case vt_UINT16:
                case vt_UINT16BA:
                case vt_UDINT:
                case vt_UDINTDCBA:
                case vt_UDINTCDAB:
                case vt_UDINTBADC:
                    switch (CrossTable[ALCrossTable[indx].CompareAddr].Types) {
                    case vt_BIT:
                    case vt_BYTE_BIT:
                    case vt_WORD_BIT:
                    case vt_DWORD_BIT:
                        compatible = FALSE;
                        break;
                    case vt_INT16:
                    case vt_INT16BA:
                    case vt_DINT:
                    case vt_DINTDCBA:
                    case vt_DINTCDAB:
                    case vt_DINTBADC:
                        // compatible = (CrossTable[ALCrossTable[indx].SourceAddr].Decimal == CrossTable[ALCrossTable[indx].CompareAddr].Decimal);
                        compatible = FALSE;
                        break;
                    case vt_UINT8:
                    case vt_UINT16:
                    case vt_UINT16BA:
                    case vt_UDINT:
                    case vt_UDINTDCBA:
                    case vt_UDINTCDAB:
                    case vt_UDINTBADC:
                        compatible = (CrossTable[ALCrossTable[indx].SourceAddr].Decimal == CrossTable[ALCrossTable[indx].CompareAddr].Decimal);
                        break;
                    case vt_REAL:
                    case vt_REALDCBA:
                    case vt_REALCDAB:
                    case vt_REALBADC:
                        compatible = FALSE;
                        break;
                    default:
                        ; // FIXME: assert
                    }
                    break;

                case vt_REAL:
                case vt_REALDCBA:
                case vt_REALCDAB:
                case vt_REALBADC:
                    switch (CrossTable[ALCrossTable[indx].CompareAddr].Types) {
                    case vt_BIT:
                    case vt_BYTE_BIT:
                    case vt_WORD_BIT:
                    case vt_DWORD_BIT:
                        compatible = FALSE;
                        break;
                    case vt_INT16:
                    case vt_INT16BA:
                    case vt_DINT:
                    case vt_DINTDCBA:
                    case vt_DINTCDAB:
                    case vt_DINTBADC:
                        compatible = FALSE;
                        break;
                    case vt_UINT8:
                    case vt_UINT16:
                    case vt_UINT16BA:
                    case vt_UDINT:
                    case vt_UDINTDCBA:
                    case vt_UDINTCDAB:
                    case vt_UDINTBADC:
                        compatible = FALSE;
                        break;
                    case vt_REAL:
                    case vt_REALDCBA:
                    case vt_REALCDAB:
                    case vt_REALBADC:
                        compatible = TRUE; // no decimal test
                        break;
                    default:
                        ; // FIXME: assert
                    }
                    break;

                default:
                    ; // FIXME: assert
                }
                if (! compatible) {
                    fprintf(stderr, " [WARNING: comparison between incompatible types]");
                }

            } else {
                // the comparison is with a fixed value, now check for the vartype
                // since we saved the value as float before
                float fvalue = ALCrossTable[indx].ALCompareVal.f;
                int n;

                switch (CrossTable[addr].Types) {
                case vt_BIT:
                case vt_BYTE_BIT:
                case vt_WORD_BIT:
                case vt_DWORD_BIT:
                    if (fvalue <= 0.0) {
                        ALCrossTable[indx].ALCompareVal.u32 = 0;
                    } else if (fvalue <= 1.0) {
                        ALCrossTable[indx].ALCompareVal.u32 = 1;
                    } else {
                        ALCrossTable[indx].ALCompareVal.u32 = 2;
                    }
                    break;
                case vt_INT16:
                case vt_INT16BA:
                    for (n = 0; n < CrossTable[addr].Decimal; ++n) {
                        fvalue *= 10;
                    }
                    ALCrossTable[indx].ALCompareVal.i16 = fvalue;
                    break;
                case vt_DINT:
                case vt_DINTDCBA:
                case vt_DINTCDAB:
                case vt_DINTBADC:
                    for (n = 0; n < CrossTable[addr].Decimal; ++n) {
                        fvalue *= 10;
                    }
                    ALCrossTable[indx].ALCompareVal.i32 = fvalue;
                    break;
                case vt_UINT8:
                case vt_UINT16:
                case vt_UINT16BA:
                case vt_UDINT:
                case vt_UDINTDCBA:
                case vt_UDINTCDAB:
                case vt_UDINTBADC:
                    if (fvalue <= 0.0) {
                        fvalue = 0.0; // why check unsigned with a negative value?
                    } else {
                        for (n = 0; n < CrossTable[addr].Decimal; ++n) {
                            fvalue *= 10;
                        }
                    }
                    // NB this may overflow
                    ALCrossTable[indx].ALCompareVal.u32 = fvalue;
                    break;
                case vt_REAL:
                case vt_REALDCBA:
                case vt_REALCDAB:
                case vt_REALBADC:
                    // the value is already stored as a float, comparisons will be ok
                    break;
                default:
                    ; // FIXME: assert
                }

                switch (ALCrossTable[indx].comparison)
                {
                case COMP_UNSIGNED:
                    fprintf(stderr, " %u", ALCrossTable[indx].ALCompareVal.u32);
                    break;
                case COMP_SIGNED16:
                    fprintf(stderr, " %d", ALCrossTable[indx].ALCompareVal.i16);
                    break;
                case COMP_SIGNED32:
                    fprintf(stderr, " %d", ALCrossTable[indx].ALCompareVal.i32);
                    break;
                case COMP_FLOATING:
                    fprintf(stderr, " %f", ALCrossTable[indx].ALCompareVal.f);
                    break;
                default:
                    ;
                }
            }
        }
        fprintf(stderr, "\n");
    }

// close file
exit_function:
    if (xtable) {
        fclose(xtable);
    }
    fprintf(stderr, "\n[%s]: ... %s\n", __func__, (ERR) ? "ERROR" : "OK");
    return ERR;
}

// -------------------------------------------------------------------------------------------

static uint16_t tagAddr(char *tag)
{
    uint16_t addr;

    for (addr = 1; addr <= DimCrossTable; ++addr) {
        if (strncmp(tag, CrossTable[addr].Tag, MAX_IDNAME_LEN) == 0) {
            return addr;
        }
    }
    return 0;
}

static int newAlarmEvent(int isAlarm, uint16_t addr, char *expr, size_t len)
{
    char *p, *r;
    (void)len;

    if (lastAlarmEvent >= DimAlarmsCT) {
        return -1;
    }
    ++lastAlarmEvent;
    ALCrossTable[lastAlarmEvent].ALType = (isAlarm ? Alarm : Event);
    ALCrossTable[lastAlarmEvent].TagAddr = addr;

    p = strtok_r(expr, " ]", &r);
    if (p == NULL) {
        goto exit_error;
    }
    strncpy(ALCrossTable[lastAlarmEvent].ALSource, p, MAX_IDNAME_LEN);

    p = strtok_r(NULL, " ]", &r);
    if (p == NULL) {
        goto exit_error;
    }
    if (strncmp(p, ">=", 2) == 0) { // before ">" !!!
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_GREATER_EQ;
    } else if (strncmp(p, ">", 1) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_GREATER;
    } else if (strncmp(p, "<=", 2) == 0) { // before "<" !!!
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_SMALLER_EQ;
    } else if (strncmp(p, "<", 1) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_SMALLER;
    } else if (strncmp(p, "==", 2) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_EQUAL;
    } else if (strncmp(p, "!=", 2) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_NOT_EQUAL;
    } else if (strncmp(p, "RISING", 6) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_RISING;
    } else if (strncmp(p, "FALLING", 7) == 0) {
        ALCrossTable[lastAlarmEvent].ALOperator = OPER_FALLING;
    } else {
        goto exit_error;
    }

    if (ALCrossTable[lastAlarmEvent].ALOperator != OPER_FALLING && ALCrossTable[lastAlarmEvent].ALOperator != OPER_RISING) {
        char *s;
        float f;

        p = strtok_r(NULL, "]", &r);
        if (p == NULL) {
            goto exit_error;
        }
        f = strtof(p, &s);
        if (s == p) {
            // identifier (check later on)
            strncpy(ALCrossTable[lastAlarmEvent].ALCompareVar, p, MAX_IDNAME_LEN);
        } else {
            // number
            ALCrossTable[lastAlarmEvent].ALCompareVar[0] = 0;
            ALCrossTable[lastAlarmEvent].ALCompareVal.f = f;
        }
    }
    return 0;

exit_error:
    --lastAlarmEvent;
    return -1;
}

// -------------------------------------------------------------------------------------------

char *strtok_csv(char *string, const char *separators, char **savedptr)
{
    char *p, *s;

    if (separators == NULL || savedptr == NULL) {
        return NULL;
    }
    if (string == NULL) {
        p = *savedptr;
        if (p == NULL) {
            return NULL;
        }
    } else {
        p = string;
    }

    s = strstr(p, separators);
    if (s == NULL) {
        *savedptr = NULL;
        return p;
    }
    *s = 0;
    *savedptr = s + 1;

    // remove spaces at head
    while (p < s && isspace(*p)) {
        ++p;
    }
    // remove spaces at tail
    --s;
    while (s > p && isspace(*s)) {
        *s = 0;
        --s;
    }
    return p;
}

// -------------------------------------------------------------------------------------------

uint32_t str2ipaddr(const char *str)
{
    uint32_t ipaddr = 0;
    char buffer[MAX_IPADDR_LEN];
    char *s, *r;
    int i;

    strncpy(buffer, str, MAX_IPADDR_LEN);
    buffer[16] = 0;

    s = strtok_csv(buffer, ".", &r);
    for (i = 3; i >= 0; --i) {
        if (s == NULL) {
            return 0x00000000;
        }
        ipaddr += (strtoul(s, NULL, 10) % 255) << (i * 8);
        s = strtok_csv(NULL, ".", &r);
    }
    return ipaddr;
}

char *ipaddr2str(uint32_t ipaddr, char *buffer)
{
    if (buffer != NULL) {
        register uint8_t a, b, c, d;
        a = (ipaddr & 0xFF000000) >> 24;
        b = (ipaddr & 0x00FF0000) >> 16;
        c = (ipaddr & 0x0000FF00) >> 8;
        d = (ipaddr & 0x000000FF);
        sprintf(buffer, "%u.%u.%u.%u", a, b, c, d);
    }
    return buffer;
}

// -------------------------------------------------------------------------------------------

