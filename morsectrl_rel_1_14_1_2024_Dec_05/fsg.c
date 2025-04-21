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

/* Limits on duty cycle, expressed in  percent */
#define FSG_DUTY_CYCLE_MIN      0.01
#define FSG_DUTY_CYCLE_MAX      99.99
#define FSG_DEFAULT_IFS_US      160

struct PACKED command_set_fsg_req
{
    /**
     * The number of rounds that the FSG needs to execute,
     * (-1) infintie, (0) disable, (> 0) finite
     */
    int32_t n_iterations;
    /**
     * The duty cycle to maintain given the previously set transmission parameters.
     * Scaled by 100, (e.g. 98.5% -> 9850)
     */
    uint32_t duty_cycle_scaled;
    /**
     * The nominal inter-frame spacing between PSDUs. Must be (!= 0). A value of (-1)
     * will default to SIFS.
     */
    int32_t ifs_us;
};

static struct
{
    struct arg_rex *enable;
    struct arg_int *iters;
    struct arg_dbl *duty_cycle;
    struct arg_int *ifs;
} args;

int fsg_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Configure the Fast Symbol Generator (FSG)",
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable the FSG"),
        args.iters = arg_int0("i", "iterations", "<iters>",
            "Number of transmissions (-1 for infinite)"),
        args.duty_cycle = arg_dbl0(NULL, NULL, "<duty cycle>",
            "The duty cycle to maintain between transmissions ("
            STR(FSG_DUTY_CYCLE_MIN) "%-" STR(FSG_DUTY_CYCLE_MAX) "%)"),
        args.ifs = arg_int0(NULL, NULL, "<IFS>",
            "Interframe spacing between PSDUs in usecs (default:" STR(FSG_DEFAULT_IFS_US) ")"));
    return 0;
}

int fsg(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_fsg_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_fsg_req);

    if (strcmp(args.enable->sval[0], "enable") == 0)
    {
        if (!args.iters->count)
        {
            mm_print_missing_argument(&args.iters->hdr);
            goto exit;
        }

        if (args.iters->ival[0] == 0)
        {
            mctrl_err("Invalid iteration value (must be non-zero)\n");
            goto exit;
        }

        if (!args.duty_cycle->count)
        {
            mm_print_missing_argument(&args.duty_cycle->hdr);
            goto exit;
        }

        cmd->n_iterations = htole32(args.iters->ival[0]);

        float duty_cycle = args.duty_cycle->dval[0];
        if ((duty_cycle < FSG_DUTY_CYCLE_MIN) || (duty_cycle > FSG_DUTY_CYCLE_MAX))
        {
            mctrl_err("Invalid duty cycle %.2f (%.2f-%.2f)\n",
                      duty_cycle, FSG_DUTY_CYCLE_MIN, FSG_DUTY_CYCLE_MAX);
            goto exit;
        }
        cmd->duty_cycle_scaled = (uint32_t)(duty_cycle * 100);

        if (args.ifs->count)
            cmd->ifs_us = htole32(args.ifs->ival[0]);
        else
            cmd->ifs_us = htole32(FSG_DEFAULT_IFS_US);
    }
    else
    {
        cmd->n_iterations = 0;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_FSG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(fsg, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
