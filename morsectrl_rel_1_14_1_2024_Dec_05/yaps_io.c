/*
 * Copyright 2023 Morse Micro
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "utilities.h"
#include "morsectrl.h"

/* Calculate padding required for yaps transaction */
#define YAPS_CALC_PADDING(_bytes) ((_bytes) & 0x3 ? (4 - ((_bytes) & 0x3)) : 0)

/*
 * Yaps data stream delimiter is a 32 bit word with the following fields:
 *
 * pkt_size (14 bits) - Packet size not including delimiter or padding
 * pool_id  (3  bits) - Pool that pages should be allocated from.
 *                      Pool IDs defined in enum yaps_alloc_pool
 * padding  (2  bits) - Padding required to bring packet to word (4 byte) boundary
 * irq      (1  bit ) - Raise a PKT_IRQ on the YDS this is sent to
 * reserved (5  bits) - Reserved, must write as 0
 * crc      (7  bits) - YAPS CRC
 */

/* Packet size not including delimiter or padding */
#define YAPS_DELIM_GET_PKT_SIZE(_delim)     (_delim & 0x3FFF)
#define YAPS_DELIM_SET_PKT_SIZE(_pkt_size)  (_pkt_size & 0x3FFF)
/* Pool that pages should be allocated from. Pool IDs defined in enum yaps_alloc_pool */
#define YAPS_DELIM_GET_POOL_ID(_delim)      ((_delim >> 14) & 0x7)
#define YAPS_DELIM_SET_POOL_ID(_pool_id)    ((_pool_id & 0x7) << 14)
/* Padding required to bring packet to word (4 byte) boundary */
#define YAPS_DELIM_GET_PADDING(_delim)      ((_delim >> 17) & 0x3)
#define YAPS_DELIM_SET_PADDING(_padding)    ((_padding & 0x3) << 17)
/* Raise a PKT_IRQ on the YDS this is sent to */
#define YAPS_DELIM_GET_IRQ(_delim)          ((_delim >> 19) & 0x1)
#define YAPS_DELIM_SET_IRQ(_irq)            ((_irq & 0x1) << 19)
/* Reserved, must write as 0 */
#define YAPS_DELIM_GET_RESERVED(_delim)     ((_delim >> 20) & 0x1F)
#define YAPS_DELIM_SET_RESERVED(_reserved)  ((_reserved & 0x1F) << 20)
/* YAPS CRC */
#define YAPS_DELIM_GET_CRC(_delim)          ((_delim >> 25) & 0x7F)
#define YAPS_DELIM_SET_CRC(_crc)            ((_crc & 0x7F) << 25)

#define YAPS_YDS_YSL_ADDRESS                0x00170000
#define YAPS_DELIMITER_SIZE                 4


#define MORSE_YAPS_IO_DEV_NAMES "/dev/morse_io"
#define HELP_PARSED_ERROR_CODE  INT32_MIN

struct yaps_io_options
{
    bool verbose;
    bool dir_write;
    uint32_t data_size;
    uint32_t pool_id;
    uint32_t address;
    uint32_t value;
    const char *filename;
};

static struct
{
    struct arg_lit *read;
    struct arg_int *queue_id;
    struct arg_int *size;
    struct arg_str *filename;
    struct arg_int *address;
    struct arg_lit *verbose;
    struct arg_int *value;
} args;

int yaps_io_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Read or write to the YAPS YSL/YDS",
        args.read = arg_lit0("r", "read", "Read from chip (default is write)"),
        args.queue_id = arg_rint0("q", "queue_id", "<queue>", 0, 3,
           "YAPS pool ID (only used with write operations)"),
        args.size = arg_rint0("s", NULL, "<bytes>", 1, INT16_MAX,
            "Size of file read/write (write default: file size, read default: 4)"),
        args.filename = arg_str0("f", "filename", NULL, "Name of file to read or write"),
        args.address = arg_int0("y", "address", NULL, "YAPS YDS/YSL address"),
        args.verbose = arg_lit0("v", NULL, "Verbose output"),
        args.value = arg_int0(NULL, NULL, "value", "Value to write if no filename provided"));

    args.address->ival[0] = YAPS_YDS_YSL_ADDRESS;
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

static inline uint32_t yaps_delimiter(unsigned int size, uint8_t pool_id, bool irq)
{
    uint32_t delim = 0;

    delim |= YAPS_DELIM_SET_PKT_SIZE(size);
    delim |= YAPS_DELIM_SET_PADDING(YAPS_CALC_PADDING(size));
    delim |= YAPS_DELIM_SET_POOL_ID(pool_id);
    delim |= YAPS_DELIM_SET_IRQ(irq);
    delim |= YAPS_DELIM_SET_CRC(crc7_gen(delim & 0x1ffffff, 32));
    return delim;
}

int yaps_io_get_options(struct yaps_io_options *parsed_options)
{
    int ret = 0;
    parsed_options->dir_write = (args.read->count == 0);

    /* If writing, but no filename and no value given, error */
    if (args.read->count == 0 &&
        args.filename->count == 0 &&
        args.value->count == 0)
    {
        mctrl_err("No write value provided\n");
        ret = -1;
    }

    parsed_options->address = args.address->ival[0];

    if (args.filename->count)
        parsed_options->filename = args.filename->sval[0];

    parsed_options->verbose = (args.verbose->count > 0);

    if (args.size->count)
        parsed_options->data_size = args.size->ival[0];

    parsed_options->pool_id = args.queue_id->ival[0];
    parsed_options->value = args.value->ival[0];

    return ret;
}

int yaps_io(struct morsectrl *mors, int argc, char *argv[])
{
    int device = -1;
    int ret = 0;
    int xfer_bytes = 0;
    int count = 0, transfer_size = 0;
    int data_size;
    int actual_read_size;
    int func = 2;
    uint32_t value[2], *data = value, *buffer = NULL;
    FILE *fptr = NULL;
    struct yaps_io_options parsed_options = {
        .verbose = false,
        .dir_write = true,
        .data_size = 0,
        .pool_id = 0,
        .address = YAPS_YDS_YSL_ADDRESS,
        .filename = NULL
    };

    ret = yaps_io_get_options(&parsed_options);
    if (ret)
    {
        ret = (ret == HELP_PARSED_ERROR_CODE) ? 0 : ret;
        goto exit;
    }

    /* Open file */
    if (parsed_options.filename != NULL)
    {
        if (parsed_options.dir_write)
        {
            fptr = fopen(parsed_options.filename, "rb");
        }
        else
        {
            fptr = fopen(parsed_options.filename, "wb");
        }

        if (!fptr)
        {
            mctrl_err("Couldn't open file\n");
            ret = -1;
            goto exit;
        }

        /* calculate data_size */
        data_size = parsed_options.data_size ?
                        parsed_options.data_size : parsed_options.dir_write ?
                                                        file_size(fptr) : 4;
        if (data_size < 0)
        {
            mctrl_err("Failed to get file size");
            goto exit;
        }
    }
    else if (parsed_options.data_size)
    {
        mctrl_err("Size can only be specified for file read/write operations\n");
        ret = -1;
        goto exit;
    }
    else
    {
        data_size = 4;
    }

    transfer_size = YAPS_DELIMITER_SIZE + data_size + YAPS_CALC_PADDING(data_size);

    /* Allocate buffer if required (i.e. > 4) */
    if (data_size > 4)
    {
        /* Allocate for size and yaps delimiter (4 extra bytes) */
        buffer = malloc(transfer_size);
        if (!buffer)
        {
            mctrl_err("Failed to allocate memory\n");
            ret = -ENOMEM;
            goto exit;
        }
        data = buffer;
    }

    /* Read input */
    if (parsed_options.dir_write)
    {
        /* Append the yaps delimiter to the data */
        if (parsed_options.verbose)
        {
            mctrl_print("Generating yaps delimiter with size = %d, pool id = %d, irq = true\n",
                        data_size, parsed_options.pool_id);
        }
        data[0] = yaps_delimiter(data_size, parsed_options.pool_id, true);

        if (fptr)
        {
            /* From input file */
            actual_read_size = fread(data, 1, data_size, fptr);
            if (actual_read_size != data_size)
            {
                mctrl_err("Error occured when reading from file (%d)", ferror(fptr));
                ret = -1;
                goto exit;
            }
        }
        else
        {
            data[1] = parsed_options.value;
        }
    }

    device = open(MORSE_YAPS_IO_DEV_NAMES, O_RDWR);
    if (device < 0)
    {
        mctrl_err("Failed to open device file\n");
        ret = -1;
        goto exit;
    }

    if (parsed_options.verbose)
    {
        mctrl_print("%s %d bytes %s Func%d (%s %s)\n",
           parsed_options.dir_write ? "writing" : "reading",
           transfer_size,
           parsed_options.dir_write ? "to" : "from",
           func,
           parsed_options.dir_write ? "from" : "to",
           fptr ? parsed_options.filename : "command line");
    }

    /* write/read */
    while (count < transfer_size)
    {
        ioctl(device, _IO('k', 1), parsed_options.address);
        xfer_bytes = parsed_options.dir_write ?
            write(device, data + count, transfer_size) : read(device, data + count, transfer_size);
        if (xfer_bytes < 0)
        {
            mctrl_err("Failed to %s device\n",
                parsed_options.dir_write ? "write to" : "read from");
            ret = -1;
            goto exit;
        }
        if (!parsed_options.dir_write)
        {
            /* write data */
            if (fptr)
            {
                if (fwrite(data, 1, xfer_bytes, fptr) != xfer_bytes)
                {
                    mctrl_err("Failed to write the result to %s\n", parsed_options.filename);
                    ret = -1;
                    goto exit;
                }
            }
            else
            {
                mctrl_print("0x%08X\n", value[1]);
            }
        }
        count += xfer_bytes;
    }

exit:
    /* Clean up */
    free(buffer);
    close(device);

    if (fptr)
    {
        fclose(fptr);
    }

    return ret;
}

MM_CLI_HANDLER(yaps_io, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
