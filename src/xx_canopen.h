#ifndef XX_CANOPEN_H
#define XX_CANOPEN_H

#include <stdint.h>

struct CANopenStatus {
    uint8_t running;
    uint8_t good;
    uint32_t error;
};

uint8_t CANopenChannels();

void CANopenStart(uint8_t channel);
int CANopenConfigured(uint8_t channel);
void CANopenStop(uint8_t channel);
void CANopenList(uint8_t channel);

uint16_t CANopenGetVarIndex(uint8_t channel, char *name);
void CANopenGetChannelStatus(uint8_t channel, struct CANopenStatus *status);
void CANopenGetNodeStatus(uint8_t channel, uint8_t node, struct CANopenStatus *status);
void CANopenResetChannel(uint8_t channel);
void CANopenResetNode(uint8_t channel, uint8_t node);
void CANopenDisableChannel(uint8_t channel);
void CANopenDisableNode(uint8_t channel, uint8_t node);

int CANopenReadPDOBit(uint8_t channel, uint16_t address, uint8_t *pvalue);
int CANopenReadPDOByte(uint8_t channel, uint16_t address, uint8_t *pvalue);
int CANopenReadPDOWord(uint8_t channel, uint16_t address, uint16_t *pvalue);
int CANopenReadPDODword(uint8_t channel, uint16_t address, uint32_t *pvalue);

int CANopenWritePDOBit(uint8_t channel, uint16_t address, uint8_t value);
int CANopenWritePDOByte(uint8_t channel, uint16_t address, uint8_t value);
int CANopenWritePDOWord(uint8_t channel, uint16_t address, uint16_t value);
int CANopenWritePDODword(uint8_t channel, uint16_t address, uint32_t value);

#endif // XX_CANOPEN_H

