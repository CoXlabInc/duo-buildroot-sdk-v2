/*
 * Copyright 2022 Morse Micro
 */

#include <stdlib.h>
#include <stdio.h>

#include "command.h"
#include "utilities.h"

/* Lifetime packet expiry in us */
#define TX_PACKET_EXPIRY_MIN_US 50000
#define TX_PACKET_EXPIRY_MAX_US 500000

struct PACKED set_tx_pkt_lifetime_us_command
{
    uint32_t lifetime_us;
};

static struct
{
    struct arg_int *lifetime;
} args;

int tx_pkt_lifetime_us_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set the TX packet lifetime expiry",
        args.lifetime = arg_rint1(NULL, NULL, NULL, TX_PACKET_EXPIRY_MIN_US,
        TX_PACKET_EXPIRY_MAX_US, "TX packet expiry (usecs): "
        STR(TX_PACKET_EXPIRY_MIN_US) "-" STR(TX_PACKET_EXPIRY_MAX_US)));
    return 0;
}

int tx_pkt_lifetime_us(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct set_tx_pkt_lifetime_us_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint32_t lifetime_us;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_tx_pkt_lifetime_us_command);

    lifetime_us = args.lifetime->ival[0];
    cmd->lifetime_us = htole32(lifetime_us);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_TX_PKT_LIFETIME_US,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tx_pkt_lifetime_us, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
