#ifndef XX_MECT_H
#define XX_MECT_H

int xx_mect_connect(unsigned devnum, unsigned baudrate, char parity, unsigned databits, unsigned stopbits, unsigned timeout_ms);
int xx_mect_read_ascii(int fd, unsigned node, unsigned command, float *value);
int xx_mect_write_ascii(int fd, unsigned node, unsigned command, float value);
int xx_mect_read_hexad(int fd, unsigned node, unsigned command, unsigned *value);
int xx_mect_write_hexad(int fd, unsigned node, unsigned command, unsigned value);
void xx_mect_close(int fd);

#endif // XX_MECT_H
