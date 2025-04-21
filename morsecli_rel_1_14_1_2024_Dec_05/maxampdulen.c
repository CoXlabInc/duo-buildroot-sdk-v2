/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"


struct PACKED set_max_ampdu_length_command
{
    int32_t n_bytes;
};

static struct {
    struct arg_int *bytes;
    struct arg_lit *reset;
} args;

int maxampdulen_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the max A-MPDU length",
        args.bytes = arg_int0(NULL, NULL, "<bytes>", "Maximum allowable A-MPDU length in bytes"),
        args.reset = arg_lit0("r", NULL, "Reset to chip default"));
    return 0;
}

int maxampdulen(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int n_bytes = 0;
    struct set_max_ampdu_length_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    if (args.bytes->count)
    {
        n_bytes = args.bytes->ival[0];
    }
    else if (args.reset->count)
    {
        n_bytes = -1;
    }
    else
    {
        mm_print_missing_argument(&args.bytes->hdr);
        mm_print_missing_argument(&args.reset->hdr);
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_max_ampdu_length_command);
    cmd->n_bytes = htole32(n_bytes);

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_MAX_AMPDU_LENGTH,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(maxampdulen, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
