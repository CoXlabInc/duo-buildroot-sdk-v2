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

struct PACKED set_antenna_command
{
    /** \ref enum fem_antenna */
    uint32_t tx_antenna;
    /** \ref enum fem_antenna */
    uint32_t rx_antenna;
};

static struct
{
    struct arg_int *tx_antenna;
    struct arg_int *rx_antenna;
} args;

int antenna_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Select TX and RX antenna",
        args.tx_antenna = arg_rint1(NULL, NULL, "<TX antenna>", 1, 2,
            "TX antenna selection (1-2)"),
        args.rx_antenna = arg_rint1(NULL, NULL, "<RX antenna>", 1, 2,
            "RX antenna selection (1-2)"));
    return 0;
}

int antenna(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t tx_antenna, rx_antenna;
    struct set_antenna_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    tx_antenna = args.tx_antenna->ival[0];
    rx_antenna = args.rx_antenna->ival[0];

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_antenna_command);
    cmd->tx_antenna = htole32(tx_antenna);
    cmd->rx_antenna = htole32(rx_antenna);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ANTENNA,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(antenna, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
