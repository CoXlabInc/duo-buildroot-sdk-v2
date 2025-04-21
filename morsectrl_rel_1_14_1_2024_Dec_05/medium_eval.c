/*
 * Copyright 2024 Morse Micro
 */

#include <stdio.h>
#include "command.h"
#include "utilities.h"

struct PACKED command_medium_eval
{
    /* 0 sets PHY into normal operation, 1 sets PHY into deaf/blocked mode */
    uint8_t enable;
};

static struct arg_rex *medium_eva_arg;

int medium_eval_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable medium evaluation mode",
                     medium_eva_arg = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX,
                         MM_ARGTABLE_ENABLE_DATATYPE, 0, "Enable/disable medium evaluation mode"));
    return 0;
}

int medium_eval(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    int enabled;
    struct command_medium_eval *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enabled = expression_to_int(medium_eva_arg->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_medium_eval);
    cmd->enable = enabled;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_MEDIUM_EVAL,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (cmd_tbuff)
        morsectrl_transport_buff_free(cmd_tbuff);
    if (rsp_tbuff)
        morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(medium_eval, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
