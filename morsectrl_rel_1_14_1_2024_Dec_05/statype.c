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

struct PACKED set_sta_type_command
{
    /* data of this message */
    uint8_t sta_type;
};

static struct
{
    struct arg_int *sta_type;
} args;

int sta_type_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the S1G STA type",
        args.sta_type = arg_rint1(NULL, NULL, "<type>", 0, 2,
            "0 (mixed), 1 (sensor), or 2 (non-sensor)"));
    return 0;
}

int sta_type(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_sta_type_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_sta_type_command);

    cmd->sta_type = args.sta_type->ival[0];

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_STA_TYPE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(sta_type, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
