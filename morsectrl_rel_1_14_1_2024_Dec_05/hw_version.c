/*
 * Copyright 2023 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "morsectrl.h"
#include "portable_endian.h"

#include "command.h"

/** Structure for a get hw_version confirm */
struct PACKED get_hw_version_response
{
    /** The version string */
    uint8_t hw_version[64];
};

int hw_version_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Get the hardware version");
    return 0;
}

int hw_version(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    struct get_hw_version_response *hw_version;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, 0);
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*hw_version));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    hw_version = TBUFF_TO_RSP(rsp_tbuff, struct get_hw_version_response);

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_HW_VERSION,
                                 cmd_tbuff, rsp_tbuff);
exit:
    if (ret >= 0)
    {
        mctrl_print("HW Version: %s\n", hw_version->hw_version);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(hw_version, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
