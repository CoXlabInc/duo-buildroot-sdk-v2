/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>

#include "command.h"

int hwkeydump_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Get firmware to dump hardware encryption keys to UART");
    return 0;
}

int hwkeydump(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_DUMP_HW_KEYS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(hwkeydump, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
