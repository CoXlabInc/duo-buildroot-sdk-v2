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

#define MAX_NSS 4

struct PACKED set_transmission_rate
{
    int32_t mcs_index;
    int32_t bandwidth_mhz;
    int32_t tx_80211ah_format;
    int8_t use_traveling_pilots;
    int8_t use_sgi;
    uint8_t enabled;
    int8_t nss_idx;
    int8_t use_ldpc;
    int8_t use_stbc;
};

static struct
{
    struct arg_int *mcs;
    struct arg_rex *bw_mhz;
    struct arg_int *tx_format;
    struct arg_int *traveling_pilots;
    struct arg_int *use_sgi;
    struct arg_int *nss_idx;
    struct arg_int *use_ldpc;
    struct arg_int *use_stbc;
    struct arg_rex *enabled;
} args;

int txrate_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Fix packet transmission rate parameters in firmware",
        args.enabled = arg_rex1(NULL, NULL, "enable|disable", "{enable|disable}", 0,
            "Enable/disable fixed transmission rate parameters"),
        args.mcs = arg_rint0("m", NULL, "{-1|0-10}", -1, 10,
                "MCS index (0-10) or -1 for default"),
        args.bw_mhz = arg_rex0("b", NULL, "(1|2|4|8|-1)", "{1|2|4|8|-1}", 0,
                "TX bandwidth in MHz, or -1 for default"),
        args.tx_format = arg_rint0("f", NULL, "{-1|[0-2]}", -1, 2,
                "Duplicate format (0, 1, 2) or -1 to use default"),
        args.traveling_pilots = arg_rint0("t", NULL, "{-1|[0-1]}", -1, 1,
                "Traveling pilots (0, 1) or -1 to use default"),
        args.use_sgi = arg_rint0("s", NULL, "{-1|[0-1]}", -1, 1,
                "Short guard interval (0, 1) or -1 to use default"),
        args.nss_idx = arg_rint0("n", NULL, "{-1|[0-" STR(MAX_NSS) "]}", -1, MAX_NSS,
                "Spatial streams (1-" STR(MAX_NSS) ") or -1 to use default"),
        args.use_ldpc = arg_rint0("l", NULL, "{-1|[0-1]}", -1, 1,
                "LDPC (0, 1) or -1 to use default"),
        args.use_stbc = arg_rint0("c", NULL, "{-1|[0-1]}", -1, 1,
                "STBC (0, 1) or -1 to use default"));
    return 0;
}

int txrate(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_transmission_rate *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t tmp;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_transmission_rate);
    cmd->mcs_index = -1;
    cmd->bandwidth_mhz = -1;
    cmd->tx_80211ah_format = -1;
    cmd->use_traveling_pilots = -1;
    cmd->use_sgi = -1;
    cmd->nss_idx = -1;
    cmd->use_ldpc = -1;
    cmd->use_stbc = -1;
    cmd->enabled = expression_to_int(args.enabled->sval[0]) ? 1 : 0;

    if (args.mcs->count)
    {
        cmd->mcs_index = htole32(args.mcs->ival[0]);
    }

    if (args.bw_mhz->count)
    {
        if (str_to_int32(args.bw_mhz->sval[0], &tmp))
        {
            mctrl_err("Invalid bandwidth\n");
            ret = -1;
            goto exit;
        }
        cmd->bandwidth_mhz = htole32(tmp);
    }

    if (args.tx_format->count)
    {
        cmd->tx_80211ah_format = htole32(args.tx_format->ival[0]);
    }

    if (args.traveling_pilots->count)
    {
        cmd->use_traveling_pilots = htole32(args.traveling_pilots->ival[0]);
    }

    if (args.use_sgi->count)
    {
        cmd->use_sgi = htole32(args.use_sgi->ival[0]);
    }

    if (args.nss_idx->count)
    {
        /* Support default (-1) or 1 to MAX_NSS spatial streams. */
        int8_t nss = args.nss_idx->ival[0];
        if (nss == 0)
        {
            mctrl_err("Invalid spatial streams parameter: %d\n", nss);
        }

        cmd->nss_idx = (nss == -1) ? htole32(-1) : htole32(NSS_TO_NSS_IDX(nss));
    }

    if (args.use_ldpc->count)
    {
        cmd->use_ldpc = htole32(args.use_ldpc->ival[0]);
    }

    if (args.use_stbc->count)
    {
        cmd->use_stbc = htole32(args.use_stbc->ival[0]);
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_TRANSMISSION_RATE,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        goto exit;
    }

    if (cmd->enabled)
    {
        mctrl_print("Set the following transmission rate parameters:\n");

        if (cmd->mcs_index != -1)
            mctrl_print("\tMCS index: %d\n", cmd->mcs_index);
        if (cmd->bandwidth_mhz != -1)
            mctrl_print("\tTX Channel BW: %d (MHz)\n", cmd->bandwidth_mhz);
        if (cmd->tx_80211ah_format != -1)
            mctrl_print("\tTX format: %d\n", cmd->tx_80211ah_format);
        if (cmd->use_traveling_pilots != -1)
            mctrl_print("\tUse Traveling pilots: %d\n", cmd->use_traveling_pilots);
        if (cmd->use_sgi != -1)
            mctrl_print("\tUse Short Guard Interval: %d\n", cmd->use_sgi);
        if (cmd->nss_idx != -1)
            mctrl_print("\tSpatial Streams: %d\n", NSS_IDX_TO_NSS(cmd->nss_idx));
        if (cmd->use_ldpc != -1)
            mctrl_print("\tUse LDPC: %d\n", cmd->use_ldpc);
        if (cmd->use_stbc != -1)
            mctrl_print("\tUse STBC: %d\n", cmd->use_stbc);
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(txrate, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
