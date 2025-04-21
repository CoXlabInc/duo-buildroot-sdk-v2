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

struct PACKED set_txop_command
{
    /** The flags of this message */
    uint8_t txop;
};

static struct
{
    struct arg_int *packets;
} args;

int txop_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the TXOP threshold",
        args.packets = arg_rint1(NULL, NULL, "{0-10}", 0, 10,
            "Minimum packets to start TXOP (0 to disable)"));
    return 0;
}

int txop(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t txop;
    struct set_txop_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_txop_command);

    txop = args.packets->ival[0];

    cmd->txop = htole32(txop);
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_TXOP,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(txop, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
