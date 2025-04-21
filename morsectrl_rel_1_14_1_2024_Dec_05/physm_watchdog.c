/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"

struct PACKED command_set_physm_watchdog
{
    /* physm watchdog */
    uint8_t physm_watchdog_en;
};

static struct
{
    struct arg_rex *enable;
} args;

int physm_watchdog_en_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable PHYSM watchdog with a 60ms timeout",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable the watchdog"));
    return 0;
}

int physm_watchdog_en(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    uint8_t physm_watchdog_en;
    struct command_set_physm_watchdog *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    physm_watchdog_en = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_physm_watchdog);
    cmd->physm_watchdog_en = physm_watchdog_en;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_PHYSM_WATCHDOG,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(physm_watchdog_en, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
