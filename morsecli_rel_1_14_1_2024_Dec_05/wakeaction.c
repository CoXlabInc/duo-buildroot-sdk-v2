/*
 * Copyright 2022 Morse Micro
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_send_wake_action_frame_req
{
    uint8_t dest_addr[6];
    uint32_t payload_size;
    uint8_t payload[0];
};

static struct
{
    struct arg_rex *macaddr;
    struct arg_str *payload;
} args;

int wakeaction_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Send a wake action frame to a destination",
        args.macaddr = arg_rex1(NULL, NULL, "([a-f0-9]{2}:){5}([a-f0-9]{2})",
            "<MAC Address>", ARG_REX_ICASE, "Destination MAC address"),
        args.payload = arg_str1(NULL, NULL, "<payload>", "Hex string of payload to send"));
    return 0;
}

int wakeaction(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_send_wake_action_frame_req* cmd = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    int payload_size = 0;

    payload_size = strlen(args.payload->sval[0]);
    if ((payload_size % 2) != 0)
    {
        mctrl_err("Invalid hex string, length must be a multiple of 2\n");
        return -1;
    }

    payload_size = (payload_size / 2);
    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd) + payload_size);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_send_wake_action_frame_req);
    if (!cmd)
        goto exit;

    cmd->payload_size = payload_size;

    if (hexstr2bin(args.payload->sval[0], cmd->payload, payload_size) < 0)
    {
        mctrl_err("Invalid hex string\n");
        goto exit;
    }

    if (str_to_mac_addr(cmd->dest_addr, args.macaddr->sval[0]) < 0)
    {
        mctrl_err("Invalid MAC address - must be in the format aa:bb:cc:dd:ee:ff\n");
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport,
        MORSE_COMMAND_SEND_WAKE_ACTION_FRAME, cmd_tbuff, rsp_tbuff);

exit:
    if (cmd_tbuff)
        morsectrl_transport_buff_free(cmd_tbuff);

    if (rsp_tbuff)
        morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(wakeaction, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
