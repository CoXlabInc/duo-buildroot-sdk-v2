/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"

#define SIG_FIELD_ERROR_EVENT_DISABLED              (0)
#define SIG_FIELD_ERROR_EVENT_ENABLED_MONITOR_ONLY  (1)
#define SIG_FIELD_ERROR_EVENT_ENABLED_ANY_MODE      (2)

struct PACKED command_set_sig_field_error_event_config_req
{
    uint8_t config;
};

static struct
{
    struct arg_rex *enable;
} args;

int sig_field_error_evt_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Enable/disable signal field error events in monitor mode",
        arg_rem(NULL,
            "These events will appear in sniffer traces as radiotap headers with no payload"),
        args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
            "Enable/disable signal field errors in radiotap headers with no payload"));
    return 0;
}

int sig_field_error_evt(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int enabled;
    struct command_set_sig_field_error_event_config_req *cmd;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    enabled = expression_to_int(args.enable->sval[0]);

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_sig_field_error_event_config_req);

    if (enabled)
    {
        /*
         * As noted in the usage documentation, the "enable" option means enabled in monitor
         * mode. The firmware supports enabling this event in any mode (i.e.,
         * SIG_FIELD_ERROR_EVENT_ENABLED_ANY_MODE), but there is currently no use case for it.
         */
        cmd->config = SIG_FIELD_ERROR_EVENT_ENABLED_MONITOR_ONLY;
    }
    else
    {
        cmd->config = SIG_FIELD_ERROR_EVENT_DISABLED;
    }

    ret = morsectrl_send_command(mors->transport,
                                 MORSE_TEST_COMMAND_SET_SIG_FIELD_ERROR_EVENT_CONFIG,
                                 cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(sig_field_error_evt, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
