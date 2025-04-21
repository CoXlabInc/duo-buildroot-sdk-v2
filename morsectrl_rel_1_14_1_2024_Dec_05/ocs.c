/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "channel.h"
#include "transport/transport.h"
#include "utilities.h"

enum ocs_subcmd
{
    OCS_SUBCMD_CONFIG = 1,
    OCS_SUBCMD_STATUS,
};

struct PACKED command_ocs_config_req
{
    uint32_t operating_channel_freq_hz;
    uint8_t operating_channel_bw_mhz;
    uint8_t primary_channel_bw_mhz;
    uint8_t primary_1mhz_channel_index;
};

struct PACKED command_ocs_req
{
    uint32_t subcmd;
    union
    {
        uint8_t opaque[0];
        struct command_ocs_config_req config;
    };
};

struct PACKED command_ocs_status_cfm
{
    uint8_t running;
};

struct PACKED command_ocs_cfm
{
    uint32_t subcmd;
    union
    {
        uint8_t opaque[0];
        struct command_ocs_status_cfm status;
    };
};

static struct {
    struct arg_rex *command;
} args;

static struct mm_argtable config;
static struct mm_argtable status;

static struct mm_argtable *subcmds[] =
{
    &config, &status
};

static struct {
    struct arg_rem *desc;
} status_args;

static struct {
    struct arg_int *channel_freq_hz;
    struct arg_int *bw_hz;
    struct arg_int *primary_bw;
    struct arg_int *chan_idx;
} config_args;

int ocs_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure/report the Off Channel Scan (OCS) settings",
                    args.command = arg_rex1(NULL, NULL, "(config|status)",
                                            "{config|status}", 0, "OCS subcommand"));
    args.command->hdr.flag |= ARG_STOPPARSE;

    MM_INIT_ARGTABLE(&status, "Report OCS status",
                    status_args.desc = arg_rem(NULL, "Gets OCS values"));

    MM_INIT_ARGTABLE(&config, "Set OCS Config",
                    config_args.channel_freq_hz = arg_int1("c", NULL, "<value>",
                        "Set channel frequnecy (kHz)"),
                    config_args.bw_hz = arg_int1("o", NULL, "<value>",
                        "Operating bandwidth (MHz)"),
                    config_args.primary_bw = arg_int1("p", NULL, "<value>",
                        "Primary bandwidth (MHz)"),
                    config_args.chan_idx = arg_int1("n", NULL, "<value>",
                        "Primary 1 MHz channel index"));
    return 0;
}

int ocs_help(void)
{
    mm_help_argtable("ocs config", &config);
    mm_help_argtable("ocs status", &status);
    return 0;
}

static int ocs_cmd_config(int argc, char *argv[], struct command_ocs_req *req)
{
    uint32_t freq, operating_bw, primary_bw, chan_idx;

    int ret = mm_parse_argtable("ocs config", &config, argc, argv);
    if (ret != 0)
    {
        return -1;
    }

    memset(req, 0, sizeof(*req));
    req->subcmd = OCS_SUBCMD_CONFIG;

    freq = config_args.channel_freq_hz->ival[0];
    req->config.operating_channel_freq_hz = htole32(KHZ_TO_HZ(freq));

    operating_bw = config_args.bw_hz->ival[0];
    req->config.operating_channel_bw_mhz = operating_bw;

    primary_bw = config_args.primary_bw->ival[0];
    req->config.primary_channel_bw_mhz = primary_bw;

    chan_idx = config_args.chan_idx->ival[0];
    req->config.primary_1mhz_channel_index = chan_idx;

    return 0;
}

static int ocs_cmd_status(int argc, char *argv[], struct command_ocs_req *req)
{
    int ret = mm_parse_argtable("ocs status", &status, argc, argv);
    if (ret != 0)
    {
        return -1;
    }

    req->subcmd = OCS_SUBCMD_STATUS;

    return 0;
}

static void ocs_cfm_status(struct command_ocs_cfm *cfm)
{
    mctrl_print("%s: running %u\n", __func__, cfm->status.running);
}

int ocs(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *resp_tbuff;
    struct command_ocs_req *req;
    struct command_ocs_cfm *cfm;
    uint32_t subcmd;

    if (0 == strcmp(args.command->sval[0], "config"))
    {
        subcmd = htole32(OCS_SUBCMD_CONFIG);
    }
    else if (0 == strcmp(args.command->sval[0], "status"))
    {
        subcmd = htole32(OCS_SUBCMD_STATUS);
    }
    else
    {
        mctrl_err("Invalid subcommand for morsectrl ocs\n");
        return -EINVAL;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*req));
    resp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm));
    if (!cmd_tbuff || !resp_tbuff)
    {
        ret = -ENOMEM;
        goto out;
    }

    req = TBUFF_TO_CMD(cmd_tbuff, struct command_ocs_req);
    cfm = TBUFF_TO_RSP(resp_tbuff, struct command_ocs_cfm);

    switch (subcmd)
    {
        case OCS_SUBCMD_CONFIG:
            ret = ocs_cmd_config(argc, argv, req);
            break;
        case OCS_SUBCMD_STATUS:
            ret = ocs_cmd_status(argc, argv, req);
            break;
    }

    if (ret != 0)
    {
        goto out;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_OCS_REQ, cmd_tbuff, resp_tbuff);
    if (ret < 0)
    {
        goto out;
    }

    switch (cfm->subcmd)
    {
        case OCS_SUBCMD_CONFIG:
            /* Nothing to do */
            break;
        case OCS_SUBCMD_STATUS:
            ocs_cfm_status(cfm);
            break;
        default:
            /* Should not be possible */
            break;
    }

out:
    /* Check if the reason we got here is because --help was given */
    if (mm_check_help_argtable(subcmds, MORSE_ARRAY_SIZE(subcmds)))
    {
        ret = 0;
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(resp_tbuff);

    for (int i = 0; i < MORSE_ARRAY_SIZE(subcmds); i++)
    {
        mm_free_argtable(subcmds[i]);
    }
    return ret;
}

MM_CLI_HANDLER_CUSTOM_HELP(ocs, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
