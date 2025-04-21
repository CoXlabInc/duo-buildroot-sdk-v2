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

struct PACKED set_ndp_probe_support
{
    uint8_t enabled;
    uint8_t requested_response_is_pv1;
    int8_t tx_bw_mhz;
};

static struct
{
    struct arg_rex *enable;
    struct arg_int *probe_mode;
    struct arg_rex *bandwidth;
} args;

int ndpprobe_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure NDP probe request mode",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable sending probe requests as NDP"),
        args.probe_mode = arg_rint0("r", NULL, "{0|1}", 0, 1,
            "Set probe response reply type (0: PV0, 1: PV1)"),
        args.bandwidth = arg_rex0("b", NULL, "(-1|1|2)", "{-1|1|2}", 0,
            "TX bandwidth in MHz, -1 to use default"));
    return 0;
}

int ndpprobe(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_ndp_probe_support *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int8_t tmp;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ndp_probe_support);

    cmd->enabled = expression_to_int(args.enable->sval[0]);
    cmd->tx_bw_mhz = -1;
    cmd->requested_response_is_pv1 = 0;

    if (cmd->enabled)
    {
        if (args.probe_mode->count)
        {
            cmd->requested_response_is_pv1 = args.probe_mode->ival[0];
        }

        if (args.bandwidth->count)
        {
            str_to_int8((char *)args.bandwidth->sval[0], &tmp);
            cmd->tx_bw_mhz = tmp;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_NDP_PROBE_SUPPORT,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ndpprobe, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
