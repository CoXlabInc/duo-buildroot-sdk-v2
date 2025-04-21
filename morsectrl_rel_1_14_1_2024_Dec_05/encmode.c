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

struct PACKED set_enc_mode_command
{
    /* data of this message */
    uint8_t enc_mode;
};

static struct {
    struct arg_int *encmode;
} args;

int enc_mode_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set TIM encoding mode",
        args.encmode = arg_int1(NULL, NULL, "<value>", "TIM encoding mode"));
    return 0;
}

int enc_mode(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_enc_mode_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_enc_mode_command);

    cmd->enc_mode = args.encmode->ival[0];

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ENC_MODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(enc_mode, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
