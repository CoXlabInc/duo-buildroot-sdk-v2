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

/** Calibration types available in the system, values can be or'ed together to create a mask */
typedef enum
{
    /** Temperature calibration */
    CALIBRATION_TEMPERATURE = (1 << 0),
    /** VBAT calibration */
    CALIBRATION_VBAT = (1 << 1),
    /** AON clock calibration */
    CALIBRATION_AON_CLK = (1 << 2),
    /** DC calibration */
    CALIBRATION_DC = (1 << 3),
    /** I/Q calibration */
    CALIBRATION_IQ = (1 << 4),

    /** Special calibration type for testing purposes */
    CALIBRATION_SPOOF_TEST = (1 << 30),

    /** Last / MAX */
    CALIBRATION_LAST = (CALIBRATION_TEMPERATURE | CALIBRATION_VBAT |
                        CALIBRATION_AON_CLK | CALIBRATION_DC |
                        CALIBRATION_IQ | CALIBRATION_SPOOF_TEST),
    CALIBRATION_MAX = UINT32_MAX
} calibration_type_t;

struct PACKED set_periodic_cal_cmd
{
    calibration_type_t periodic_cal_enabled;
};

static struct {
    struct arg_str *mask;
} args;

int periodic_cal_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(
        mm_args, "Enable/disable periodic calibrations on-chip",
        args.mask = arg_str1(NULL, NULL, "<enable mask>",
        "This is a one-shot enable/disable for all cals - overwrites all current config"),
        arg_rem(NULL, "0x10 - IQ"),
        arg_rem(NULL, "0x08 - DC"),
        arg_rem(NULL, "0x04 - AON_CLK"),
        arg_rem(NULL, "0x02 - VBAT"),
        arg_rem(NULL, "0x01 - TEMP"));
    return 0;
}

int periodic_cal(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enable_mask;
    char *ptr;
    struct set_periodic_cal_cmd *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enable_mask = strtol(args.mask->sval[0] + 2, &ptr, 16);

    if (enable_mask == -1)
    {
        mctrl_err("Invalid command parameters\n");
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_periodic_cal_cmd);
    cmd->periodic_cal_enabled = enable_mask;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_PERIODIC_CAL,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER_DEPRECATED(periodic_cal, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
