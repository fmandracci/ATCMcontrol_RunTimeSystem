#ifndef XX_GPIO_H
#define XX_GPIO_H

#if defined(KIT_IMX28)
#define XX_GPIO_MAX 18
#define XX_GPIO_MIN_TEST 0
#define XX_GPIO_MAX_TEST 8
#undef  XX_GPIO_INTERRUPT

#elif defined(KIT_RPI4)
#define XX_GPIO_MAX 8
#define XX_GPIO_MIN_TEST 4
#define XX_GPIO_MAX_TEST 8
#define XX_GPIO_INTERRUPT

#else
#error unknown platform

#endif

#include <stdint.h> // int64_t

void xx_gpio_init();
void xx_gpio_enable(unsigned n);
int xx_gpio_is_enabled(unsigned n);
int xx_gpio_is_output(unsigned n);
int xx_gpio_is_input(unsigned n);
int xx_gpio_has_outputs();
int xx_gpio_has_inputs();
void xx_gpio_config(unsigned n, int output);
void xx_gpio_set(unsigned n);
void xx_gpio_clr(unsigned n);
void xx_gpio_refresh(unsigned fastio_1_addr);
int xx_gpio_wait_for_events(int64_t timeout_ns);
void xx_gpio_readall();
int xx_gpio_get(unsigned n);
void xx_gpio_close();

void xx_gpio_tic_set();
void xx_gpio_tac_set();
void xx_gpio_tic_clr();
void xx_gpio_tac_clr();

void xx_watchdog_enable();
void xx_watchdog_disable();
void xx_watchdog_reset(unsigned value_ms);
unsigned xx_watchdog_get();

void xx_nbacklight(unsigned p);

void xx_pwm3_set(unsigned duty_cycle);
void xx_pwm3_enable();
void xx_pwm3_disable();

#endif // XX_GPIO_H
