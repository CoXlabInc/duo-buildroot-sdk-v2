/*
 * Copyright 2020 Morse Micro
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

enum morse_rpg_commands_id
{
    /* RPG Commands */
    MORSE_CMD_RPG_START_TX          = 0x100,
    MORSE_CMD_RPG_STOP_TX           = 0x101,
    MORSE_CMD_RPG_GET_STATS         = 0x102,
    MORSE_CMD_RPG_RESET_STATS       = 0x103,
    MORSE_CMD_RPG_SET_SOURCE_ADDR   = 0x104,
    MORSE_CMD_RPG_SET_DEST_ADDR     = 0x105,
    MORSE_CMD_RPG_FORCE_AMPDU       = 0x106
};

struct PACKED memcmd_rpg_start_tx
{
    int32_t  size;
    int32_t  count;
    uint8_t  random;
};

struct PACKED memcmd_rpg_get_statistics
{
    uint32_t total_rx_packets;
    uint32_t total_rx_packets_w_correct_fcs;
    uint32_t total_tx_packets;
    uint32_t rx_signal_field_errors;
};

struct PACKED morse_rpg_cmd_set_source_addr
{
    uint8_t source[6];
};

struct PACKED morse_rpg_cmd_set_destination_addr
{
    uint8_t destination[6];
};

struct PACKED morse_rpg_cmd_force_ampdu
{
    uint32_t number;
};

struct PACKED morse_rpg_cmd
{
    uint16_t id;
    union {
        uint8_t opaque[0];
        struct memcmd_rpg_start_tx start;
        struct morse_rpg_cmd_set_source_addr set_source;
        struct morse_rpg_cmd_set_destination_addr set_destination;
        struct morse_rpg_cmd_force_ampdu force_ampdu;
    };
};

static struct {
    struct arg_rex *command;
} args;

static struct mm_argtable start;
static struct mm_argtable stop;
static struct mm_argtable stats_cmd;
static struct mm_argtable srcaddr;
static struct mm_argtable dstaddr;
static struct mm_argtable ampdu;

static struct mm_argtable *subcmds[] =
{
    &start, &stop, &stats_cmd, &srcaddr, &dstaddr, &ampdu
};

static struct {
    struct arg_lit *listen;
    struct arg_lit *disable;
    struct arg_int *count;
    struct arg_int *size;
} start_args;

static struct {
    struct arg_rem *desc;
} stop_args;

static struct {
    struct arg_lit *reset;
} stats_args;

static struct {
    struct arg_str *addr;
} dst_args;

static struct {
    struct arg_str *addr;
} src_args;

static struct {
    struct arg_int *num;
} ampdu_args;

int rpg_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Random Packet Generator commands",
        args.command = arg_rex1(NULL, NULL, "(start|stop|stats|srcaddr|dstaddr|ampdu)",
            "{start|stop|stats|srcaddr|dstaddr|ampdu}", 0, "RPG subcommand"));
    args.command->hdr.flag |= ARG_STOPPARSE;

    MM_INIT_ARGTABLE(&start, "Start Random Packet Generator",
        start_args.listen = arg_lit0("l", NULL, "Listen mode (other options are overlooked)"),
        start_args.disable = arg_lit0("d", NULL, "Disable random packet contents (for casim)"),
        start_args.count = arg_int0("c", NULL, "<num pkts>",
            "Number of packets to send (default unlimited)"),
        start_args.size = arg_rint0("s", NULL, "<size>", 24, INT32_MAX,
            "Size of packets to be sent (min: 24, default: random)"));

    MM_INIT_ARGTABLE(&stop, "Stop RPG",
        stop_args.desc = arg_rem(NULL, NULL));

    MM_INIT_ARGTABLE(&stats_cmd, "Read RPG stats",
        stats_args.reset = arg_lit0("r", NULL, "Reset the rpg stats (if started)"));

    MM_INIT_ARGTABLE(&srcaddr, "Set source adddress of RPG packets",
        src_args.addr = arg_str1(NULL, NULL, "<mac address>",
            "Set src adddress of RPG packets"));

    MM_INIT_ARGTABLE(&dstaddr, "Set destination adddress of RPG packets",
        dst_args.addr = arg_str1(NULL, NULL, "<mac address>",
            "Set dest adddress of RPG packets"));

    MM_INIT_ARGTABLE(&ampdu, "Force using A-MPDU with [number] MPDUs",
        ampdu_args.num = arg_int1(NULL, NULL, "<value>", NULL));
    return 0;
}

int rpg_help(void)
{
    mm_help_argtable("rpg start", &start);
    mm_help_argtable("rpg stop", &stop);
    mm_help_argtable("rpg stats", &stats_cmd);
    mm_help_argtable("rpg srcaddr", &srcaddr);
    mm_help_argtable("rpg dstaddr", &dstaddr);
    mm_help_argtable("rpg ampdu", &ampdu);
    return 0;
}

int rpg_get_cmd(const char str[])
{
    if (strcmp("start", str) == 0) return MORSE_CMD_RPG_START_TX;
    else if (strcmp("stop", str) == 0) return MORSE_CMD_RPG_STOP_TX;
    else if (strcmp("stats", str) == 0) return MORSE_CMD_RPG_GET_STATS;
    else if (strcmp("reset", str) == 0) return MORSE_CMD_RPG_RESET_STATS;
    else if (strcmp("srcaddr", str) == 0) return MORSE_CMD_RPG_SET_SOURCE_ADDR;
    else if (strcmp("dstaddr", str) == 0) return MORSE_CMD_RPG_SET_DEST_ADDR;
    else if (strcmp("ampdu", str) == 0) return MORSE_CMD_RPG_FORCE_AMPDU;
    else
    {
        return -1;
    }
}

int rpg(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct morse_rpg_cmd *cmd;
    struct memcmd_rpg_get_statistics *stats;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    ret = rpg_get_cmd(args.command->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*stats));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct morse_rpg_cmd);
    stats = TBUFF_TO_RSP(rsp_tbuff, struct memcmd_rpg_get_statistics);
    cmd->id = ret;
    switch (cmd->id)
    {
        case MORSE_CMD_RPG_START_TX:
        {
            cmd->start.random = 1;
            cmd->start.count = -1;
            cmd->start.size = -1;

            ret = mm_parse_argtable("rpg start", &start, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }

            if (start_args.count->count)
            {
                cmd->start.count = start_args.count->ival[0];
            }
            if (start_args.size->count)
            {
                cmd->start.size = start_args.size->ival[0];
            }

            if (start_args.listen->count)
            {
                if (start_args.count->count || start_args.size->count)
                {
                    mctrl_err("Conflicting options.\n");
                    ret = -1;
                    goto exit;
                }

                cmd->start.size = 0;
                cmd->start.count = 0;
            }

            if (start_args.disable->count)
            {
                cmd->start.random = 0;
            }
        }
        break;
        case MORSE_CMD_RPG_STOP_TX:
        {
            ret = mm_parse_argtable("rpg stop", &stop, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
        }
        break;
        case MORSE_CMD_RPG_SET_SOURCE_ADDR:
        {
            ret = mm_parse_argtable("rpg srcaddr", &srcaddr, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            uint8_t *mac = cmd->set_source.source;

            if (src_args.addr->count)
            {
                if (str_to_mac_addr(mac, src_args.addr->sval[0]) < 0)
                {
                    ret = -1;
                    goto exit;
                }
            }
        }
        break;
        case MORSE_CMD_RPG_SET_DEST_ADDR:
        {
            ret = mm_parse_argtable("rpg dstaddr", &dstaddr, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            uint8_t *mac = cmd->set_destination.destination;

            if (dst_args.addr->count)
            {
                if (str_to_mac_addr(mac, dst_args.addr->sval[0]) < 0)
                {
                    ret = -1;
                    goto exit;
                }
            }
        }
        break;
        case MORSE_CMD_RPG_FORCE_AMPDU:
        {
            ret = mm_parse_argtable("rpg ampdu", &ampdu, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            cmd->force_ampdu.number = ampdu_args.num->ival[0];
        }
        break;
        case MORSE_CMD_RPG_GET_STATS:
        {
            ret = mm_parse_argtable("rpg stats", &stats_cmd, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            if (stats_args.reset->count)
            {
                cmd->id = MORSE_CMD_RPG_RESET_STATS;
            }
        }
        break;
        case MORSE_CMD_RPG_RESET_STATS:
            mctrl_err("rpg reset is deprecated and replaced with rpg stats -r\n");
            /* fall through */
        default:
        {
            goto exit;
        }
    }

    cmd->start.size = htole32(cmd->start.size);
    cmd->start.count = htole32(cmd->start.count);
    cmd->id = htole16(cmd->id);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_RPG,
                                 cmd_tbuff, rsp_tbuff);

    if ((!ret) && (cmd->id == MORSE_CMD_RPG_GET_STATS))
    {
        mctrl_print("total Rx = %d, RX FCS pass = %d, RX sig field fail = %d, total TX = %d\n",
               (int)stats->total_rx_packets,
               (int)stats->total_rx_packets_w_correct_fcs,
               (int)stats->rx_signal_field_errors,
               (int)stats->total_tx_packets);
    }

exit:
    /* Check if the reason we got here is because --help was given */
    if (mm_check_help_argtable(subcmds, MORSE_ARRAY_SIZE(subcmds)))
    {
        ret = 0;
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    for (int i = 0; i < MORSE_ARRAY_SIZE(subcmds); i++)
    {
        mm_free_argtable(subcmds[i]);
    }
    return ret;
}

MM_CLI_HANDLER_CUSTOM_HELP(rpg, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
