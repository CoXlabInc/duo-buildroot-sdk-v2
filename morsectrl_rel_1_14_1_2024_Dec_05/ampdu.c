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


struct PACKED set_ampdu_command
{
    uint8_t ampdu_enabled;
};

static struct arg_rex *ampdu_enable_arg;

int ampdu_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable AMPDU",
        ampdu_enable_arg = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX,
            MM_ARGTABLE_ENABLE_DATATYPE, 0, "Enable/disable A-MPDU sessions"),
        arg_rem(NULL, "Must be run before association"));
    return 0;
}

int ampdu(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct set_ampdu_command *cmd;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ampdu_command);
    cmd->ampdu_enabled = 0;

    enabled = expression_to_int(ampdu_enable_arg->sval[0]);

    cmd->ampdu_enabled = enabled;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_AMPDU,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ampdu, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
