/*
 * Copyright 2022 Morse Micro
 *
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

struct PACKED command_energy_detection_mode_req
{
    /**
     * Mode for energy detection:
     * 0:auto - The default. System will automatically sense the medium and set the energy
     *          threshold / noise estimate.
     * 1:static - Set a static value for the specified parameter. The variable `value` must
     *            be set in this scenario.
     * 2:ignore - Only valid when param == 0 (energy threshold)
     *            Energy on air will be ignored when device goes to transmit.
     * 3:jammer - Only valid when param == 0 (energy threshold)
     *            Energy on air will be ignored if in-channel jammer is detected.
     */
    uint16_t mode;

    /**
     * Energy detect parameter to target
     * 0: energy threshold
     * 1: noise estimate
     */
    uint8_t param;

    /** 0 : value is in dbm
     *  1 : value is linear
     */
    uint8_t linear;

    /** If mode is static (1), then this will define the value to assign to specified parameter.
     *  Must evaluate to a positive linear value (after dBm conversion if applicable)
    */
    int16_t value;
};

static struct
{
    struct arg_rex *target;
    struct arg_rex *command;
    struct arg_rex *static_mode;
    struct arg_int *threshold;
} args;

int edconfig_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure energy detect/noise estimation",
        args.target = arg_rex1(NULL, NULL, "(energy|noise)", "{energy|noise}", 0,
            "Configure energy detect threshold or noise estimate"),
        args.command = arg_rex1(NULL, NULL, "(automatic|static|ignore|jammer)",
            "{automatic|static|ignore|jammer}", 0, "Mode"),
        args.static_mode = arg_rex0(NULL, NULL, "(dbm|linear)", "{dbm|linear}", 0,
            "Integer dBm or linear scale (only valid for static mode)"),
        args.threshold = arg_int0("t", "threshold", "<threshold>",
            "Threshold value (only valid for static mode)"));
    return 0;
}

int edconfig(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_energy_detection_mode_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_energy_detection_mode_req);

    if (strcmp("energy", args.target->sval[0]) == 0)
    {
        cmd->param = 0;
    }
    else if (strcmp("noise", args.target->sval[0]) == 0)
    {
        cmd->param = 1;
    }
    else
    {
        mctrl_err("Invalid target\n");
        ret = -1;
        goto exit;
    }

    // mode checks
    if (strcmp("automatic", args.command->sval[0]) == 0)
    {
        cmd->mode = 0;
    }
    else if (strcmp("ignore", args.command->sval[0]) == 0)
    {
        cmd->mode = 2;
    }
    else if (strcmp("jammer", args.command->sval[0]) == 0)
    {
        cmd->mode = 3;
    }
    else if (strcmp("static", args.command->sval[0]) == 0)
    {
        if (args.static_mode->count <= 0 && args.threshold->count <= 0)
        {
            mctrl_err("Not enough arguments\n");
            ret = -1;
            goto exit;
        }

        if (strcmp("dbm", args.static_mode->sval[0]) == 0)
        {
            cmd->linear = 0;
        }
        else if (strcmp("linear", args.static_mode->sval[0]) == 0)
        {
            cmd->linear = 1;
        }
        else
        {
            mctrl_err("Invalid static threshold type (specify either 'dbm' or 'linear'\n");
            ret = -1;
            goto exit;
        }

        cmd->mode = 1;
        if (args.threshold->count == 0)
        {
            mm_print_missing_argument(&args.threshold->hdr);
            ret = -1;
            goto exit;
        }
        cmd->value = args.threshold->ival[0];
    }
    else
    {
        mctrl_err("Invalid mode\n");
        ret = -1;
        goto exit;
    }

    // good to go
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_ENERGY_DETECTION_MODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(edconfig, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
