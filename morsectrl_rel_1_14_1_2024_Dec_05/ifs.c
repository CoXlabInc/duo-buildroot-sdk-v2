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

#define IF_SPACING_MIN_US 160

struct PACKED set_ifs_command
{
    /** The flags of this message */
    uint32_t ifs;
};

static struct {
    struct arg_int *ifs;
} args;

int ifs_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the interframe spacing",
        args.ifs = arg_rint1(NULL, NULL, "<ifs>", IF_SPACING_MIN_US, INT32_MAX,
            "Interframe spacing in usecs (min: 160)"));
    return 0;
}

int ifs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t ifs;
    struct set_ifs_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ifs_command);
    ifs = args.ifs->ival[0];

    cmd->ifs = htole32(ifs);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_IFS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ifs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
