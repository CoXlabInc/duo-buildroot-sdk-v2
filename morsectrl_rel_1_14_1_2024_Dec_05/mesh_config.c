/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#ifndef MORSE_WIN_BUILD
#include <net/if.h>
#endif
#include "portable_endian.h"

#include "command.h"
#include "utilities.h"

#define MESH_ID_LEN_MAX 32

#define MESH_BEACONLESS_MODE_DISABLE 0
#define MESH_BEACONLESS_MODE_ENABLE 1
#define PEER_LINKS_MIN 0
#define PEER_LINKS_MAX 10

struct PACKED set_mesh_config
{
    /** Mesh ID Len */
    uint8_t mesh_id_len;

    /** Mesh ID, equivalent to SSID in infra */
    uint8_t mesh_id[MESH_ID_LEN_MAX];

    /** Mesh beaconless mode enabled/disabled */
    uint8_t mesh_beaconless_mode;

    /** Maximum number of peer links */
    uint8_t max_plinks;
};

static struct
{
    struct arg_str *mesh_id;
    struct arg_int *beaconless;
    struct arg_int *peer_links;
} args;


int mesh_config_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set Mesh configuration parameters",
        args.mesh_id = arg_str1("m", NULL, "<mesh id>", "Mesh ID as a hex string"),
        args.beaconless = arg_rint0("b", NULL, "<mode>", MESH_BEACONLESS_MODE_DISABLE,
        MESH_BEACONLESS_MODE_ENABLE, "Mesh beaconless mode, "
        STR(MESH_BEACONLESS_MODE_ENABLE)": enable, " STR(MESH_BEACONLESS_MODE_DISABLE)": disable"),
        args.peer_links = arg_rint1("p", NULL, "<max peer links>", PEER_LINKS_MIN, PEER_LINKS_MAX,
            "Maximum number of peer links. (" STR(PEER_LINKS_MIN) "-" STR(PEER_LINKS_MAX) ")"));
    return 0;
}

int mesh_config(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    size_t length = 0;
    struct set_mesh_config *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_mesh_config);

    memset(cmd, 0, sizeof(*cmd));

    length = strlen(args.mesh_id->sval[0]);
    if (!length || (length & 1))
    {
        mctrl_err("Invalid Mesh ID hex string length\n");
        ret = -1;
        goto exit;
    }
    length = length / 2;

    if (length > sizeof(cmd->mesh_id))
    {
        mctrl_err("Mesh ID invalid length:%zu, max allowed length is:%zu\n",
                length, sizeof(cmd->mesh_id));
        ret = -1;
        goto exit;
    }

    if (hexstr2bin(args.mesh_id->sval[0], cmd->mesh_id, length))
    {
        mctrl_err("Invalid Mesh ID hex string\n");
        ret = -1;
        goto exit;
    }
    cmd->mesh_id_len = length;

    if (args.beaconless->count > 0)
    {
        cmd->mesh_beaconless_mode = args.beaconless->ival[0];
    }

    cmd->max_plinks = args.peer_links->ival[0];
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_MESH_CONFIG,
                                 cmd_tbuff, rsp_tbuff);
exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(mesh_config, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
