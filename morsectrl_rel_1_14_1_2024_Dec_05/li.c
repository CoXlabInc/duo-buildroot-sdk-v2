/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"

#define UNSCALED_INTERVAL_MAX               ((2 << 14) - 1)

struct PACKED set_li_command
{
    /** The flags of this message */
    uint16_t li;
};

static struct {
    struct arg_int *unscaled;
    struct arg_int *scale_idx;
} args;

int li_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Set the listen interval on a STA. On an AP, set the max BSS idle period",
        args.unscaled = arg_rint1(NULL, NULL, "<unscaled interval>", 0, UNSCALED_INTERVAL_MAX,
            "Unscaled listen interval"),
        args.scale_idx = arg_rint1(NULL, NULL, "<scale index>", 0, 3,
            "Scale index: 0=1, 1=10, 2=100, 3=1000"));
    return 0;
}

int li(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t unscaled_interval;
    uint8_t scale_idx;
    struct set_li_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_li_command);

    unscaled_interval = args.unscaled->ival[0];
    scale_idx = args.scale_idx->ival[0];

    /* Max is same as mask */
    cmd->li = htole32((unscaled_interval & UNSCALED_INTERVAL_MAX) | scale_idx << 14);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_LISTEN_INTERVAL,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(li, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
