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

#define S1G_CAPABILITY_FLAGS_WIDTH 4
#define SET_S1G_CAP_FLAGS          BIT(0)
#define SET_S1G_CAP_AMPDU_MSS      BIT(1)
#define SET_S1G_CAP_BEAM_STS       BIT(2)
#define SET_S1G_CAP_NUM_SOUND_DIMS BIT(3)
#define SET_S1G_CAP_MAX_AMPDU_LEXP BIT(4)

struct PACKED mm_capabilities
{
    /** Capability flags */
    uint32_t flags[S1G_CAPABILITY_FLAGS_WIDTH];
    /** The minimum A-MPDU start spacing required by firmware.*/
    uint8_t ampdu_mss;
    /** The beamformee STS capability value */
    uint8_t beamformee_sts_capability;
    /** Number of sounding dimensions */
    uint8_t number_sounding_dimensions;
    /** The maximum A-MPDU length. This is the exponent value such that
     * (2^(13 + exponent) - 1) is the length
     */
    uint8_t maximum_ampdu_length_exponent;
};

struct PACKED command_set_capabilities_req
{
    struct mm_capabilities capabilities;
    /** Which caps we are setting */
    uint8_t set_caps;
};

struct PACKED command_get_capabilities_req
{
};

struct PACKED command_get_capabilities_cfm
{
    struct mm_capabilities capabilities;
    /** Morse custom MMSS (Minimum MPDU Start Spacing) offset */
    uint8_t morse_mmss_offset;
};

static struct
{
    struct arg_str *fields;
    struct arg_csi *ampdu_mss;
    struct arg_int *beamformee;
    struct arg_int *sounding_dims;
} args;

int capabilities_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Get (default) or set the device capabilities manifest",
        args.fields = arg_str0("f", NULL, "<hex>", "hex string for the 128-bit capability flags"),
        args.ampdu_mss = arg_csi0("a", NULL,
            "<A-MPDU min start spacing>,<A-MPDU max length exponent>", 2,
            "A-MPDU capabilities"),
        args.beamformee = arg_int0("b", NULL, "<beamformee STS value>", "Beamformee STS value"),
        args.sounding_dims = arg_int0("s", NULL, "<dims>", "Beamformer sounding dimensions"));
    return 0;
}

static void print_capabs(struct morsectrl *mors,
    struct command_get_capabilities_cfm *rsp_get_capabs)
{
    struct mm_capabilities *capabs = &rsp_get_capabs->capabilities;

    mctrl_print("Interface: %s\n", morsectrl_transport_get_ifname(mors->transport));

    for (int i = 0; i < MORSE_ARRAY_SIZE(capabs->flags); i++)
        mctrl_print("Flags %u: 0x%x\n", i, capabs->flags[i]);
    mctrl_print("A-MPDU MSS: %u\n", capabs->ampdu_mss);
    mctrl_print("Maximum A-MPDU length exponent: %u\n",
        capabs->maximum_ampdu_length_exponent);
    mctrl_print("Beamformee STS cap: %u\n",
        capabs->beamformee_sts_capability);
    mctrl_print("Number of sounding dimensions: %u\n",
        capabs->number_sounding_dimensions);
    mctrl_print("Custom MMSS (Minimum MPDU Start Spacing) offset: %u\n",
        rsp_get_capabs->morse_mmss_offset);
    return;
}

int capabilities(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    bool is_get = false;
    uint32_t buffer[4] = {};
    struct command_get_capabilities_req *cmd_get_capabs;
    struct command_get_capabilities_cfm *rsp_get_capabs;
    struct command_set_capabilities_req *cmd_set_capabs;
    struct morsectrl_transport_buff *cmd_get_tbuff;
    struct morsectrl_transport_buff *rsp_get_tbuff;
    struct morsectrl_transport_buff *cmd_set_tbuff;
    struct morsectrl_transport_buff *rsp_set_tbuff;

    cmd_get_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_get_capabs));
    rsp_get_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp_get_capabs));
    cmd_set_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd_set_capabs));
    rsp_set_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    int count = args.fields->count + args.ampdu_mss->count +
                args.beamformee->count + args.sounding_dims->count;
    if (count == 0)
    {
        is_get = true;
    }

    if (!cmd_get_tbuff || !rsp_get_tbuff || !cmd_set_tbuff || !rsp_set_tbuff)
        goto exit;

    cmd_set_capabs = TBUFF_TO_CMD(cmd_set_tbuff, struct command_set_capabilities_req);
    cmd_get_capabs = TBUFF_TO_CMD(cmd_get_tbuff, struct command_get_capabilities_req);
    rsp_get_capabs = TBUFF_TO_RSP(rsp_get_tbuff, struct command_get_capabilities_cfm);

    memset(cmd_set_capabs, 0, sizeof(*cmd_set_capabs));
    memset(cmd_get_capabs, 0, sizeof(*cmd_get_capabs));

    /* parse capability flag args */
    if (args.fields->count)
    {
        cmd_set_capabs->set_caps |= SET_S1G_CAP_FLAGS;
        for (int flag_idx = 0; flag_idx < S1G_CAPABILITY_FLAGS_WIDTH; flag_idx++)
        {
            /* Avoid taking address of packed struct member since it could be unaligned */
            if (hexstr2uint32_arr(args.fields->sval[0], buffer, S1G_CAPABILITY_FLAGS_WIDTH) != 0)
            {
                mctrl_err("Invalid hex string provided\n");
            }
            memcpy(cmd_set_capabs->capabilities.flags, buffer,
                   sizeof(*cmd_set_capabs->capabilities.flags));
        }
    }

    if (args.ampdu_mss->count)
    {
        cmd_set_capabs->set_caps |= SET_S1G_CAP_AMPDU_MSS;
        cmd_set_capabs->set_caps |= SET_S1G_CAP_MAX_AMPDU_LEXP;
        cmd_set_capabs->capabilities.ampdu_mss = args.ampdu_mss->ival[0][0];
        cmd_set_capabs->capabilities.maximum_ampdu_length_exponent = args.ampdu_mss->ival[0][1];
    }

    if (args.beamformee->count)
    {
        cmd_set_capabs->set_caps |= SET_S1G_CAP_BEAM_STS;
        cmd_set_capabs->capabilities.beamformee_sts_capability = args.beamformee->ival[0];
    }

    if (args.sounding_dims)
    {
        cmd_set_capabs->set_caps |= SET_S1G_CAP_NUM_SOUND_DIMS;
        cmd_set_capabs->capabilities.number_sounding_dimensions = args.sounding_dims->ival[0];
    }

    if (is_get)
    {
        ret = morsectrl_send_command(mors->transport,
                                     MORSE_COMMAND_GET_CAPABILITIES,
                                     cmd_get_tbuff, rsp_get_tbuff);

        if (ret >= 0)
            print_capabs(mors, rsp_get_capabs);
    }
    else
    {
        ret = morsectrl_send_command(mors->transport,
                                     MORSE_TEST_SET_CAPABILITIES,
                                     cmd_set_tbuff, rsp_set_tbuff);
    }

exit:
    morsectrl_transport_buff_free(cmd_set_tbuff);
    morsectrl_transport_buff_free(rsp_set_tbuff);
    morsectrl_transport_buff_free(cmd_get_tbuff);
    morsectrl_transport_buff_free(rsp_get_tbuff);

    return ret;
}

MM_CLI_HANDLER(capabilities, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
