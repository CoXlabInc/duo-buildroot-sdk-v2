/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

struct PACKED transmit_cw_command
{
    /** The flags of this message */
    int32_t start;
    int32_t tone_frequency_hz;
    int32_t power_dbm;
};

static struct
{
    struct arg_rex *enable;
    struct arg_int *frequency;
    struct arg_int *power_dbm;
} args;

int transmit_cw_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Start or stop continuous wave transmission",
    args.enable = arg_rex1(NULL, NULL, "(start|stop)", "{start|stop}", 0,
        "Start or stop continuous wave transmission"),
    args.frequency = arg_int0(NULL, NULL, "<tone freq>",
        "Transmission frequency in Hz. Possible frequencies:"),
    arg_rem(NULL, "OFDM tones, other frequencies that are an integer multiple of (BW / 4000)"),
    args.power_dbm = arg_int0(NULL, NULL, "<power>", "Transmission power in dBm"));
    return 0;
}

int transmit_cw(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int32_t tone_frequency_hz;
    int32_t power_dbm;
    int32_t start;
    struct transmit_cw_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff = 0;
    struct morsectrl_transport_buff *rsp_tbuff = 0;

    if (strcmp("start", args.enable->sval[0]) == 0)
    {
        if (!(args.frequency->count))
        {
            mctrl_err("Frequency argument required to enable continuous wave transmission\n");
            goto exit;
        }

        if (!(args.power_dbm->count))
        {
            mctrl_err("Power argument required to enable continuous wave transmission\n");
            goto exit;
        }

        start = 1;
        tone_frequency_hz = args.frequency->ival[0];
        power_dbm = args.power_dbm->ival[0];
    }
    else if (strcmp("stop", args.enable->sval[0]) == 0)
    {
        start = 0;
        tone_frequency_hz = 0;
        power_dbm = 0;
    }
    else
    {
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct transmit_cw_command);
    cmd->start = htole32(start);
    cmd->tone_frequency_hz = htole32(tone_frequency_hz);
    cmd->power_dbm = htole32(power_dbm);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TRANSMIT_CW,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(transmit_cw, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
