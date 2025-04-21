/*
 * Copyright 2023 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

#define RSSI_MARGIN_MIN 3
#define RSSI_MARGIN_MAX 30
#define BLACKLIST_TIMEOUT_MIN 10
#define BLACKLIST_TIMEOUT_MAX 600

struct PACKED command_set_dynamic_peering_conf {
    /** Enable or disable mesh dynamic peering */
    uint8_t enabled;

    /** RSSI margin to consider while selecting a peer to kick out */
    uint8_t rssi_margin;

    /** Kicked out peer is not allowed connection during this period */
    uint32_t blacklist_timeout;
};

static struct {
    struct arg_rex *enable;
    struct arg_int *rssi_margin;
    struct arg_int *timeout;
    struct arg_rem *note;
    struct arg_rem *note2;
    struct arg_rem *note3;
} args;

int dynamic_peering_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure Mesh Dynamic Peering",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable Mesh Dynamic Peering"),
        args.note = arg_rem(NULL, "Do not use - for internal use by wpa_supplicant"),
        args.rssi_margin = arg_int0("r", NULL, "<RSSI margin>",
            "RSSI margin (dBm) to consider while selecting a peer to kick out."),
        args.note2 = arg_rem(NULL, "(min:"STR(RSSI_MARGIN_MIN)
            ", max:"STR(RSSI_MARGIN_MAX)")"),
        args.timeout = arg_int0("t", NULL, "<blacklist timeout>",
            "Blacklist time for a kicked-out peer (secs)"),
        args.note3 = arg_rem(NULL, "(min:"STR(BLACKLIST_TIMEOUT_MIN)
            ", max:"STR(BLACKLIST_TIMEOUT_MAX)")"));
    return 0;
}

int dynamic_peering(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_dynamic_peering_conf *dyn_peering_conf = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    uint8_t temp = 0;
    uint32_t timeout = 0;
    int enabled = -1;

    if (!(args.enable->count))
    {
        /* It shouldn't be possible to get here due to rex1 requiring min. one match */
        mctrl_err("Invalid argument, provide either enable -r <rssi_margin> -t <blacklist timeout> "
                    "or disable\n");
        goto exit;
    }

    if (strcmp("enable", args.enable->sval[0]) == 0)
    {
        if (args.rssi_margin->count && args.timeout->count)
        {
            enabled = true;
            temp = args.rssi_margin->ival[0];
            timeout = args.timeout->ival[0];

            if (temp < RSSI_MARGIN_MIN || temp > RSSI_MARGIN_MAX)
            {
                mctrl_err("RSSI margin %u must be within the range min %u : max %u\n",
                    temp, RSSI_MARGIN_MIN, RSSI_MARGIN_MAX);
                goto exit;
            }
            if (timeout < BLACKLIST_TIMEOUT_MIN || timeout > BLACKLIST_TIMEOUT_MAX)
            {
                mctrl_err("Blacklist timeout %u must be within the range min %u : max %u\n",
                    timeout, BLACKLIST_TIMEOUT_MIN, BLACKLIST_TIMEOUT_MAX);
                goto exit;
            }
        }
        else
        {
            if (args.rssi_margin->count <= 0)
            {
                mctrl_err("-r <rssi_margin> required\n");
            }
            if (args.timeout->count <= 0)
            {
                mctrl_err("-t <blacklist timeout> required\n");
            }
            goto exit;
        }
    }
    else if (strcmp("disable", args.enable->sval[0]) == 0)
    {
        if (args.rssi_margin->count || args.timeout->count)
        {
            mctrl_err("Invalid arguments: Try --help for more information");
            goto exit;
        }
        enabled = false;
    }
    else
    {
        mctrl_err("Invalid arguments: Try --help for more information");
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*dyn_peering_conf));
    if (!cmd_tbuff)
    {
        goto exit;
    }

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    if (!rsp_tbuff)
    {
        goto exit;
    }

    dyn_peering_conf = TBUFF_TO_CMD(cmd_tbuff, struct command_set_dynamic_peering_conf);
    memset(dyn_peering_conf, 0, sizeof(*dyn_peering_conf));

    dyn_peering_conf->enabled = enabled;
    dyn_peering_conf->rssi_margin = temp;
    dyn_peering_conf->blacklist_timeout = timeout;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_DYNAMIC_PEERING_SET_CONF,
        cmd_tbuff, rsp_tbuff);

exit:
    if (cmd_tbuff)
    {
        morsectrl_transport_buff_free(cmd_tbuff);
    }

    if (rsp_tbuff)
    {
        morsectrl_transport_buff_free(rsp_tbuff);
    }

    return ret;
}

MM_CLI_HANDLER(dynamic_peering, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
