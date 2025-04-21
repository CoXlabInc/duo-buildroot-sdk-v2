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

struct PACKED command_tdc_pg_disable
{
    /* 0 sets LNA as default, 1 sets LNA in bypass mode */
    uint8_t tdc_pg_disable;
};

static struct
{
    struct arg_rex *enable;
} args;

int tdc_pg_disable_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable TDC clock gating",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable TDC clock gating"));
    return 0;
}

int tdc_pg_disable(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int8_t tdc_pg_disable;
    struct command_tdc_pg_disable *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    tdc_pg_disable = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_tdc_pg_disable);
    cmd->tdc_pg_disable = tdc_pg_disable;

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TDC_PG_DISABLE,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tdc_pg_disable, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
