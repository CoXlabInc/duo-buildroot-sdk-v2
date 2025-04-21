/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

struct PACKED set_cts_self_ps_command
{
    /** The flag of this message */
    uint8_t enable;
};

static struct
{
    struct arg_rex *enable;
} args;

int cts_self_ps_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable CTS-to-self PS beacon mode",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable CTS-to-self PS beacon mode"));
    return 0;
}

int cts_self_ps(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enable;
    struct set_cts_self_ps_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enable = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_cts_self_ps_command);
    cmd->enable = enable;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CTS_SELF_PS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(cts_self_ps, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
