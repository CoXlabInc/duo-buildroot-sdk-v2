/*
 * Copyright 2020 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"

struct PACKED set_mcs_command
{
    /** The flags of this message */
    uint32_t mcs;
};

static struct
{
    struct arg_rex *mcs;
} args;

int mcs_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set MCS mode",
        args.mcs = arg_rex1(NULL, NULL, "(auto|10|[0-9])", "{auto|0-10}", 0,
            "MCS rate (0-10) or 'auto' for automatic rate control"));
    return 0;
}

int mcs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t mcs;
    struct set_mcs_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mcs_command);

    if (!strcmp(args.mcs->sval[0], "auto"))
    {
        mcs = 0x7FFFFFFF;
    }
    else
    {
        if (str_to_uint32_range(args.mcs->sval[0], &mcs, 0, 10))
        {
            mctrl_err("Invalid mcs value\n");
            ret = -1;
            goto exit;
        }
    }

    cmd->mcs = htole32(mcs);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_MODULATION,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mcs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
