/*
 * Copyright 2020 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"

#define NUM_BOUNDS_VALUES 2

struct PACKED command_qos_params_req
{
    /** index of the QoS queue whose values will be changed */
    uint8_t queue_idx;

    /** How many slots to wait for AIFS */
    uint8_t aifs_slot_count;

    /** Contention window min/max values */
    uint16_t contention_window_min;
    uint16_t contention_window_max;

    /** Maximum possible TX OP in us */
    uint32_t max_txop_us;
};

struct PACKED command_qos_params_cfm
{
    /** How many slots to wait for AIFS */
    uint8_t aifs_slot_count;

    /** Contention window min/max values */
    uint16_t contention_window_min;
    uint16_t contention_window_max;

    /** Maximum possible TX OP in us */
    uint32_t max_txop_us;
};

static struct {
    struct arg_int *aifs_slots;
    struct arg_int *max_txop_us;
    struct arg_csi *contention;
    struct arg_int *queue;
} args;

int qos_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Get (default) or set QOS parameters for a queue",
        args.aifs_slots = arg_int0("c", NULL, "<value>",
            "Number of AIFS slots to wait"),
        args.max_txop_us = arg_int0("t", NULL, "<value>",
            "Maximum possible TX OP in us"),
        args.contention = arg_csi0("m", NULL, "<min>,<max>", NUM_BOUNDS_VALUES,
            "Contention window min and max values"),
        args.queue = arg_int1(NULL, NULL, "<queue ID>", "QoS queue ID"));
    return 0;
}

int qos(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct command_qos_params_req *cmd;
    struct command_qos_params_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    uint8_t set = 0;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_qos_params_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct command_qos_params_cfm);

    cmd->queue_idx = -1;
    cmd->aifs_slot_count = -1;
    cmd->contention_window_min = -1;
    cmd->contention_window_max = -1;
    cmd->max_txop_us = -1;

    cmd->queue_idx = args.queue->ival[0];

    if (args.aifs_slots->count)
    {
        cmd->aifs_slot_count = args.aifs_slots->ival[0];
        set = 1;
    }

    if (args.max_txop_us->count)
    {
        cmd->max_txop_us = htole32(args.max_txop_us->ival[0]);
        set = 1;
    }

    if (args.contention->count)
    {
        cmd->contention_window_min = args.contention->ival[0][0];
        cmd->contention_window_max = args.contention->ival[0][1];

        if (cmd->contention_window_min > cmd->contention_window_max)
        {
            mctrl_err("Min must not exceed max\n");
            ret = -1;
            goto exit;
        }
        set = 1;
    }

    if (set)
    {
        cmd->contention_window_max = htole16(cmd->contention_window_max);
        cmd->contention_window_min = htole16(cmd->contention_window_min);

        ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_QOS_PARAMS,
                                     cmd_tbuff, rsp_tbuff);
        if (ret < 0)
        {
            goto exit;
        }
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_QOS_PARAMS,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (!ret)
    {
        mctrl_print("QoS (min): %d\t(max): %d\n",
               (uint16_t)resp->contention_window_min, (uint16_t)resp->contention_window_max);
        mctrl_print("AIFS count: %d\n", (uint8_t)resp->aifs_slot_count);
        mctrl_print("Max TX OP (us): %d\n", (uint32_t)resp->max_txop_us);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(qos, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
