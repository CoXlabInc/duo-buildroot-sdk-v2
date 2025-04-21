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
#include "channel.h"

#define BANDWIDTH_DEFAULT 0xFF
#define PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT 0xFF

static struct
{
    struct arg_rex *bw;
} args;

int bw_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Get (default) or set the channel bandwidth",
        args.bw = arg_rex0(NULL, NULL, "(1|2|4|8)", "<bw>", 0, "New bandwidth {1|2|4|8}"));
    return 0;
}

int bw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t bw;
    struct command_set_channel_req *cmd;
    struct command_get_channel_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct  command_get_channel_cfm);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_FULL_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        ret = -1;
        goto exit;
    }

    if (args.bw->count == 0)
    {
        mctrl_print("Current bw is (%d)\n", resp->operating_channel_bw_mhz);
    }
    else
    {
        if (str_to_uint8((char *)args.bw->sval[0], &bw))
        {
            goto exit;
        }

        cmd->operating_channel_freq_hz = resp->operating_channel_freq_hz;
        cmd->operating_channel_bw_mhz = bw;
        cmd->primary_channel_bw_mhz = BANDWIDTH_DEFAULT;
        cmd->primary_1mhz_channel_index = PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT;
        cmd->dot11_mode = 0; /* TODO */

        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CHANNEL,
                                     cmd_tbuff, rsp_tbuff);
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER_DEPRECATED(bw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
