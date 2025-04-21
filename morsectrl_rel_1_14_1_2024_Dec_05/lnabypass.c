/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"

struct PACKED command_lna_bypass
{
    /* 0 sets LNA as default, 1 sets LNA in bypass mode */
    uint8_t lna_bypass;
};

static struct
{
    struct arg_int *mode;
} args;

int lnabypass_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set LNA bypass mode",
        args.mode = arg_rint1(NULL, NULL, "<bypass mode>", 0, 1,
            "0 = auto mode, 1 = always bypassed mode"));
    return 0;
}

int lnabypass(struct morsectrl *mors, int argc, char *argv[])
{
    int ret  = -1;
    uint8_t lna_bypass;
    struct command_lna_bypass *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    lna_bypass = args.mode->ival[0];

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_lna_bypass);
    cmd->lna_bypass = lna_bypass;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_LNA_BYPASS,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(lnabypass, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
