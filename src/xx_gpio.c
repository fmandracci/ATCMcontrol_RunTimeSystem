#include "xx_gpio.h"

#include "../main.h"

#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
#include <stdio.h>
#include <gpiod.h>
#include <string.h>
#include <errno.h>

#else
#error unknown platform

#endif

/* ----  Local Defines:   ----------------------------------------------------- */

#if defined(KIT_IMX28)
#define XX_GPIO_BASE        0x80000000
#define XX_GPIO_SIZE        0x00100000
#define XX_GPIO_SET_OFFSET  0x0004
#define XX_GPIO_CLR_OFFSET  0x0008

#define XX_PINCTL_BASE      0x00018000 // 0x80018000

#define XX_RTCCTL_BASE      0x00056000 // 0x80056000
#define XX_WATCHDOGEN_OFFS  0x00000000 // 0x80056000 HW_RTC_CTRL
#define XX_WATCHDOGEN_MASK  0x00000010 //            WATCHDOGEN
#define XX_WATCHDOGms_OFFS  0x00000050 // 0x80056050 HW_RTC_WATCHDOG

#define XX_PWMCTL_BASE      0x00064000 // 0x80064000
#define XX_PMW3_ENABLE_OFFS 0x00000000 // 0x80064000 HW_PWM_CTRL
#define XX_PMW3_ENABLE_MASK 0x00000008 //            PMW3_ENABLE
#define XX_PWM3_ACTIVE_OFFS 0x00000070 // 0x80064070 HW_PWM_ACTIVE3
#define XX_PWM3_PERIOD_OFFS 0x00000080 // 0x80064080 HW_PWM_ACTIVE3

#elif defined(KIT_RPI4)
#define GPIO_FastIO_1      0 // input
#define GPIO_FastIO_2      1 // input
#define GPIO_FastIO_3      2 // input
#define GPIO_FastIO_4      3 // input
#define GPIO_USR_INPUTS    4
#define GPIO_FastIO_Input_v(n) (n) // libgio bulk value index
#define GPIO_FastIO_Input_ok(n) ((n) < GPIO_USR_INPUTS) // NB: unsigned --> no test >=0

#define GPIO_FastIO_5      4 // output
#define GPIO_FastIO_6      5 // output
#define GPIO_FastIO_7      6 // output
#define GPIO_FastIO_8      7 // output
#define GPIO_USR_OUTPUTS   4
#define GPIO_FastIO_Output_v(n) (n - GPIO_USR_INPUTS) // libgio bulk value index
#define GPIO_FastIO_Output_ok(n) (GPIO_USR_INPUTS <= (n) && (n) < (GPIO_USR_INPUTS + GPIO_USR_OUTPUTS))

#define GPIO_FastIO_ok(n) ((n) < (GPIO_USR_INPUTS + GPIO_USR_OUTPUTS)) // NB: unsigned --> no test >=0

#define GPIO_uC_IRQ       23 // input, interrupt (vedi mcp23008 in dtoverlay)
#define GPIO_SYS_INPUTS    1
#define GPIO_uC_IRQ_v      0 // libgio bulk value index

#define GPIO_BUZZER       12 // output
#define GPIO_nBACKLIGHT   13 // output
#define GPIO_uC_nRESET    16 // output
#define GPIO_SYS_TAC      22 // output
#define GPIO_SYS_TIC      24 // output
#define GPIO_SYS_OUTPUTS   5
#define GPIO_BUZZER_v      0 // libgio bulk value index
#define GPIO_nBACKLIGHT_v  1 // libgio bulk value index
#define GPIO_uC_nRESET_v   2 // libgio bulk value index
#define GPIO_SYS_TAC_v     3 // libgio bulk value index
#define GPIO_SYS_TIC_v     4 // libgio bulk value index

#else
#error unknown platform

#endif

/* ----  Local Variables:	 -------------------------------------------------- */

#if defined(KIT_IMX28)
static void *xx_base_ptr = NULL;
static int xx_fd = -1;
static struct {
    unsigned offset;
    unsigned value;
} xx_gpio_enabler[] = {

    // PINCTRL register in "i.MX28 Applications Processor Reference Manual", # MCIMX28RM, rev. 1, 2010, page 688

    // XX_GPIO( 0) FastIO_1 bank 2, pin 14 (pin 21, SSP1_DATA0)
    { 0x0144, 0x30000000 },		//MUXSEL4 SET(GPIO), page 705
    { 0x0394, 0x04000000 },		//DRIVE9 SET(3.3V), page 756
    { 0x0398, 0x03000000 },		//DRIVE9 CLR(4mA), page 756
    { 0x0628, 0x00004000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00004000 },		//DOUT2 CLR, page 801

    // XX_GPIO( 1) FastIO_2 bank 0, pin 17 (pin 131, GPMI_CE1N)
    { 0x0114, 0x0000000c },		//MUXSEL1 SET(GPIO), page 696
    { 0x0324, 0x00000040 },		//DRIVE2 SET(3.3V), page 734
    { 0x0328, 0x00000030 },		//DRIVE2 CLR(4mA), page 734
    { 0x0608, 0x00020000 },		//PULL0 CLR(no), page 785
    { 0x0708, 0x00020000 },		//DOUT0 CLR, page 800

    // XX_GPIO( 2) FastIO_3 bank 2, pin 12 (pin 11, SSP1_SCK)
    { 0x0144, 0x03000000 },		//MUXSEL4 SET(GPIO), page 705
    { 0x0394, 0x00040000 },		//DRIVE9 SET(3.3V), page 756
    { 0x0398, 0x00030000 },		//DRIVE9 CLR(4mA), page 756
    { 0x0628, 0x00001000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00001000 },		//DOUT2 CLR, page 801

    // XX_GPIO( 3) FastIO_4 bank 3, pin 6 (pin 78, AUART1_CTS)
    { 0x0164, 0x00003000 },		//MUXSEL6 SET(GPIO), page 710
    { 0x03c4, 0x04000000 },		//DRIVE12 SET(3.3V), page 764
    { 0x03c8, 0x03000000 },		//DRIVEx CLR(4mA), page 764
    { 0x0638, 0x00000040 },		//PULL3 CLR(no), page 791
    { 0x0738, 0x00000040 },		//DOUT3 CLR, page 802

    // XX_GPIO( 4) FastIO_5 bank 2, pin 20 (pin 7, SSP2_SS1)
    { 0x0154, 0x00000300 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00040000 },		//DRIVE10 SET(3.3V), page 760
    { 0x03a8, 0x00030000 },		//DRIVE10 CLR(4mA), page 760
    { 0x0628, 0x00100000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00100000 },		//DOUT2 CLR, page 801

    // XX_GPIO( 5) FastIO_6 bank 3, pin 2 (pin 70, AUART0_CTS)
    { 0x0164, 0x00000030 },		//MUXSEL6 SET(GPIO), page 711
    { 0x03c4, 0x00000400 },		//DRIVE12 SET(3.3V), page 764
    { 0x03c8, 0x00000300 },		//DRIVE12 CLR(4mA), page 764
    { 0x0638, 0x00000004 },		//PULL3 CLR(no), page 791
    { 0x0738, 0x00000004 },		//DOUT3 CLR, page 802

    // XX_GPIO( 6) FastIO_7 bank 3, pin 4 (pin 81, AUART1_RX)
    { 0x0164, 0x00000300 },		//MUXSEL6 SET(GPIO), page 710
    { 0x03c4, 0x00040000 },		//DRIVE12 SET(3.3V), page 764
    { 0x03c8, 0x00030000 },		//DRIVE12 CLR(4mA), page 764
#if 0
    { 0x0638, 0x00000010 },		//PULL3 CLR(no), page 791
#else
    { 0x0634, 0x00000010 },		//PULL3 SET, page 791
#endif
    { 0x0738, 0x00000010 },		//DOUT3 CLR, page 802

    // XX_GPIO( 7) FastIO_8 bank 3, pin 5 (pin 65, AUART1_TX)
    { 0x0164, 0x00000c00 },		//MUXSEL6 SET(GPIO), page 710
    { 0x03c4, 0x00400000 },		//DRIVE12 SET(3.3V), page 764
    { 0x03c8, 0x00300000 },		//DRIVE12 CLR(4mA), page 764
#if 0
    { 0x0638, 0x00000020 },		//PULL3 CLR(no), page 791
#else
    { 0x0634, 0x00000020 },		//PULL3 SET, page 791
#endif
    { 0x0738, 0x00000020 },		//DOUT3 CLR, page 802

    // XX_GPIO( 8) FastIO_9 bank 2, pin 24 (pin 286, SSP3_SCK)
    { 0x0154, 0x00030000 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03b4, 0x00000004 },		//DRIVE11 SET(3.3V), page 762
    { 0x03b8, 0x00000003 },		//DRIVE11 CLR(4mA), page 762
    { 0x0628, 0x01000000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x01000000 },		//DOUT2 CLR, page 801

    // XX_GPIO( 9) FastIO_10 bank 2, pin 27 (pin 15, SSP3_SS0)
    { 0x0154, 0x00300000 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03b4, 0x00004000 },		//DRIVE11 SET(3.3V), page 762
    { 0x03b8, 0x00003000 },		//DRIVE11 CLR(4mA), page 762
    { 0x0628, 0x08000000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x08000000 },		//DOUT2 CLR, page 801

    // XX_GPIO(10) FastIO_11 bank 2, pin 17 (pin 1, SSP2_MOSI)
    { 0x0154, 0x0000000c },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00000040 },		//DRIVE10 SET(3.3V), page 759
    { 0x03a8, 0x00000030 },		//DRIVE10 CLR(4mA), page 759
    { 0x0628, 0x00020000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00020000 },		//DOUT2 CLR, page 801

    // XX_GPIO(11) FastIO_12 bank 2, pin 18 (pin 288, SSP2_MISO)
    { 0x0154, 0x00000030 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00000400 },		//DRIVE10 SET(3.3V), page 759
    { 0x03a8, 0x00000300 },		//DRIVE10 CLR(4mA), page 759
    { 0x0628, 0x00040000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00040000 },		//DOUT2 CLR, page 801

    // XX_GPIO(12) FastIO_13 bank 2, pin 16 (pin 280, SSP2_SCK)
    { 0x0154, 0x00000003 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00000004 },		//DRIVE10 SET(3.3V), page 759
    { 0x03a8, 0x00000003 },		//DRIVE10 CLR(4mA), page 759
    { 0x0628, 0x00010000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00010000 },		//DOUT2 CLR, page 801

    // XX_GPIO(13) FastIO_14 bank 2, pin 19 (pin 4, SSP2_SS0)
    { 0x0154, 0x000000c0 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00004000 },		//DRIVE10 SET(3.3V), page 759
    { 0x03a8, 0x00003000 },		//DRIVE10 CLR(4mA), page 759
    { 0x0628, 0x00080000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00080000 },		//DOUT2 CLR, page 801

    // XX_GPIO(14) FastIO_15 bank 2, pin 21 (pin 18, SSP2_SS2)
    { 0x0154, 0x00000c00 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03a4, 0x00040000 },		//DRIVE10 SET(3.3V), page 759
    { 0x03a8, 0x00030000 },		//DRIVE10 CLR(4mA), page 759
    { 0x0628, 0x00200000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00200000 },		//DOUT2 CLR, page 801

    // XX_GPIO(15) FastIO_16 bank 2, pin 25 (pin 9, SSP3_MOSI)
    { 0x0154, 0x000c0000 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03b4, 0x00000040 },		//DRIVE11 SET(3.3V), page 762
    { 0x03b8, 0x00000030 },		//DRIVE11 CLR(4mA), page 762
    { 0x0628, 0x02000000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x02000000 },		//DOUT2 CLR, page 801

    // XX_GPIO(16) FastIO_17 bank 2, pin 26 (pin 3, SSP3_MISO)
    { 0x0154, 0x00300000 },		//MUXSEL5 SET(GPIO), page 708
    { 0x03b4, 0x00000400 },		//DRIVE11 SET(3.3V), page 762
    { 0x03b8, 0x00000300 },		//DRIVE11 CLR(4mA), page 762
    { 0x0628, 0x04000000 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x04000000 },		//DOUT2 CLR, page 801

    // XX_GPIO(17) FastIO_18 bank 2, pin 9 (pin 275, SSP0_DETECT)
    { 0x0144, 0x000c0000 },		//MUXSEL4 SET(GPIO), page 705
    { 0x0394, 0x00000040 },		//DRIVE9 SET(3.3V), page 756
    { 0x0398, 0x00000030 },		//DRIVE9 CLR(4mA), page 756
    { 0x0628, 0x00000200 },		//PULL2 CLR(no), page 790
    { 0x0728, 0x00000200 },		//DOUT2 CLR, page 801

    // THE END
    { 0xffff, 0xffffffff }
};

static struct {
    unsigned offset;
    unsigned mask;
} xx_gpio_doe[] = {

    // PINCTRL register in "i.MX28 Applications Processor Reference Manual", # MCIMX28RM, rev. 1, 2010, page 688

    // XX_GPIO( 0) FastIO_1 bank 2, pin 14 (pin 21, SSP1_DATA0)
    { 0x0b20, 0x00004000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO( 1) FastIO_2 bank 0, pin 17 (pin 131, GPMI_CE1N)
    { 0x0b00, 0x00020000 },		//DOEx SET/CLR(en.), page 810

    // XX_GPIO( 2) FastIO_3 bank 2, pin 12 (pin 11, SSP1_SCK)
    { 0x0b20, 0x00001000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO( 3) FastIO_4 bank 3, pin 6 (pin 78, AUART1_CTS)
    { 0x0b30, 0x00000040 },		//DOE3 SET/CLR(en.), page 810

    // XX_GPIO( 4) FastIO_5 bank 2, pin 20 (pin 7, SSP2_SS1)
    { 0x0b20, 0x00100000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO( 5) FastIO_6 bank 3, pin 2 (pin 70, AUART0_CTS)
    { 0x0b30, 0x00000004 },		//DOE3 SET/CLR(en.), page 811

    // XX_GPIO( 6) FastIO_7 bank 3, pin 4 (pin 81, AUART1_RX)
    { 0x0b30, 0x00000010 },		//DOE3 SET/CLR(en.), page 810

    // XX_GPIO( 7) FastIO_8 bank 3, pin 5 (pin 65, AUART1_TX)
    { 0x0b30, 0x00000020 },		//DOE3 SET/CLR(en.), page 810

    // XX_GPIO( 8) FastIO_9 bank 2, pin 24 (pin 286, SSP3_SCK)
    { 0x0b20, 0x01000000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO( 9) FastIO_10 bank 2, pin 27 (pin 15, SSP3_SS0)
    { 0x0b20, 0x08000000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(10) FastIO_11 bank 2, pin 17 (pin 1, SSP2_MOSI)
    { 0x0b20, 0x00020000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(11) FastIO_12 bank 2, pin 18 (pin 288, SSP2_MISO)
    { 0x0b20, 0x00040000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(12) FastIO_13 bank 2, pin 16 (pin 280, SSP2_SCK)
    { 0x0b20, 0x00010000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(13) FastIO_14 bank 2, pin 19 (pin 4, SSP2_SS0)
    { 0x0b20, 0x00080000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(14) FastIO_15 bank 2, pin 21 (pin 18, SSP2_SS2)
    { 0x0b20, 0x00200000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(15) FastIO_16 bank 2, pin 25 (pin 9, SSP3_MOSI)
    { 0x0b20, 0x02000000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(16) FastIO_17 bank 2, pin 26 (pin 3, SSP3_MISO)
    { 0x0b20, 0x04000000 },		//DOE2 SET/CLR(en.), page 810

    // XX_GPIO(17) FastIO_18 bank 2, pin 9 (pin 275, SSP0_DETECT)
    { 0x0b20, 0x00000200 },		//DOE2 SET/CLR(en.), page 810

    // THE END
    { 0xffff, 0xffffffff }
};

static struct {
    unsigned offset;
    unsigned mask;
} xx_gpio_dout[] = {

    // PINCTRL register in "i.MX28 Applications Processor Reference Manual", # MCIMX28RM, rev. 1, 2010, page 688

    // XX_GPIO( 0) FastIO_1 bank 2, pin 14 (pin 21, SSP1_DATA0)
    { 0x0720, 0x00004000 },		//DOUT2, page 801

    // XX_GPIO( 1) FastIO_2 bank 0, pin 17 (pin 131, GPMI_CE1N)
    { 0x0700, 0x00020000 },		//DOUT0, page 800

    // XX_GPIO( 2) FastIO_3 bank 2, pin 12 (pin 11, SSP1_SCK)
    { 0x0720, 0x00001000 },		//DOUT2, page 801

    // XX_GPIO( 3) FastIO_4 bank 3, pin 6 (pin 78, AUART1_CTS)
    { 0x0730, 0x00000040 },		//DOUT3, page 802

    // XX_GPIO( 4) FastIO_5 bank 2, pin 20 (pin 7, SSP2_SS1)
    { 0x0720, 0x00100000 },		//DOUT2, page 801

    // XX_GPIO( 5) FastIO_6 bank 3, pin 2 (pin 70, AUART0_CTS)
    { 0x0730, 0x00000004 },		//DOUT3, page 802

    // XX_GPIO( 6) FastIO_7 bank 3, pin 4 (pin 81, AUART1_RX)
    { 0x0730, 0x00000010 },		//DOUT3, page 802

    // XX_GPIO( 7) FastIO_8 bank 3, pin 5 (pin 65, AUART1_TX)
    { 0x0730, 0x00000020 },		//DOUT3, page 802

    // XX_GPIO( 8) FastIO_9 bank 2, pin 24 (pin 286, SSP3_SCK)
    { 0x0720, 0x01000000 },		//DOUT2, page 801

    // XX_GPIO( 9) FastIO_10 bank 2, pin 27 (pin 15, SSP3_SS0)
    { 0x0720, 0x08000000 },		//DOUT2, page 801

    // XX_GPIO(10) FastIO_11 bank 2, pin 17 (pin 1, SSP2_MOSI)
    { 0x0720, 0x00020000 },		//DOUT2, page 801

    // XX_GPIO(11) FastIO_12 bank 2, pin 18 (pin 288, SSP2_MISO)
    { 0x0720, 0x00040000 },		//DOUT2, page 801

    // XX_GPIO(12) FastIO_13 bank 2, pin 16 (pin 280, SSP2_SCK)
    { 0x0720, 0x00010000 },		//DOUT2, page 801

    // XX_GPIO(13) FastIO_14 bank 2, pin 19 (pin 4, SSP2_SS0)
    { 0x0720, 0x00080000 },		//DOUT2, page 801

    // XX_GPIO(14) FastIO_15 bank 2, pin 21 (pin 18, SSP2_SS2)
    { 0x0720, 0x00200000 },		//DOUT2, page 801

    // XX_GPIO(15) FastIO_16 bank 2, pin 25 (pin 9, SSP3_MOSI)
    { 0x0720, 0x02000000 },		//DOUT2, page 801

    // XX_GPIO(16) FastIO_17 bank 2, pin 26 (pin 3, SSP3_MISO)
    { 0x0720, 0x04000000 },		//DOUT2, page 801

    // XX_GPIO(17) FastIO_18 bank 2, pin 9 (pin 275, SSP0_DETECT)
    { 0x0720, 0x00000200 },		//DOUT2, page 801

    // THE END
    { 0xffff, 0xffffffff }
};

static struct {
    unsigned offset;
    unsigned mask;
} xx_gpio_din[] = {

    // PINCTRL register in "i.MX28 Applications Processor Reference Manual", # MCIMX28RM, rev. 1, 2010, page 688

    // XX_GPIO( 0) FastIO_1 bank 2, pin 14 (pin 21, SSP1_DATA0)
    { 0x0920, 0x00004000 },		//DIN2, page 806

    // XX_GPIO( 1) FastIO_2 bank 0, pin 17 (pin 131, GPMI_CE1N)
    { 0x0900, 0x00020000 },		//DIN0, page 804

    // XX_GPIO( 2) FastIO_3 bank 2, pin 12 (pin 11, SSP1_SCK)
    { 0x0920, 0x00001000 },		//DIN2, page 806

    // XX_GPIO( 3) FastIO_4 bank 3, pin 6 (pin 78, AUART1_CTS)
    { 0x0930, 0x00000040 },		//DIN3, page 802

    // XX_GPIO( 4) FastIO_5 bank 2, pin 20 (pin 7, SSP2_SS1)
    { 0x0920, 0x00100000 },		//DIN2, page 806

    // XX_GPIO( 5) FastIO_6 bank 3, pin 2 (pin 70, AUART0_CTS)
    { 0x0930, 0x00000004 },		//DIN3, page 806

    // XX_GPIO( 6) FastIO_7 bank 3, pin 4 (pin 81, AUART1_RX)
    { 0x0930, 0x00000010 },		//DIN3, page 806

    // XX_GPIO( 7) FastIO_8 bank 3, pin 5 (pin 65, AUART1_TX)
    { 0x0930, 0x00000020 },		//DIN3, page 806

    // XX_GPIO( 8) FastIO_9 bank 2, pin 24 (pin 286, SSP3_SCK)
    { 0x0920, 0x01000000 },		//DIN2, page 806

    // XX_GPIO( 9) FastIO_10 bank 2, pin 27 (pin 15, SSP3_SS0)
    { 0x0920, 0x08000000 },		//DIN2, page 806

    // XX_GPIO(10) FastIO_11 bank 2, pin 17 (pin 1, SSP2_MOSI)
    { 0x0920, 0x00020000 },		//DIN2, page 806

    // XX_GPIO(11) FastIO_12 bank 2, pin 18 (pin 288, SSP2_MISO)
    { 0x0920, 0x00040000 },		//DIN2, page 806

    // XX_GPIO(12) FastIO_13 bank 2, pin 16 (pin 280, SSP2_SCK)
    { 0x0920, 0x00010000 },		//DIN2, page 806

    // XX_GPIO(13) FastIO_14 bank 2, pin 19 (pin 4, SSP2_SS0)
    { 0x0920, 0x00080000 },		//DIN2, page 806

    // XX_GPIO(14) FastIO_15 bank 2, pin 21 (pin 18, SSP2_SS2)
    { 0x0920, 0x00200000 },		//DIN2, page 806

    // XX_GPIO(15) FastIO_16 bank 2, pin 25 (pin 9, SSP3_MOSI)
    { 0x0920, 0x02000000 },		//DIN2, page 806

    // XX_GPIO(16) FastIO_17 bank 2, pin 26 (pin 3, SSP3_MISO)
    { 0x0920, 0x04000000 },		//DIN2, page 806

    // XX_GPIO(17) FastIO_18 bank 2, pin 9 (pin 275, SSP0_DETECT)
    { 0x0920, 0x00000200 },		//DIN2, page 806

    // THE END
    { 0xffff, 0xffffffff }
};

#elif defined(KIT_RPI4)

static struct gpiod_chip *usrGpioChip;
static struct gpiod_line_request *usrInLineRequest;
static struct gpiod_edge_event_buffer *usrInEdgeEventBuffer;
static struct gpiod_line_request *usrOutLineRequest;

static struct gpiod_chip *sysGpioChip;
static struct gpiod_line_request *sysOutLineRequest;

static unsigned int usrInOffsets[GPIO_USR_INPUTS]   = { GPIO_FastIO_1, GPIO_FastIO_2, GPIO_FastIO_3, GPIO_FastIO_4};
static int          usrInValues[GPIO_USR_INPUTS]    = {            -1,            -1,            -1,            -1};

static unsigned int usrOutOffsets[GPIO_USR_OUTPUTS] = { GPIO_FastIO_5, GPIO_FastIO_6, GPIO_FastIO_7, GPIO_FastIO_8};
static int          usrOutValues[GPIO_USR_OUTPUTS]  = {             0,             0,             0,             0};

static unsigned int sysOutOffsets[GPIO_SYS_OUTPUTS] = { GPIO_BUZZER, GPIO_nBACKLIGHT, GPIO_uC_nRESET, GPIO_SYS_TAC, GPIO_SYS_TIC};
static int          sysOutValues[GPIO_SYS_OUTPUTS]  = {           0,               0,              1,            0,            0}; // NB: GPIO_uC_nRESET=1

#else
#error unknown platform

#endif

/* ----  Implementations:	--------------------------------------------------- */

unsigned xx_gpio_enabled;
unsigned xx_gpio_output ;
unsigned xx_gpio_input  ;

void xx_gpio_init(void)
{
    const char *application_name = APPLICATION_NAME;
    xx_gpio_enabled = 0;
    xx_gpio_output  = 0;
    xx_gpio_input   = 0;

#if defined(KIT_IMX28)
    xx_base_ptr = NULL;
    xx_fd = open("/dev/mem", O_RDWR, 0);
    if (xx_fd <= 0)
        return;
    xx_base_ptr = mmap(NULL, XX_GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, xx_fd, XX_GPIO_BASE);

#elif defined(KIT_RPI4)
    struct gpiod_line_settings *usrInSettings = NULL;
    struct gpiod_line_settings *usrOutSettings = NULL;
    struct gpiod_line_settings *sysOutSettings = NULL;

    struct gpiod_line_config *usrInLineConfig = NULL;
    struct gpiod_line_config *usrOutLineConfig = NULL;
    struct gpiod_line_config *sysOutLineConfig = NULL;

    struct gpiod_request_config *usrInRequestConfig = NULL;
    struct gpiod_request_config *usrOutRequestConfig = NULL;
    struct gpiod_request_config *sysOutRequestConfig = NULL;

    usrGpioChip = gpiod_chip_open("/dev/gpiochip2");
    if (! usrGpioChip)  {
        fprintf(stderr, "[%s] error in gpiod_chip_open usr: %s\n", __func__, strerror(errno));
        goto exit_failure;
    }
    sysGpioChip = gpiod_chip_open("/dev/gpiochip0");
    if (! sysGpioChip)  {
        fprintf(stderr, "[%s] error in gpiod_chip_open sys: %s\n", __func__, strerror(errno));
        goto exit_failure;
    }

    if (usrGpioChip) {
        // --------- usr inputs
        usrInSettings = gpiod_line_settings_new();
        if (! usrInSettings) {
            fprintf(stderr, "[%s] error creating usrInSettings: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        if (gpiod_line_settings_set_direction(usrInSettings, GPIOD_LINE_DIRECTION_INPUT)) {
            fprintf(stderr, "[%s] error in setting usr input lines direction: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
#ifdef XX_GPIO_INTERRUPT
        if (gpiod_line_settings_set_edge_detection(usrInSettings, GPIOD_LINE_EDGE_BOTH)) {
            fprintf(stderr, "[%s] error in requesting usr input lines events: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        usrInEdgeEventBuffer = gpiod_edge_event_buffer_new(GPIO_USR_INPUTS);
        if (! usrInEdgeEventBuffer) {
            fprintf(stderr, "[%s] error creating usrInEdgeEventBuffer: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
#else
#error we need XX_GPIO_INTERRUPT
#endif
        // FIXME: gpiod_line_settings_set_bias(usrInSettings, GPIOD_LINE_BIAS_PULL_DOWN);
        // FIXME: gpiod_line_settings_set_debounce_period_us(usrInSettings, 100);

        usrInLineConfig = gpiod_line_config_new();
        if (! usrInLineConfig) {
            fprintf(stderr, "[%s] error creating usrInLineConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        for (int i = 0; i < GPIO_USR_INPUTS; i++) {
            if (gpiod_line_config_add_line_settings(usrInLineConfig, &usrInOffsets[i], 1, usrInSettings)) {
                fprintf(stderr, "[%s] error adding usrInOffsets[%d]: %s\n", __func__, i, strerror(errno));
                goto exit_failure;
            }
        }
        usrInRequestConfig = gpiod_request_config_new();
        if (! usrInRequestConfig) {
            fprintf(stderr, "[%s] error creating usrInRequestConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_set_consumer(usrInRequestConfig, application_name);
        usrInLineRequest = gpiod_chip_request_lines(usrGpioChip, usrInRequestConfig, usrInLineConfig);
        if (! usrInLineRequest) {
            fprintf(stderr, "[%s] error creating usrInLineRequest: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_free(usrInRequestConfig);
        usrInRequestConfig = NULL;
        fprintf(stderr, "[%s] usr IN OK\n", __func__);

        // --------- usr outputs
        usrOutSettings = gpiod_line_settings_new();
        if (! usrOutSettings) {
            fprintf(stderr, "[%s] error creating usrOutSettings: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        if (gpiod_line_settings_set_direction(usrOutSettings, GPIOD_LINE_DIRECTION_OUTPUT)) {
            fprintf(stderr, "[%s] error setting direction in usrOutSettings: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        usrOutLineConfig = gpiod_line_config_new();
        if (! usrOutSettings) {
            fprintf(stderr, "[%s] error creating usrOutLineConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        for (int i = 0; i < GPIO_USR_OUTPUTS; ++i) {
            if (gpiod_line_config_add_line_settings(usrOutLineConfig, &usrOutOffsets[i], 1, usrOutSettings)) {
                fprintf(stderr, "[%s] error adding usrOutOffsets[%d]: %s\n", __func__, i, strerror(errno));
                goto exit_failure;
            }
        }
        gpiod_line_config_set_output_values(usrOutLineConfig, usrOutValues, GPIO_USR_OUTPUTS);

        usrOutRequestConfig = gpiod_request_config_new();
        if (! usrOutRequestConfig) {
            fprintf(stderr, "[%s] error creating usrOutRequestConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_set_consumer(usrOutRequestConfig, application_name);
        usrOutLineRequest = gpiod_chip_request_lines(usrGpioChip, usrOutRequestConfig, usrOutLineConfig);
        if (! usrOutLineRequest) {
            fprintf(stderr, "[%s] error creating usrOutLineRequest: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_free(usrOutRequestConfig);
        fprintf(stderr, "[%s] usr OUT OK\n", __func__);
    }

    if (sysGpioChip) {
        sysOutSettings = gpiod_line_settings_new();
        if (! sysOutSettings) {
            fprintf(stderr, "[%s] error creating sysOutSettings: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        if (gpiod_line_settings_set_direction(sysOutSettings, GPIOD_LINE_DIRECTION_OUTPUT)) {
            fprintf(stderr, "[%s] error setting direction in sysOutSettings: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        sysOutLineConfig = gpiod_line_config_new();
        if (! sysOutSettings) {
            fprintf(stderr, "[%s] error creating sysOutLineConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        for (int i = 0; i < GPIO_SYS_OUTPUTS; ++i) {
            if (gpiod_line_config_add_line_settings(sysOutLineConfig, &sysOutOffsets[i], 1, sysOutSettings)) {
                fprintf(stderr, "[%s] error adding sysOutOffsets[%d]: %s\n", __func__, i, strerror(errno));
                goto exit_failure;
            }
        }
        gpiod_line_config_set_output_values(sysOutLineConfig, sysOutValues, GPIO_SYS_OUTPUTS);

        sysOutRequestConfig = gpiod_request_config_new();
        if (! sysOutRequestConfig) {
            fprintf(stderr, "[%s] error creating sysOutRequestConfig: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_set_consumer(sysOutRequestConfig, application_name);

        sysOutLineRequest = gpiod_chip_request_lines(sysGpioChip, sysOutRequestConfig, sysOutLineConfig);
        if (! sysOutLineRequest) {
            fprintf(stderr, "[%s] error creating sysOutLineRequest: %s\n", __func__, strerror(errno));
            goto exit_failure;
        }
        gpiod_request_config_free(sysOutRequestConfig);
        fprintf(stderr, "[%s] sys OUT OK\n", __func__);
    }
    return;

exit_failure:
    if (usrInRequestConfig)
        gpiod_request_config_free(usrInRequestConfig);
    if (usrOutRequestConfig)
        gpiod_request_config_free(usrOutRequestConfig);
    if (sysOutRequestConfig)
        gpiod_request_config_free(sysOutRequestConfig);

    if (usrInLineConfig)
        gpiod_line_config_free(usrInLineConfig);
    if (usrOutLineConfig)
        gpiod_line_config_free(usrOutLineConfig);
    if (usrOutLineConfig)
        gpiod_line_config_free(usrOutLineConfig);

    if (usrInSettings)
        gpiod_line_settings_free(usrInSettings);
    if (usrOutSettings)
        gpiod_line_settings_free(usrOutSettings);
    if (sysOutSettings)
        gpiod_line_settings_free(sysOutSettings);
    xx_gpio_close();

#else
#error unknown platform

#endif
}

void xx_gpio_enable(unsigned n)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr && n < XX_GPIO_MAX) {
        // configure as GPIO
        unsigned i = 0;
        unsigned *reg_ptr = NULL;

        for (i = (n*5); i < ((n+1)*5) && xx_gpio_enabler[i].offset < 0xffff; i++) {
            reg_ptr = (unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_enabler[i].offset);
            *reg_ptr = xx_gpio_enabler[i].value;
        }

        // configure as input (default)
        xx_gpio_config(n, 0);
        xx_gpio_enabled |= (1 << n);
    }

#elif defined(KIT_RPI4)
    if (usrGpioChip && (GPIO_FastIO_ok(n))) {
        xx_gpio_enabled |= (1 << n);
    }

#else
#error unknown platform

#endif
}

int xx_gpio_is_enabled(unsigned n)
{
    return xx_gpio_enabled & (1 << n);
}

int xx_gpio_is_output(unsigned n)
{
    return xx_gpio_output & (1 << n);
}

int xx_gpio_is_input(unsigned n)
{
    return xx_gpio_input & (1 << n);
}

static void xx_gpio_config_set(unsigned n, int output)
{
    if (output)
        xx_gpio_output |= (1 << n);
    else
        xx_gpio_input |= (1 << n);
}

int xx_gpio_has_outputs()
{
    return xx_gpio_output != 0;
}

int xx_gpio_has_inputs()
{
    return xx_gpio_input != 0;
}

void xx_gpio_config(unsigned n, int output)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr && n < XX_GPIO_MAX && (xx_gpio_enabled & (1 << n))) {
        register unsigned *reg_ptr;

        if (output)
            reg_ptr = (unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_doe[n].offset + XX_GPIO_SET_OFFSET);
        else
            reg_ptr = (unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_doe[n].offset + XX_GPIO_CLR_OFFSET);
        *reg_ptr = xx_gpio_doe[n].mask;
        xx_gpio_config_set(n, output);
    }

#elif defined(KIT_RPI4)
    if (n < XX_GPIO_MAX && xx_gpio_enabled & (1 << n)) {
        if (usrGpioChip) {
            if (! output && GPIO_FastIO_Input_ok(n)) {
                xx_gpio_config_set(n, 0);
            } else if (output && GPIO_FastIO_Output_ok(n)) {
                xx_gpio_config_set(n, 1);
            }
        }
    }
#else
#error unknown platform

#endif
}

void xx_gpio_set(unsigned n)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr && n < XX_GPIO_MAX && (xx_gpio_enabled & (1 << n))) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_dout[n].offset + XX_GPIO_SET_OFFSET);
        *reg_ptr = xx_gpio_dout[n].mask;
    }

#elif defined(KIT_RPI4)
    if (n < XX_GPIO_MAX && xx_gpio_enabled & (1 << n)) {
        if (usrGpioChip && GPIO_FastIO_Output_ok(n)) {
            usrOutValues[GPIO_FastIO_Output_v(n)] = 1;
            if (gpiod_line_request_set_values(usrOutLineRequest, usrOutValues))  {
                fprintf(stderr, "[%s] error n=%u: %s\n", __func__, n, strerror(errno));
            }
        }
    }
#else
#error unknown platform

#endif
}

static void xx_gpio_sys_output(unsigned v, int onoff)
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    if (sysGpioChip && v < GPIO_SYS_OUTPUTS) {
        sysOutValues[v] = onoff;
        if (gpiod_line_request_set_values(sysOutLineRequest, sysOutValues))  {
            fprintf(stderr, "[%s] error v=%u onoff=%d: %s\n", __func__, v, onoff, strerror(errno));
        }
    }

#else
#error unknown platform

#endif
}

void xx_gpio_tic_set()
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    xx_gpio_sys_output(GPIO_SYS_TIC_v, 1);

#else
#error unknown platform

#endif
}

void xx_gpio_tac_set()
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    xx_gpio_sys_output(GPIO_SYS_TAC_v, 1);

#else
#error unknown platform

#endif
}

void xx_gpio_clr(unsigned n)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr && n < XX_GPIO_MAX && (xx_gpio_enabled & (1 << n))) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_dout[n].offset + XX_GPIO_CLR_OFFSET);
        *reg_ptr = xx_gpio_dout[n].mask;
    }

#elif defined(KIT_RPI4)
    if (n < XX_GPIO_MAX && xx_gpio_enabled & (1 << n)) {
        if (usrGpioChip && GPIO_FastIO_Output_ok(n)) {
            usrOutValues[GPIO_FastIO_Output_v(n)] = 0;
            if (gpiod_line_request_set_values(usrOutLineRequest, usrOutValues))  {
                fprintf(stderr, "[%s] error n=%u: %s\n", __func__, n, strerror(errno));
            }
        }
    }

#else
#error unknown platform

#endif
}

void xx_gpio_tic_clr()
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    xx_gpio_sys_output(GPIO_SYS_TIC_v, 0);

#else
#error unknown platform

#endif
}

void xx_gpio_tac_clr()
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    xx_gpio_sys_output(GPIO_SYS_TAC_v, 0);

#else
#error unknown platform

#endif
}

int xx_gpio_wait_for_events(int64_t timeout_ns)
{
    int retval = -1;

#if defined(KIT_IMX28)
#pragma message "missing implementation"
    (void)timeout_ns;

#elif defined(KIT_RPI4)
#ifdef XX_GPIO_INTERRUPT
    if (usrGpioChip && timeout_ns) {
        retval = gpiod_line_request_wait_edge_events(usrInLineRequest, timeout_ns);
        if (retval < 0) {
            fprintf(stderr, "[%s] error waiting for events: %s\n", __func__, strerror(errno));
        } else if (retval == 0) {
            // timeout, no events
        } else {
            int events;

            events = gpiod_line_request_read_edge_events(usrInLineRequest, usrInEdgeEventBuffer, GPIO_USR_INPUTS);
            for (int n = 0; n < events; ++n)
            {
                struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(usrInEdgeEventBuffer, n);

                if (event) {
                    int offset = gpiod_edge_event_get_line_offset(event);
                    enum gpiod_edge_event_type event_type = gpiod_edge_event_get_event_type(event);
                    int which_input = -1;

                    if (offset >= 0) {
                        for (int i = 0; i < GPIO_USR_INPUTS; ++i) {
                            if (usrInOffsets[i] == (unsigned)offset) {
                                which_input = i;
                                break;
                            }
                        }
                        if (which_input >= 0) {
                            if (event_type == GPIOD_EDGE_EVENT_RISING_EDGE) {
                                usrInValues[which_input] = 1;
                            } else if (event_type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
                                usrInValues[which_input] = 0;
                            } else {
                                // FIXME: error unknown event_type
                            }
                        } else {
                            // FIXME: error unknown input line
                        }
                    } else {
                        // FIXME: error unkown offset?
                    }
                } else {
                    // FIXME: error wrong event?
                }
            }
        }
    }
#else
    // vedi xx_gpio_readall()
    (void)timeout_ns;
#endif

#else
#error unknown platform

#endif
    return retval;
}

void xx_gpio_readall()
{
#if defined(KIT_IMX28)

#elif defined(KIT_RPI4)
    // NB: vedi xx_gpio_wait_for_events()
    if (usrGpioChip) {
        if (gpiod_line_request_get_values(usrInLineRequest, usrInValues)) {
            // FIXME: manage error
        }
    }

#else
#error unknown platform

#endif
}

int xx_gpio_get(unsigned n)
{
    register int retval = 0;

#if defined(KIT_IMX28)
    if (xx_base_ptr && n < XX_GPIO_MAX && (xx_gpio_enabled & (1 << n))) {
        retval = (*((unsigned *)(xx_base_ptr + XX_PINCTL_BASE + xx_gpio_din[n].offset)) & xx_gpio_din[n].mask) ? 1 : 0;
    }

#elif defined(KIT_RPI4)
    if (n < XX_GPIO_MAX && xx_gpio_enabled & (1 << n)) {
        if (usrGpioChip) {
            if (GPIO_FastIO_Input_ok(n)) {
                retval = usrInValues[GPIO_FastIO_Input_v(n)];
            } else if (GPIO_FastIO_Output_ok(n)) {
                retval = usrOutValues[GPIO_FastIO_Output_v(n)];
            }
        }
    }

#else
#error unknown platform

#endif
    return retval;
}

void xx_gpio_close(void)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        munmap(xx_base_ptr, XX_GPIO_SIZE);
        xx_base_ptr = NULL;
    }
    if (xx_fd > 0) {
        close(xx_fd);
        xx_fd = -1;
    }

#elif defined(KIT_RPI4)
    if (usrGpioChip) {
        if (usrInEdgeEventBuffer)
            gpiod_edge_event_buffer_free(usrInEdgeEventBuffer);
        usrInEdgeEventBuffer = NULL;
        if (usrOutLineRequest)
            gpiod_line_request_release(usrOutLineRequest);
        usrOutLineRequest = NULL;
        gpiod_chip_close(usrGpioChip);
        usrGpioChip = NULL;
    }
    if (sysGpioChip) {
        if (sysOutLineRequest)
            gpiod_line_request_release(sysOutLineRequest);
        sysOutLineRequest = NULL;
        gpiod_chip_close(sysGpioChip);
        sysGpioChip = NULL;
    }

#else
#error unknown platform

#endif
}

void xx_watchdog_enable()
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_RTCCTL_BASE + XX_WATCHDOGEN_OFFS + XX_GPIO_SET_OFFSET);
        *reg_ptr = XX_WATCHDOGEN_MASK;
    }

#elif defined(KIT_RPI4)
#pragma message "missing implementation"

#else
#error unknown platform

#endif
}

void xx_watchdog_disable()
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_RTCCTL_BASE + XX_WATCHDOGEN_OFFS + XX_GPIO_CLR_OFFSET);
        *reg_ptr = XX_WATCHDOGEN_MASK;
    }

#elif defined(KIT_RPI4)
#pragma message "missing implementation"

#else
#error unknown platform

#endif
}

void xx_watchdog_reset(unsigned value_ms)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_RTCCTL_BASE + XX_WATCHDOGms_OFFS);
        *reg_ptr = value_ms;
    }

#elif defined(KIT_RPI4)
#pragma message "missing implementation"
    (void)value_ms;

#else
#error unknown platform

#endif
}

unsigned xx_watchdog_get()
{
    register unsigned retval = 0;

#if defined(KIT_IMX28)

    if (xx_base_ptr) {
        register unsigned *reg_ptr;
        reg_ptr = (unsigned *)(xx_base_ptr + XX_RTCCTL_BASE + XX_WATCHDOGms_OFFS);
        retval = *reg_ptr;
    }

#elif defined(KIT_RPI4)
#pragma message "missing implementation"

#else
#error unknown platform

#endif
    return retval;
}

void xx_nbacklight(unsigned p)
{
    unsigned nBacklight = p & 0x000000FF;

#if defined(KIT_IMX28)
#pragma message "missing implementation"


#elif defined(KIT_RPI4)
    xx_gpio_sys_output(GPIO_nBACKLIGHT_v, nBacklight); // FIXME: use PWM

#else
#error unknown platform

#endif
}

void xx_pwm3_set(unsigned duty_cycle)
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;
        //register unsigned period_cycles = 37495; // 100ms = 37495 * 2.667us (24Mhz/64)
        register unsigned period_cycles = 3750; // 10ms = 3750 * 2.667us (24Mhz/64)

        // PERIOD=period_cycles ACTIVE_STATE=1 INACTIVE_STATE=0 CDIV=DIV_64
        reg_ptr = (unsigned *)(xx_base_ptr + XX_PWMCTL_BASE + XX_PWM3_PERIOD_OFFS);
        *reg_ptr = period_cycles + 0x005B0000;

        // ACTIVE=0 INACTIVE=(duty cycle)
        if (duty_cycle > 100)
            duty_cycle = 100;
        reg_ptr = (unsigned *)(xx_base_ptr + XX_PWMCTL_BASE + XX_PWM3_ACTIVE_OFFS);
        *reg_ptr = (period_cycles * duty_cycle / 100) << 16;
    }

#elif defined(KIT_RPI4)
    (void)duty_cycle;

#else
#error unknown platform

#endif
}

void xx_pwm3_enable()
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_PWMCTL_BASE + XX_PMW3_ENABLE_OFFS + XX_GPIO_SET_OFFSET);
        *reg_ptr = XX_PMW3_ENABLE_MASK;
    }

#elif defined(KIT_RPI4)
    if (sysGpioChip) {
        sysOutValues[GPIO_BUZZER_v] = 1;
        if (gpiod_line_request_set_values(sysOutLineRequest, sysOutValues))  {
            fprintf(stderr, "[%s] error: %s\n", __func__, strerror(errno));
        }
    }

#else
#error unknown platform

#endif
}

void xx_pwm3_disable()
{
#if defined(KIT_IMX28)
    if (xx_base_ptr) {
        register unsigned *reg_ptr;

        reg_ptr = (unsigned *)(xx_base_ptr + XX_PWMCTL_BASE + XX_PMW3_ENABLE_OFFS  + XX_GPIO_CLR_OFFSET);
        *reg_ptr = XX_PMW3_ENABLE_MASK;
    }

#elif defined(KIT_RPI4)
    if (sysGpioChip) {
        sysOutValues[GPIO_BUZZER_v] = 0;
        if (gpiod_line_request_set_values(sysOutLineRequest, sysOutValues))  {
            fprintf(stderr, "[%s] error: %s\n", __func__, strerror(errno));
        }
    }

#else
#error unknown platform

#endif
}

/* ---------------------------------------------------------------------------- */
