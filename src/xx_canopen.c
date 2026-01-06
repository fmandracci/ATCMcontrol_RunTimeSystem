/*
 * Filename: CANopen.c
 */

/* ----  Local Defines:   ----------------------------------------------------- */

/* ----  Includes:	 ---------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "xx_canopen.h"

#define NO_THREAD 0xFFFFFFFFL
#define STACK_RATE_MS   10
#define MESSAGE_RATE_MS 1

#define CAN_CHANNELS 2
#define MAX_NODES 32

struct statusID {
    uint16_t runningID;
    uint16_t goodID;
    uint16_t errorID;
    uint16_t resetID;
    uint16_t disableID;
};

uint8_t CANopenChannels()
{
    return CAN_CHANNELS;
}

void CANopenStart(uint8_t channel)
{
    if (channel > CAN_CHANNELS) {
        fprintf(stderr, "CANopenStart, ERROR: wrong channel %u\n", channel);
        return;
    }
    // start
    /***************************************
     *           CANopen start             *
     ***************************************/
}

void CANopenList(uint8_t channel)
{
    if (channel >= CAN_CHANNELS) {
        fprintf(stderr, "CANopenStart, ERROR: wrong channel %u\n", channel);
        return;
    }
    // list
    /***************************************
     *           CANopen list              *
     ***************************************/
}

int CANopenConfigured(uint8_t channel)
{
    if (channel > CAN_CHANNELS) {
        fprintf(stderr, "CANopenConfigured, ERROR: wrong channel %u\n", channel);
        return 0;
    }
    /***************************************
     *                 stub                *
     ***************************************/
    return 0;
}

void CANopenStop(uint8_t channel)
{
    if (channel > CAN_CHANNELS) {
        fprintf(stderr, "CANopenStart, ERROR: wrong channel %u\n", channel);
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

uint16_t CANopenGetVarIndex(uint8_t channel, char *name)
{
    uint16_t retval = 0;

    if (channel > CAN_CHANNELS || name == NULL) {
        return retval;
    }
    /***************************************
     *                 stub                *
     ***************************************/
    return retval;
}

void CANopenGetChannelStatus(uint8_t channel, struct CANopenStatus *status)
{
    if (channel > CAN_CHANNELS || status == NULL) {
        if (status) {
            status->running = 0;
            status->good = 0;
            status->error = 0xffffffff;
        }
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

void CANopenGetNodeStatus(uint8_t channel, uint8_t node, struct CANopenStatus *status)
{
    if (channel > CAN_CHANNELS || node > MAX_NODES || status == NULL) {
        if (status) {
            status->running = 0;
            status->good = 0;
            status->error = 0xffffffff;
        }
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

void CANopenResetChannel(uint8_t channel)
{
    if (channel > CAN_CHANNELS) {
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

void CANopenResetNode(uint8_t channel, uint8_t node)
{
    if (channel > CAN_CHANNELS || node > MAX_NODES) {
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

void CANopenDisableChannel(uint8_t channel)
{
    if (channel > CAN_CHANNELS) {
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

void CANopenDisableNode(uint8_t channel, uint8_t node)
{
    if (channel > CAN_CHANNELS || node > MAX_NODES) {
        return;
    }
    /***************************************
     *                 stub                *
     ***************************************/
}

int CANopenReadPDOBit(uint8_t channel, uint16_t address, uint8_t *pvalue)
{
    (void)channel;
    (void)address;
    (void)pvalue;

    /***************************************
     *                 stub                *
     ***************************************/
    return 0;
}

int CANopenReadPDOByte(uint8_t channel, uint16_t address, uint8_t *pvalue)
{
    (void)channel;
    (void)address;
    (void)pvalue;

    /***************************************
     *                 stub                *
     ***************************************/
    return 0;
}

int CANopenReadPDOWord(uint8_t channel, uint16_t address, uint16_t *pvalue)
{
    (void)channel;
    (void)address;
    (void)pvalue;

    /***************************************
     *                 stub                *
     ***************************************/
    return 0;
}

int CANopenReadPDODword(uint8_t channel, uint16_t address, uint32_t *pvalue)
{
    (void)channel;
    (void)address;
    (void)pvalue;

    /***************************************
     *                 stub                *
     ***************************************/
    return 0;
}

int CANopenWritePDOBit(uint8_t channel, uint16_t address, uint8_t value)
{
    (void)channel;
    (void)address;
    (void)value;

    /***************************************
     *                 stub                *
     ***************************************/
	return 0;
}

int CANopenWritePDOByte(uint8_t channel, uint16_t address, uint8_t value)
{
    (void)channel;
    (void)address;
    (void)value;

    /***************************************
     *                 stub                *
     ***************************************/
	return 0;
}

int CANopenWritePDOWord(uint8_t channel, uint16_t address, uint16_t value)
{
    (void)channel;
    (void)address;
    (void)value;

    /***************************************
     *                 stub                *
     ***************************************/
	return 0;
}

int CANopenWritePDODword(uint8_t channel, uint16_t address, uint32_t value)
{
    (void)channel;
    (void)address;
    (void)value;

    /***************************************
     *                 stub                *
     ***************************************/
	return 0;
}
