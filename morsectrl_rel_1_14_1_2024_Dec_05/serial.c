/*
 * Copyright 2021 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"

static struct mm_argtable io_argtable;

int io_init(struct morsectrl *mors, struct mm_argtable *mm_args);
int io(struct morsectrl *mors, int argc, char *argv[]);

int serial_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Read the chip serial number from OTP (zeros if not set)");
    io_init(mors, &io_argtable);
    return 0;
}

/**
 * This address should be the same for all boards but some boards will
 * have the bits not blown which will always show zeroes in that case,
 * please update the address if necessary
 */
#define SERIAL_OTP_ADDR "0x1005412c"

int serial(struct morsectrl *mors, int argc, char *argv[])
{
    int ret;

    char io_cmd_name[] = "io";
    char io_read_arg[] = "-r";
    char otp_addr[] = SERIAL_OTP_ADDR;

    char *io_argv[] = {io_cmd_name, io_read_arg, otp_addr};
    int io_argc = MORSE_ARRAY_SIZE(io_argv);

    ret = mm_parse_argtable_noerror("Internal IO command", &io_argtable, io_argc, io_argv);
    if (ret)
    {
        arg_print_errors(stdout, io_argtable.end, TOOL_NAME);
        goto exit;
    }
    ret = io(mors, io_argc, io_argv);

exit:
    return ret;
}

MM_CLI_HANDLER(serial, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
