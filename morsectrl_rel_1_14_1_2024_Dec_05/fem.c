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

struct PACKED set_fem_settings_command
{
    /** \ref enum fem_antenna */
    uint32_t tx_antenna;
    /** \ref enum fem_antenna */
    uint32_t rx_antenna;
    /** Bool, 1=enabled, 0=disabled */
    uint32_t lna_enabled;
    /** Bool, 1=enabled, 0=disabled */
    uint32_t pa_enabled;
};

static struct {
    struct arg_int *tx_antenna;
    struct arg_int *rx_antenna;
    struct arg_int *lna_enable;
    struct arg_int *pa_enable;
} args;

int fem_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure the FEM",
        args.tx_antenna = arg_rint1(NULL, NULL, "<TX antenna>", 0, 2,
            "TX antenna select (0 for auto, 1 for antenna 1...)"),
        args.rx_antenna = arg_rint1(NULL, NULL, "<RX antenna>", 0, 2,
            "RX antenna select (0 for auto, 1 for antenna 1...)"),
        args.lna_enable = arg_rint1(NULL, NULL, "<LNA enable>", 0, 1, "Enable RX LNA"),
        args.pa_enable = arg_rint1(NULL, NULL, "<PA enable>", 0, 1, "Enable TX PA"));
    return 0;
}

int fem(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t tx_antenna, rx_antenna, lna_enabled, pa_enabled;
    struct set_fem_settings_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_fem_settings_command);

    tx_antenna = args.tx_antenna->ival[0];
    rx_antenna = args.rx_antenna->ival[0];
    lna_enabled = args.lna_enable->ival[0];
    pa_enabled = args.pa_enable->ival[0];

    cmd->tx_antenna = htole32(tx_antenna);
    cmd->rx_antenna = htole32(rx_antenna);
    cmd->lna_enabled = htole32(lna_enabled);
    cmd->pa_enabled = htole32(pa_enabled);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_FEM_SETTINGS,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(fem, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
