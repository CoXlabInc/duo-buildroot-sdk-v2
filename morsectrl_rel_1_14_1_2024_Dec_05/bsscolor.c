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

struct PACKED set_bss_color
{
    /** The BSS color */
    uint8_t bss_color;
};

static struct
{
    struct arg_int *bsscolor;
} args;

int bsscolor_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set BSS color",
        args.bsscolor = arg_rint1(NULL, NULL, "<color>", 0, 7, "BSS color (0-7)"));
    return 0;
}

int bsscolor(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t color;
    struct set_bss_color *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(0));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_bss_color);

    color = args.bsscolor->ival[0];

    cmd->bss_color = htole32(color);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_BSS_COLOR,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(bsscolor, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
