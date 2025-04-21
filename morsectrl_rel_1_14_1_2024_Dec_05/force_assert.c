/*
 * Copyright 2021 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "portable_endian.h"

#include "command.h"

enum hartid
{
    HOST_HARTID = 0,
    MAC_HARTID = 1,
    UPHY_HARTID = 2,
    LPHY_HARTID = 3
};
struct PACKED command_force_assert_req
{
    /* Target hart to crash with an intended assert */
    uint32_t hart_id;
};

static struct
{
    struct arg_lit *app;
    struct arg_lit *mac;
    struct arg_lit *uphy;
    struct arg_lit *lphy;
} args;

int assert_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Force a firmware core to assert (defaults to MAC core)",
        args.app = arg_lit0("a", NULL, "APP core"),
        args.mac = arg_lit0("m", NULL, "MAC core"),
        args.uphy = arg_lit0("u", NULL, "UPHY core"),
        args.lphy = arg_lit0("l", NULL, "LPHY core"));
    return 0;
}

static int assert(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int count = 0;
    int hart_id = 0;
    struct command_force_assert_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    count = args.app->count + args.mac->count + args.uphy->count + args.lphy->count;
    if (count == 0)
    {
        hart_id = MAC_HARTID;
    }
    else if (count > 1)
    {
        mctrl_err("Only one core may be asserted at a time\n");
        ret = -1;
        return ret;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_force_assert_req);

    if (args.app->count)
    {
        hart_id = HOST_HARTID;
    }
    if (args.mac->count)
    {
        hart_id = MAC_HARTID;
    }
    if (args.uphy->count)
    {
        hart_id = UPHY_HARTID;
    }
    if (args.lphy->count)
    {
        hart_id = LPHY_HARTID;
    }
    cmd->hart_id = htole32(hart_id);

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_FORCE_ASSERT,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret != -110)
    {
        mctrl_err("Chip didn't timeout. Command failed\n");
        if (ret)
            mctrl_err("Wrong error code returned: %d\n", ret);
        ret = -1;
    }
    else
    {
        ret = 0;
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(assert, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
