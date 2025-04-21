/*
 * Copyright 2022 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

/** No upper bound value for airtime duration */
#define AIRTIME_UNLIMITED 0

#define NUM_BOUNDS_VALUES 2

#define SET_MPSW_CFG_AIRTIME_BOUNDS  BIT(0)
#define SET_MPSW_CFG_PKT_SPC_WIN_LEN BIT(1)
#define SET_MPSW_CFG_ENABLED         BIT(2)

struct PACKED mpsw_configuration
{
    /** The maximum allowable packet airtime duration */
    uint32_t airtime_max_us;
    /** The minimum packet airtime duration to trigger spacing */
    uint32_t airtime_min_us;
    /** The length of time to close the tx window between packets */
    uint32_t packet_space_window_length_us;
    /** Whether to enable airtime bounds checking and packet spacing enforcement */
    uint8_t  enable;
};

struct PACKED command_mpsw_cfg_req
{
    struct mpsw_configuration config;
    uint8_t set_cfgs;
};

struct PACKED command_mpsw_cfg_cfm
{
    struct mpsw_configuration config;
};

static struct {
    struct arg_csi *bounds;
    struct arg_int *len;
    struct arg_rex *enable;
} args;

int mpsw_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(
        mm_args, "Get (default) or set Minimum Packet Spacing Window parameters",
        args.bounds = arg_csi0("b", NULL, "<low usecs>,<high usecs>", NUM_BOUNDS_VALUES,
            "Min required/max allowable packet airtime duration to trigger spacing"),
        args.len = arg_int0("w", NULL, "<length>",
            "Length of time to close the TX window between packets"),
        args.enable = arg_rex0("e", NULL, "(0|1)", "{0|1}", 0,
            "Enable airtime bounds checking and packet spacing enforcement"));
    return 0;
}

static void print_mpsw_cfg(struct mpsw_configuration *cfg)
{
    mctrl_print("                 MPSW Active: %d\n", cfg->enable);
    mctrl_print("       Airtime Minimum Bound: %u\n", cfg->airtime_min_us);
    mctrl_print("       Airtime Maximum Bound: %u\n", cfg->airtime_max_us);
    mctrl_print("Packet Spacing Window Length: %u\n", cfg->packet_space_window_length_us);
}

int mpsw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;

    struct command_mpsw_cfg_req *cmd_mpsw;
    struct command_mpsw_cfg_cfm *rsp_mpsw;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_mpsw));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_mpsw));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -1;
        goto exit;
    }

    cmd_mpsw = TBUFF_TO_CMD(cmd_tbuff, struct command_mpsw_cfg_req);
    rsp_mpsw = TBUFF_TO_RSP(rsp_tbuff, struct command_mpsw_cfg_cfm);

    if (cmd_mpsw == NULL ||
        rsp_mpsw == NULL)
    {
        goto exit;
    }

    memset(cmd_mpsw, 0, sizeof(*cmd_mpsw));

    if (args.bounds->count)
    {
        cmd_mpsw->config.airtime_min_us = args.bounds->ival[0][0];
        cmd_mpsw->config.airtime_max_us = args.bounds->ival[0][1];

        if (((cmd_mpsw->config.airtime_min_us > cmd_mpsw->config.airtime_max_us) &&
             (cmd_mpsw->config.airtime_max_us != AIRTIME_UNLIMITED)) ||
             (cmd_mpsw->config.airtime_min_us == cmd_mpsw->config.airtime_max_us))
        {
            mctrl_err(
                "airtime min (%d) must be less than airtime max (%d), or airtime max must be %d\n",
                cmd_mpsw->config.airtime_min_us, cmd_mpsw->config.airtime_max_us,
                AIRTIME_UNLIMITED);
            goto exit;
        }

        cmd_mpsw->set_cfgs |= SET_MPSW_CFG_AIRTIME_BOUNDS;
        cmd_mpsw->config.airtime_min_us = htole32(cmd_mpsw->config.airtime_min_us);
        cmd_mpsw->config.airtime_max_us = htole32(cmd_mpsw->config.airtime_max_us);
    }

    if (args.len->count)
    {
        cmd_mpsw->set_cfgs |= SET_MPSW_CFG_PKT_SPC_WIN_LEN;
        cmd_mpsw->config.packet_space_window_length_us = htole32(args.len->ival[0]);
    }

    if (args.enable->count)
    {
        cmd_mpsw->set_cfgs |= SET_MPSW_CFG_ENABLED;
        cmd_mpsw->config.enable = expression_to_int(args.enable->sval[0]);
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_COMMAND_MPSW_CONFIG,
                                 cmd_tbuff,
                                 rsp_tbuff);

exit:
    if (!ret)
    {
        print_mpsw_cfg(&rsp_mpsw->config);
    }

    morsectrl_transport_buff_free(cmd_tbuff);

    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(mpsw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
