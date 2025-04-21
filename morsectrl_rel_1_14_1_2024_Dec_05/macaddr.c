/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_mac_addr_req
{
    uint8_t write;
    uint8_t mac_octet[MAC_ADDR_LEN];
};

struct PACKED command_mac_addr_cfm
{
    uint8_t mac_octet[MAC_ADDR_LEN];
};

#if !defined(MORSE_CLIENT)
static struct
{
    struct arg_rex *macaddr;
} args;
#endif

int macaddr_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Read or write the chip MAC address"
#if !defined(MORSE_CLIENT)
    , args.macaddr = arg_rex0("w", "write", "([a-f0-9]{2}:){5}([a-f0-9]{2})",
            "<macaddr>", 0, "Write the given 'XX:XX:XX:XX:XX:XX' MAC address (not reversible)")
#endif
            ); /* NOLINT (whitespace/parens) */
    return 0;
}


int macaddr(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_mac_addr_req *cmd;
    struct command_mac_addr_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));
    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_mac_addr_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct command_mac_addr_cfm);
    cmd->write = false;

#if !defined(MORSE_CLIENT)
    if (args.macaddr->count > 0)
    {
        cmd->write = true;
        if (str_to_mac_addr(cmd->mac_octet, args.macaddr->sval[0]) < 0)
        {
            mctrl_err("Invalid MAC address\n");
            ret = -1;
            goto exit;
        }
    }
#endif

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MAC_ADDR,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (!ret)
    {
        uint8_t *mac_octet = resp->mac_octet;
        mctrl_print("Chip MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac_octet[0], mac_octet[1], mac_octet[2], mac_octet[3],
               mac_octet[4], mac_octet[5]);
    }
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(macaddr, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
