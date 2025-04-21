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
#include "channel.h"
#include "utilities.h"

#define OPCLASS_DEFAULT 0xFF

/* S1G capabilities, from hostap */
#define S1G_CAP0_S1G_LONG   BIT(0)
#define S1G_CAP0_SGI_1MHZ   BIT(1)
#define S1G_CAP0_SGI_2MHZ   BIT(2)
#define S1G_CAP0_SGI_4MHZ   BIT(3)
#define S1G_CAP0_SGI_8MHZ   BIT(4)
#define S1G_CAP0_SGI_16MHZ  BIT(5)
#define S1G_CAP0_SGI_ALL    ((S1G_CAP0_SGI_1MHZ | S1G_CAP0_SGI_2MHZ | \
                             S1G_CAP0_SGI_4MHZ | S1G_CAP0_SGI_8MHZ))

struct PACKED set_ecsa_command
{
    /** Centre frequency of the operating channel */
    uint32_t operating_channel_freq_hz;

    /** Global Operating class */
    uint8_t opclass;

    /** Pimary channel bw in MHz */
    uint8_t primary_channel_bw_mhz;

    /** 1MHz channel index */
    uint8_t prim_1mhz_ch_idx;

    /** Operating channel bandwidth in MHz */
    uint8_t operating_channel_bw_mhz;

    /** Global Operating class for primary chan */
    uint8_t prim_opclass;

    /** Configured S1G capabilities */
    uint8_t s1g_cap0;
    uint8_t s1g_cap1;
    uint8_t s1g_cap2;
    uint8_t s1g_cap3;
};

static struct
{
    struct arg_int *global_opclass;
    struct arg_int *prim_chan_bw;
    struct arg_int *prim_1mhz_idx;
    struct arg_int *operating_bw;
    struct arg_int *chan_freq;
    struct arg_int *prim_ch_opclass;
    struct arg_int *s1g_capab;
} args;

int ecsa_info_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Set channel parameters for ECSA IE in probe responses and beacons",
        arg_rem(NULL, "Do not use - for internal use by hostapd_s1g"),
        args.chan_freq = arg_rint1("c", NULL, NULL, MIN_FREQ_KHZ, MAX_FREQ_KHZ,
            "Operating channel frequency in kHz"),
        args.operating_bw = arg_int1("o", NULL, NULL, "Operating channel bandwidth in MHz"),
        args.prim_chan_bw = arg_int1("p", NULL, NULL, "Primary channel bandwidth in MHz"),
        args.prim_1mhz_idx = arg_int1("n", NULL, NULL, "Primary 1MHz channel index"),
        args.global_opclass = arg_int1("g", NULL, NULL, "Global operating class"),
        args.prim_ch_opclass = arg_int1("l", NULL, NULL,
            "Global operating class for primary channel"),
        args.s1g_capab = arg_rint0("s", NULL, NULL, 0, S1G_CAP0_SGI_ALL,
            "S1G SGI capabilities"));
    return 0;
}

int ecsa_info(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t freq_khz = 0;
    uint8_t primary_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t op_channel_bandwidth = BANDWIDTH_DEFAULT;
    uint8_t global_operating_class = OPCLASS_DEFAULT;
    uint8_t primary_1Mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX_DEFAULT;
    uint8_t prim_chan_global_op_class = OPCLASS_DEFAULT;
    uint32_t s1g_capab = 0;
    struct set_ecsa_command *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct set_ecsa_command);

    global_operating_class = args.global_opclass->ival[0];
    primary_channel_bandwidth = args.prim_chan_bw->ival[0];
    primary_1Mhz_chan_idx = args.prim_1mhz_idx->ival[0];
    op_channel_bandwidth = args.operating_bw->ival[0];

    freq_khz = args.chan_freq->ival[0];

    prim_chan_global_op_class = args.prim_ch_opclass->ival[0];

    if (args.s1g_capab->count)
        s1g_capab = args.s1g_capab->ival[0];

    cmd->primary_channel_bw_mhz = primary_channel_bandwidth;
    cmd->opclass = global_operating_class;
    cmd->prim_1mhz_ch_idx = primary_1Mhz_chan_idx;
    cmd->operating_channel_freq_hz = htole32(KHZ_TO_HZ(freq_khz));
    cmd->operating_channel_bw_mhz = op_channel_bandwidth;
    cmd->prim_opclass = prim_chan_global_op_class;

    s1g_capab = htole32(s1g_capab);
    cmd->s1g_cap0 = s1g_capab & 0xFF;
    cmd->s1g_cap1 = (s1g_capab >> 8) & 0xFF;
    cmd->s1g_cap2 = (s1g_capab >> 16) & 0xFF;
    cmd->s1g_cap3 = (s1g_capab >> 24) & 0xFF;

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_ECSA_S1G_INFO,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(ecsa_info, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
