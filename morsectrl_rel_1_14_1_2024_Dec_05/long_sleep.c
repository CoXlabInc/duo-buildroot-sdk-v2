/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

struct PACKED set_long_sleep_config_command
{
    uint8_t long_sleep_enabled;
};

struct
{
    struct arg_rex *enable;
} args;

int long_sleep_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure long sleep mode (allow sleeping through DTIM)",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable long sleep mode"));
    return 0;
}


int long_sleep(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct set_long_sleep_config_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enabled = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_long_sleep_config_command);
    cmd->long_sleep_enabled = enabled;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_LONG_SLEEP_CONFIG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(long_sleep, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
