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
#include "utilities.h"

struct PACKED set_response_indication_command
{
    int8_t response_indication;
};

#define RESPONSE_INDICATION_MAX 3

static struct {
    struct arg_rex *enable;
    struct arg_int *value;
    struct arg_rem *note;
} args;

int ri_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Force a specified response indication",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable forced response indication"),
        args.value = arg_rint0(NULL, NULL, "<value>", 0, RESPONSE_INDICATION_MAX,
            "Response indication to force (0-3)"));
    return 0;
}

int ri(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int8_t ri;
    struct set_response_indication_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_response_indication_command);

    if (strcmp("enable", args.enable->sval[0]) == 0)
    {
        if (!args.value->count)
        {
            mm_print_missing_argument(&args.value->hdr);
            ret = -1;
            goto exit;
        }
        ri = args.value->ival[0];
    }
    else if (strcmp("disable", args.enable->sval[0]) == 0)
    {
        ri = -1;
    }
    else
    {
        mctrl_err("Invalid command parameters\n");
        ret = -1;
        goto exit;
    }

    cmd->response_indication = ri;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_RESPONSE_INDICATION,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ri, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
