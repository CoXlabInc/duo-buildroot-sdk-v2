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

struct PACKED set_bcn_rssi_threshold_command
{
    /** The threshold in dB */
    uint8_t threshold_db;
};


static struct
{
    struct arg_int *bcn_threshold;
} args;

int bcn_rssi_threshold_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the beacon RSSI change threshold to trigger reporting to host",
        args.bcn_threshold = arg_rint1(NULL, NULL, "<value>", 0, 100, "Threshold in dB (0-100)"));
    return 0;
}

int bcn_rssi_threshold(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_bcn_rssi_threshold_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_bcn_rssi_threshold_command);

    cmd->threshold_db = args.bcn_threshold->ival[0];
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_BCN_RSSI_THRESHOLD,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(bcn_rssi_threshold, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
