/*
 * Copyright 2021 Mect s.r.l
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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---------------------------------------------------------------------------- */

#undef VERBOSE_DEBUG

#define RETENTIVE_FILE 			"/local/retentive"
uint32_t *xx_retentives_ptr = NULL;

static void *ptRetentive = NULL;
static off_t lenRetentive = 0;

/* ---------------------------------------------------------------------------- */

void xx_retentives_init(int size)
{
    int fd = -1;
    struct stat sb;

    if (xx_retentives_ptr != NULL) {
        fprintf(stderr, "[%s] recursive call\n", __func__);
        goto exit_failure;
    }
    ptRetentive = NULL;
    lenRetentive = 0;

    fd = open(RETENTIVE_FILE, O_RDWR | O_SYNC);
    if (fd == -1) {
        fprintf(stderr, "[%s] error in open(): %s\n", __func__, strerror(errno));
        goto exit_failure;
    }
    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "[%s] error in fstat(): %s\n", __func__, strerror(errno));
        goto exit_failure;
    }
    if (!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "[%s] error %s is not a file\n", __func__, RETENTIVE_FILE);
        goto exit_failure;
    }
    lenRetentive = sb.st_size;
    if (lenRetentive != size) {
        fprintf(stderr, "Wrong retentive file size: got %ld expecting %u.\n", lenRetentive, size);
        goto exit_failure;
    }
    ptRetentive = mmap(0, lenRetentive, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptRetentive == MAP_FAILED) {
        fprintf(stderr, "[%s] error in mmap(): %s\n", __func__, strerror(errno));
        goto exit_failure;
    }
    if (close(fd) == -1) {
        fprintf(stderr, "[%s] error in close(): %s\n", __func__, strerror(errno));
        goto exit_failure;
    }
    xx_retentives_ptr = (uint32_t *)ptRetentive;
    return;

exit_failure:
    if (ptRetentive && ptRetentive != MAP_FAILED) {
        munmap(ptRetentive, lenRetentive);
        ptRetentive = NULL;
        lenRetentive = 0;
    }
    if (fd > 0) {
        close(fd);
    }
}

void xx_retentives_sync()
{
    if (ptRetentive) {
        msync(ptRetentive, lenRetentive, MS_SYNC);
    }
}

void xx_retentives_dump()
{
    int uRes;

    if (ptRetentive) {
        uRes = msync(ptRetentive, lenRetentive, MS_SYNC);
        if (uRes != 0) {
            fprintf(stderr,"%s CANNOT sync retain variables!\n", __func__);
        }
        uRes = munmap(ptRetentive, lenRetentive);
        if (uRes != 0) {
            fprintf(stderr,"%s CANNOT unmap retain variables!\n", __func__);
        }
        sync();
    }
}
