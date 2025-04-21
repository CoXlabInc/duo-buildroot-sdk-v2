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
#include "utilities.h"

struct PACKED set_control_response_command
{
    uint8_t direction;
    uint8_t control_response_bw_mhz;
};

static struct {
    struct arg_int *direction;
    struct arg_rex *bandwidth;
} args;

int cr_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Force control response frames to use the specified bandwidth",
            args.direction = arg_rint1(NULL, NULL, "<direction>", 0, 1,
                "Apply to outbound (0) or inbound (1)"),
            args.bandwidth = arg_rex1(NULL, NULL, "(0|1|2|4|8)", "<bw>", 0,
                "Bandwidth (MHz), or 0 to disable"));
    return 0;
}

int cr(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t bw_mhz;
    uint8_t direction;
    struct set_control_response_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_control_response_command);

    direction = args.direction->ival[0];

    if (str_to_uint8((char *)args.bandwidth->sval[0], &bw_mhz))
    {
        goto exit;
    }

    cmd->control_response_bw_mhz = bw_mhz;
    cmd->direction = direction;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CONTROL_RESPONSE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(cr, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
