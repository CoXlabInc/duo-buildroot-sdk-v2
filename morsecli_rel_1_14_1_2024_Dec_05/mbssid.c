/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#ifndef MORSE_WIN_BUILD
#include <net/if.h>
#endif
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

#define BSS_MIN 0
#define BSS_MAX 2
#define BSS_ID_DEFAULT 0


struct PACKED set_mbssid_ie
{
    /** Maximum supported BSS to be updated in MBSSID IE */
    uint8_t max_bssid_indicator;

    /** Beacon or probe reponse transmitting interface name */
    char transmitter_iface[IFNAMSIZ];
};

static struct {
    struct arg_str *iface;
    struct arg_int *max_supported_bss;
} args;

int mbssid_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Advertise BSS in the beacons of another BSS",
        args.iface = arg_str1("t", NULL, "<transmitting BSS>",
            "Transmitting interface name, e.g. wlan0"),
        args.max_supported_bss = arg_rint1("m", NULL, "<max BSS ID>", BSS_MIN, BSS_MAX,
            "Maximum number of BSSs supported (" STR(BSS_MIN) "-" STR(BSS_MAX) ")"));
    return 0;
}

int mbssid(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t max_bssid_indicator = BSS_ID_DEFAULT;
    struct set_mbssid_ie *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mbssid_ie);

    strncpy(cmd->transmitter_iface, args.iface->sval[0], sizeof(cmd->transmitter_iface) - 1);

    max_bssid_indicator = args.max_supported_bss->ival[0];
    cmd->max_bssid_indicator = max_bssid_indicator;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MBSSID_INFO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mbssid, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
