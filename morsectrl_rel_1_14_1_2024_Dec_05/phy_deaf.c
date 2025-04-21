/*
 * Copyright 2021 Morse Micro
 */

#include <stdio.h>
#include "command.h"
#include "utilities.h"

struct PACKED command_phy_deaf
{
    /* 0 sets PHY into normal operation, 1 sets PHY into deaf/blocked mode */
    uint8_t enable;
};

static struct
{
    struct arg_int *mode;
} args;

int phy_deaf_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure PHY deaf and blocking modes",
        args.mode = arg_rint1(NULL, NULL, "<mode>", 0, 3, "PHY mode"),
        arg_rem(NULL, "Normal (0): PHY transmit/receive unblocked"),
        arg_rem(NULL, "Deaf (1): PHY receive blocked"),
        arg_rem(NULL, "Blocked (2): PHY transmit blocked"),
        arg_rem(NULL, "Both (3): PHY receive/transmit blocked"));
    return 0;
}

int phy_deaf(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    int8_t phy_deaf;
    struct command_phy_deaf *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    phy_deaf = (int8_t)args.mode->ival[0];

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_phy_deaf);
    cmd->enable = phy_deaf;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_PHY_DEAF,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (cmd_tbuff)
        morsectrl_transport_buff_free(cmd_tbuff);
    if (rsp_tbuff)
        morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(phy_deaf, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
