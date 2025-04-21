/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

struct PACKED override_pa_on_delay_command
{
    /** The flags of this message. Bool, 1=enabled, 0=disabled */
    uint8_t enable;
    uint32_t delay_us;
};

static struct
{
    struct arg_rex *enable;
    struct arg_int *delay_us;
} args;

int override_pa_on_delay_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable overriding PA turn on delay",
    args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
        "Enable overriding with given delay, or disable overriding"),
    args.delay_us = arg_rint0(NULL, NULL, "<delay>", 0, INT32_MAX, "PA turn on delay in usecs"));
    return 0;
}

int override_pa_on_delay(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    bool enable = 0;
    uint32_t delay_us = 0;
    struct override_pa_on_delay_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    if (strcmp("enable", args.enable->sval[0]) == 0)
    {
        if (args.delay_us->count != 1)
        {
            mm_print_missing_argument(&args.delay_us->hdr);
            ret = -1;
            goto exit;
        }
        enable = 1;
        delay_us = args.delay_us->ival[0];
    }
    else if (strcmp("disable", args.enable->sval[0]) == 0)
    {
        enable = 0;
        delay_us = 0;
    }
    else
    {
        /* shouldn't be possible to get here */
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct override_pa_on_delay_command);
    cmd->enable = htole32(enable);
    cmd->delay_us = htole32(delay_us);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_OVERRIDE_PA_ON_DELAY,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(override_pa_on_delay, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
