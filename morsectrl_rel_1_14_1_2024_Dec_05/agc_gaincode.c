/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "command.h"
#include "utilities.h"

#define GAINCODE_MIN 0
#define GAINCODE_MAX 20

struct PACKED command_set_agc_gaincode
{
    /* set gain code */
    uint32_t agc_gain_code;
};

static struct
{
    struct arg_rex *gain_code;
} args;

int set_agc_gaincode_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set gain code for testing",
        args.gain_code = arg_rex1(NULL, NULL, "(auto|mask_rssi3|enable_sw_agc|[0-9]+)",
        "{auto|mask_rssi3|enable_sw_agc|0-" STR(GAINCODE_MAX) "}", 0, "Set gain code for testing"),
        arg_rem(NULL, "Use 'auto' to enable AGC"),
        arg_rem(NULL, "Use 'mask_rssi3' to mask AGC RSSI counter 3"),
        arg_rem(NULL, "Use 'enable_sw_agc' to enable SW AGC instead of HW-AGC"));
    return 0;
}

int set_agc_gaincode(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t gain_code;
    struct command_set_agc_gaincode *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    if (!strcmp(args.gain_code->sval[0], "auto"))
    {
       gain_code = UINT8_MAX;
    }
    else if (!strcmp(args.gain_code->sval[0], "mask_rssi3"))
    {
        gain_code = UINT8_MAX - 1;
    }
    else if (!strcmp(args.gain_code->sval[0], "enable_sw_agc"))
    {
        gain_code = UINT8_MAX - 2;
    }
    else if (str_to_uint32_range(args.gain_code->sval[0], &gain_code, GAINCODE_MIN, GAINCODE_MAX))
    {
        mctrl_err("Invalid gain code value '%s', expected range [%d, %d]\n",
                args.gain_code->sval[0], GAINCODE_MIN, GAINCODE_MAX);
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_agc_gaincode);
    cmd->agc_gain_code = gain_code;
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_SET_AGC_GAIN_CODE,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(set_agc_gaincode, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
