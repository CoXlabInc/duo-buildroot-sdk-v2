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

#define MAX_VENDOR_IE_LENGTH                (255)
#define NUM_OUI_BYTES                       (3)

enum command_vendor_ie_opcode
{
    MORSE_VENDOR_IE_OP_ADD_ELEMENT = 0,
    MORSE_VENDOR_IE_OP_CLEAR_ELEMENTS,
    MORSE_VENDOR_IE_OP_ADD_FILTER,
    MORSE_VENDOR_IE_OP_CLEAR_FILTERS,

    MORSE_VENDOR_IE_OP_MAX = UINT16_MAX,
    MORSE_VENDOR_IE_OP_INVALID = MORSE_VENDOR_IE_OP_MAX
};

enum vendor_ie_mgmt_type_flags {
    MORSE_VENDOR_IE_TYPE_BEACON     = BIT(0),
    MORSE_VENDOR_IE_TYPE_PROBE_REQ  = BIT(1),
    MORSE_VENDOR_IE_TYPE_PROBE_RESP = BIT(2),
    MORSE_VENDOR_IE_TYPE_ASSOC_REQ  = BIT(3),
    MORSE_VENDOR_IE_TYPE_ASSOC_RESP = BIT(4),
    /* ... etc. */

    MORSE_VENDOR_IE_TYPE_ALL        = UINT16_MAX
};

struct PACKED command_vendor_ie_req
{
    uint16_t opcode;
    uint16_t mgmt_type_mask;
    uint8_t data[MAX_VENDOR_IE_LENGTH];
};

struct PACKED command_vendor_ie_cfm
{
    /* empty */
};

static struct
{
    struct arg_str *add;
    struct arg_lit *clear;
    struct arg_rex *oui;
    struct arg_lit *reset_oui_whitelist;
    struct arg_lit *beacons;
    struct arg_lit *probes;
    struct arg_lit *assoc;
} args;

int vendor_ie_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Manipulate vendor information elements",
        args.add = arg_str0("a", "add", "<bytes>", "Add a vendor element (hex string)"),
        args.clear = arg_lit0("c", "clear", "Clear previously added vendor elements"),
        args.oui = arg_rex0("o", "oui", "[a-z0-9]{6}", "<OUI>", ARG_REX_ICASE,
            "Add an OUI to the vendor IE whitelist (hex string)"),
        args.reset_oui_whitelist = arg_lit0("r", NULL, "Reset configured OUI whitelist"),
        args.beacons = arg_lit0("b", "beacon", "Apply to beacons"),
        args.probes = arg_lit0("p", "probe", "Apply to probe requests/responses"),
        args.assoc = arg_lit0("s", "assoc", "Apply to assoc requests/responses"));
    return 0;
}

int vendor_ie(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;
    size_t length = 0;
    int count;

    struct command_vendor_ie_req *cmd_vie;
    struct command_vendor_ie_cfm *rsp_vie;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    count = args.add->count + args.clear->count + args.oui->count + args.reset_oui_whitelist->count;

    if (count > 1)
    {
        mctrl_err("Specify only one of [-a, -o, -r, -c]\n");
        return -1;
    }

    if (count == 0)
    {
        mctrl_err("You must specify one of [-a, -o, -r, -c]\n");
        return -1;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_vie));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_vie));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -1;
        goto exit;
    }

    cmd_vie = TBUFF_TO_CMD(cmd_tbuff, struct command_vendor_ie_req);
    rsp_vie = TBUFF_TO_RSP(rsp_tbuff, struct command_vendor_ie_cfm);

    if (cmd_vie == NULL ||
        rsp_vie == NULL)
    {
        ret = -1;
        goto exit;
    }

    memset(cmd_vie, 0, sizeof(*cmd_vie));
    cmd_vie->opcode = MORSE_VENDOR_IE_OP_INVALID;
    cmd_vie->mgmt_type_mask = 0;

    if (args.probes->count)
    {
        cmd_vie->mgmt_type_mask |= (MORSE_VENDOR_IE_TYPE_PROBE_REQ |
                                    MORSE_VENDOR_IE_TYPE_PROBE_RESP);
    }

    if (args.assoc->count)
    {
        cmd_vie->mgmt_type_mask |= (MORSE_VENDOR_IE_TYPE_ASSOC_REQ |
                                    MORSE_VENDOR_IE_TYPE_ASSOC_RESP);
    }

    if (args.beacons->count)
    {
        cmd_vie->mgmt_type_mask |= MORSE_VENDOR_IE_TYPE_BEACON;
    }

    if (args.add->count)
    {
        const char *ie_str = args.add->sval[0];
        length = strlen(ie_str);
        if (length & 1)
        {
            mctrl_err("Odd number of characters in data bytestring\n");
            ret = -1;
            goto exit;
        }
        length = length / 2;

        if (length > sizeof(cmd_vie->data))
        {
            mctrl_err("Vendor IE has too many bytes %zu\n", length);
            ret = -1;
            goto exit;
        }

        cmd_vie->opcode = MORSE_VENDOR_IE_OP_ADD_ELEMENT;
        if (hexstr2bin(ie_str, cmd_vie->data, length))
        {
            mctrl_err("Invalid hex string\n");
            ret = -1;
            goto exit;
        }
    }

    if (args.oui->count)
    {
        const char *oui_str = args.oui->sval[0];
        length = strlen(oui_str) / 2;
        cmd_vie->opcode = MORSE_VENDOR_IE_OP_ADD_FILTER;
        hexstr2bin(oui_str, cmd_vie->data, length);
    }

    if (args.reset_oui_whitelist->count)
    {
        cmd_vie->opcode = MORSE_VENDOR_IE_OP_CLEAR_FILTERS;
    }

    if (args.clear->count)
    {
        cmd_vie->opcode = MORSE_VENDOR_IE_OP_CLEAR_ELEMENTS;
    }

    /* set the length used in command */
    morsectrl_transport_set_cmd_data_length(cmd_tbuff,
                                length + sizeof(*cmd_vie) - sizeof(cmd_vie->data));

    if (cmd_vie->mgmt_type_mask == 0)
    {
        mctrl_err("No frame type specified\n");
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_COMMAND_VENDOR_IE_CONFIG,
                                 cmd_tbuff,
                                 rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

MM_CLI_HANDLER(vendor_ie, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
