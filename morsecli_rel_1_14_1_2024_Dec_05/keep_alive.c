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

struct PACKED command_set_keep_alive_offload
{
    /** The value of the BSS max idle period as it appears in the IE */
    uint16_t bss_max_idle_period;
    /** Set to TRUE to interpret the value of BSS max idle period as per 11ah spec */
    uint8_t interpret_as_11ah;
};

static struct
{
    struct arg_int *idle_period;
    struct arg_lit *dot11_spec;
} args;

int keepalive_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the BSS max idle period",
        args.idle_period = arg_int1(NULL, NULL, "<period>",
            "BSS idle period (1000 TUs) after which a keepalive will be sent"),
        args.dot11_spec = arg_lit0("a", NULL, "Interpret idle period as per IEEE802.11ah spec"));
    return 0;
}

int keepalive(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_set_keep_alive_offload *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint16_t bss_max_idle_period = args.idle_period->ival[0];

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_keep_alive_offload);

    cmd->interpret_as_11ah = (args.dot11_spec->count > 0);

    cmd->bss_max_idle_period = htole16(bss_max_idle_period);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_KEEP_ALIVE_OFFLOAD,
        cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(keepalive, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
