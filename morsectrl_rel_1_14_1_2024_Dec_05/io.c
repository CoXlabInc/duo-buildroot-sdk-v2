/*
 * Copyright 2020 Morse Micro
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "command.h"

char *MORSE_IO_DEV_NAMES[] =
{
    "/dev/morse_io",
    "/dev/morsef2", /* Legacy */
    "/dev/morsef1"  /* Legacy */
};

static struct
{
    struct arg_lit *read;
    struct arg_int *size;
    struct arg_str *filename;
    struct arg_int *address;
    struct arg_lit *verbose;
    struct arg_int *value;
} args;

int io_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Read from or write to the chip",
        args.read = arg_lit0("r", "read", "Read from chip (default: write)"),
        args.size = arg_int0("s", NULL, "<bytes>",
            "Size of file read/write (write default: file size, read default: 4)"),
        args.filename = arg_str0("f", "filename", NULL, "Name of file to read or write"),
        args.verbose = arg_lit0("v", NULL, "Verbose output"),
        args.address = arg_int1(NULL, NULL, "address", "Hardware address"),
        args.value = arg_int0(NULL, NULL, "value", "Value to write if no filename provided"));
    return 0;
}

static int32_t file_size(FILE *fptr)
{
    int32_t size;
    fseek(fptr, 0L, SEEK_END);
    size = ftell(fptr);
    fseek(fptr, 0L, SEEK_SET);
    return size;
}

int io(struct morsectrl *mors, int argc, char *argv[])
{
    int fd;
    int ret = 0;
    int tx_bytes = 0;
    int count = 0;
    int size = 0;
    int actual_read_size;
    int dir_write, func = 2, verbose = 0;
    uint32_t value = 0, address = 0, *data = &value, *buffer = NULL;
    const char *filename = NULL;
    FILE *fptr = NULL;

    dir_write = (args.read->count == 0);

    if (args.size->count)
    {
        size = args.size->ival[0];
    }

    if (args.filename->count)
    {
        filename = args.filename->sval[0];
    }

    verbose = (args.verbose->count > 0);

    address = args.address->ival[0];
    /* Open file */
    if (filename != NULL)
    {
        if (dir_write == 1)
        {
            fptr = fopen(filename, "rb");
        }
        else
        {
            fptr = fopen(filename, "wb");
        }
        if (!fptr)
        {
            mctrl_err("Couldn't open file\n");
            return -1;
        }
    }

    /* calculate size */
    if (fptr)
    {
        /* if size set use it, else write ? filesize : 4 */
        size = size ? size : dir_write ? file_size(fptr) : 4;
        if (size < 0)
        {
            mctrl_err("Failed to get file size");
            return -1;
        }
    }
    else
    {
        if (size)
        {
            mctrl_err("Invalid size. Only set with file access\n");
            return -1;
        }
        size = 4;
    }

    /* Allocate buffer if required (i.e > 4)*/
    if (size > 4)
    {
        buffer = malloc(size);
        if (!buffer)
        {
            mctrl_err("Failed to allocate memory\n");
            fclose(fptr);
            return (-ENOMEM);
        }
        data = buffer;
    }

    /* Read input */
    if (dir_write == 1)
    {
        if (fptr)
        {
            /* From input file */
            actual_read_size = fread(data, 1, size, fptr);
            if (actual_read_size != size)
            {
                mctrl_err("Error occured when reading from file (%d)", ferror(fptr));
                return -1;
            }
        }
        else
        {
            if (args.value->count == 0)
            {
                mctrl_err("No value provided when writing without a file\n");
                return -1;
            }
            value = args.value->ival[0];
        }
    }

    /* open device */
    int retries = 0;
    while (retries < MORSE_ARRAY_SIZE(MORSE_IO_DEV_NAMES))
    {
        if ((fd = open(MORSE_IO_DEV_NAMES[retries], O_RDWR)) >= 0)
        {
            break;
        }
        retries++;
    }
    if (retries == MORSE_ARRAY_SIZE(MORSE_IO_DEV_NAMES))
    {
        mctrl_err("Failed to open device file\n");
        return -1;
    }

    if (verbose)
    {
        mctrl_print("%s %d bytes %s Func%d (%s %s)\n",
            dir_write ? "writing" : "reading",
            size,
            dir_write ? "to" : "from",
            func,
            dir_write ? "from" : "to",
            fptr ? filename : "command line");
    }

    /* write/read */
    while (count < size)
    {
        ioctl(fd, _IO('k', 1), address + count);
        tx_bytes = dir_write ? write(fd, data + count, size) : read(fd, data + count, size);
        if (tx_bytes < 0)
        {
            mctrl_err("Failed to %s device\n", dir_write ? "write to" : "read from");
            return -1;
        }
        if (!dir_write)
        {
            /* write data */
            if (fptr)
            {
                fwrite(data, 1, tx_bytes, fptr);
            }
            else
            {
                mctrl_print("0x%08X\n", value);
            }
        }
        count += tx_bytes;
    }

    return ret;
}

MM_CLI_HANDLER(io, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
