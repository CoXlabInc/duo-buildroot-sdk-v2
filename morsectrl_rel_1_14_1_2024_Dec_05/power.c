/*
 * Copyright 2024 Morse Micro
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "utilities.h"

enum power_mode {
    POWER_MODE_SNOOZE,
    POWER_MODE_DEEP_SLEEP,
    POWER_MODE_HIBERNATE,
};

struct PACKED command_force_power_mode
{
    /* mode of operation to force @ref enum power_mode */
    uint32_t mode;
};

static struct
{
    struct arg_rex *ps_mode;
} args;

int power_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Force chip into a specific power mode",
        args.ps_mode = arg_rex1(NULL, NULL, "(hibernate)", "hibernate", 0, "Power mode"),
        arg_rem(NULL, "Power mode 'hibernate' requires reset to recover the chip"));
    return 0;
}

int power(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    enum power_mode mode;
    struct command_force_power_mode *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (!strcmp(args.ps_mode->sval[0], "hibernate"))
    {
        mode = POWER_MODE_HIBERNATE;
    }
    else
    {
        mctrl_err("Invalid power mode '%s'\n", args.ps_mode->sval[0]);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_force_power_mode);
    cmd->mode = mode;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_FORCE_POWER_MODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(power, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
