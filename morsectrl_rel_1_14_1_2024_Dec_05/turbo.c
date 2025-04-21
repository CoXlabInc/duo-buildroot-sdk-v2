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
#include "utilities.h"

struct PACKED set_turbo_mode
{
    uint32_t aid;
    uint16_t vif_id;
    uint8_t enabled;
};

static struct
{
    struct arg_rex *enable;
} args;

int turbo_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Morse Micro proprietary Turbo mode",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable Morse Micro proprietary Turbo mode"));
    return 0;
}

int turbo(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct set_turbo_mode *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enabled = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_turbo_mode);
    cmd->aid = 0;       /* Unused */
    cmd->vif_id = 0;    /* Unused */
    cmd->enabled = enabled;
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_TURBO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER_DEPRECATED(turbo, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
