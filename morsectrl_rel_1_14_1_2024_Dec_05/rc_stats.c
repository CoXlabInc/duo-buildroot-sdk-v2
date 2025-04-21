/*
 * Copyright 2024 Morse Micro
 */

#include <errno.h>
#include <stdint.h>

#include "command.h"
#include "mm_argtable3.h"
#include "portable_endian.h"
#include "transport/transport.h"
#include "utilities.h"

#define RATE_INFO_BW_MASK 0x0F
#define RATE_INFO_BW_SHIFT 0
#define RATE_INFO_MCS_MASK 0xF0
#define RATE_INFO_MCS_SHIFT 4
#define RATE_INFO_GUARD_MASK 0x100
#define RATE_INFO_GUARD_SHIFT 8

struct PACKED rc_stats_entry
{
    /** Bit field describing the rate this entry is about. */
    uint32_t rate_info;
    /** Total number of packets transmitted at this rate. */
    uint32_t total_sent;
    /** Total number of successful transmissions at this rate. */
    uint32_t total_success;
    /** Reserved for future expansion of the stats. */
    uint32_t unused;
};

struct PACKED rc_stats_response
{
    /** Reserved for future expansion of the stats. */
    uint8_t unused[8];
    uint32_t n_entries;
    struct rc_stats_entry entries[];
};

static const char *bw_mhz_from_rate_info(uint32_t rate_info)
{
    switch (rate_info & RATE_INFO_BW_MASK)
    {
        case 0: return "1MHz";
        case 1: return "2MHz";
        case 2: return "4MHz";
        case 3: return "8MHz";
    }
    MCTRL_ASSERT(false, "not reached");
}

static unsigned int mcs_from_rate_info(uint32_t rate_info)
{
    return (rate_info & RATE_INFO_MCS_MASK) >> RATE_INFO_MCS_SHIFT;
}

static const char *guard_interval_from_rate_info(uint32_t rate_info)
{
    switch ((rate_info & RATE_INFO_GUARD_MASK) >> RATE_INFO_GUARD_SHIFT)
    {
        case 0: return "LGI";
        case 1: return "SGI";
    }
    MCTRL_ASSERT(false, "not reached");
}

static void print_rc_stats(struct rc_stats_response *rc_stats)
{
    /* This output format vaguely matches mmrc_debugfs.c in the Linux driver, but
     * fullmac rate control does not give as much detail about each rate so
     * there are fewer columns here. */
    mctrl_print("   bw   guard mcs#/ss index  tot_suc  tot_att\n");
    for (size_t i = 0; i < rc_stats->n_entries; i++)
    {
        uint32_t rate_info = le32toh(rc_stats->entries[i].rate_info);
        mctrl_print("%6s %5s  MCS%-2u/1%4zu %9u%9u\n",
                bw_mhz_from_rate_info(rate_info),
                guard_interval_from_rate_info(rate_info),
                mcs_from_rate_info(rate_info),
                i,
                rc_stats->entries[i].total_success,
                rc_stats->entries[i].total_sent);
    }
}

int rc_stats_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Read rate control statistics from the chip (fullmac only)");
    return 0;
}

int rc_stats(struct morsectrl *mors, int argc, char *argv[])
{
    const size_t rsp_bufsize = MORSE_CMD_CFM_LEN;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct rc_stats_response *rc_stats;
    int ret;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, rsp_bufsize);
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -ENOMEM;
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_RC_STATS, cmd_tbuff, rsp_tbuff);
    if (ret)
    {
        goto exit;
    }

    rc_stats = TBUFF_TO_RSP(rsp_tbuff, struct rc_stats_response);

    if (rc_stats->n_entries * sizeof(rc_stats->entries[0]) > rsp_bufsize - sizeof(*rc_stats)) {
        mctrl_err("Number of rate control entries too large for buffer: %u\n",
                  rc_stats->n_entries);
        ret = -EINVAL;
        goto exit;
    }

    print_rc_stats(rc_stats);
    ret = 0;

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(rc_stats, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
