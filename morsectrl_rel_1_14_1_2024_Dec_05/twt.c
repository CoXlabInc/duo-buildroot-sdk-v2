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

#define TWT_WAKE_DURATION_UNIT                      (256)
#define TWT_WAKE_INTERVAL_EXPONENT_MAX_VAL          (31)
#define TWT_WAKE_DURATION_MAX_US                    (65280) /* UINT8_MAX * TWT_WAKE_DURATION_UNIT */
#define TWT_MAX_SETUP_COMMAND_VAL                   7
#define TWT_MAX_FLOW_ID_VAL                         7

#define TWT_WAKE_INTERVAL_DATATYPE "<wake interval>"
#define TWT_WAKE_INTERVAL_GLOSSARY "Wake interval (usecs)"

#define TWT_WAKE_DURATION_DATATYPE "<min wake duration>"
#define TWT_WAKE_DURATION_GLOSSARY "Minimum wake duration during TWT service period (usecs)"

#define TWT_SETUP_CMD_DATATYPE "<command>"
#define TWT_SETUP_CMD_GLOSSARY "TWT setup command to use:"

#define TWT_FLOW_ID_DATATYPE "<flow id>"
#define TWT_FLOW_ID_GLOSSARY "Flow id for TWT agreement (0-" STR(TWT_MAX_FLOW_ID_VAL) ")"

typedef enum {
    TWT_CONF_SUBCMD_CONFIGURE,
    TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT,
    TWT_CONF_SUBCMD_REMOVE_AGREEMENT,
    TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT,
} twt_subcommands_t;

struct PACKED command_set_twt_conf {
    /** The target wake time (TSF) for the first TWT service period */
    uint64_t target_wake_time;
    /** Wake interval (us) */
    union {
        uint64_t wake_interval_us;
        struct {
            uint16_t wake_interval_mantissa;
            uint8_t wake_interval_exponent;
            uint8_t __padding[5];
        } explicit;
    };
    /** Minimum wake duration during TWT service period (us) */
    uint32_t wake_duration_us;
    /** TWT setup command to use (0: request, 1: suggest, 2: demand) */
    uint8_t twt_setup_command;
    uint8_t __padding[3];
};

struct PACKED command_twt_req {
    /** TWT subcommands, see @ref twt_subcommands_t */
    uint8_t cmd;
    /** The flow (TWT) identifier for the agreement to set, install or remove */
    uint8_t flow_id;
    union {
        uint8_t opaque[0];
        struct command_set_twt_conf set_twt_conf;
    };
};

static struct {
    struct arg_rex *command;
} args;

static struct mm_argtable configure;
static struct mm_argtable remove_cmd;
#if !defined(MORSE_CLIENT)
static struct mm_argtable install;
static struct mm_argtable explicit;
#endif

static struct mm_argtable *subcmds[] =
{
    &configure, &remove_cmd,
#if !defined(MORSE_CLIENT)
    &install, &explicit
#endif
};

static struct {
    struct arg_int *flow_id;
    struct arg_llong *wake_interval;
    struct arg_int *wake_duration;
    struct arg_int *setup_command;
} configure_args;

static struct {
    struct arg_int *flow_id;
} remove_args;

#if !defined(MORSE_CLIENT)
static struct {
    struct arg_int *flow_id;
    struct arg_llong *wake_interval;
    struct arg_int *wake_duration;
    struct arg_llong *wake_time;
} install_args;

static struct {
    struct arg_int *flow_id;
    struct arg_int *wake_duration;
    struct arg_int *mantissa;
    struct arg_int *exponent;
    struct arg_int *setup_command;
} explicit_args;
#endif

int twt_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
#define TWT_AVAILABLE_COMMANDS "conf|remove"
#if !defined(MORSE_CLIENT)
#undef TWT_AVAILABLE_COMMANDS
#define TWT_AVAILABLE_COMMANDS "conf|remove|install|explicit"
#endif

    MM_INIT_ARGTABLE(mm_args, "Install or remove a TWT agreement on a STA interface",
        args.command = arg_rex1(NULL, NULL, "(" TWT_AVAILABLE_COMMANDS ")",
            "{" TWT_AVAILABLE_COMMANDS "}", 0, "TWT subcommand"));
    args.command->hdr.flag |= ARG_STOPPARSE;

    MM_INIT_ARGTABLE(&configure, "Configure TWT settings",
        configure_args.flow_id = arg_rint0("f", NULL, TWT_FLOW_ID_DATATYPE, 0, TWT_MAX_FLOW_ID_VAL,
            TWT_FLOW_ID_GLOSSARY),
        configure_args.wake_interval = arg_llong0("w", NULL, TWT_WAKE_INTERVAL_DATATYPE,
            TWT_WAKE_INTERVAL_GLOSSARY),
        configure_args.wake_duration = arg_rint0("d", NULL, TWT_WAKE_DURATION_DATATYPE, 0,
            TWT_WAKE_DURATION_MAX_US, TWT_WAKE_DURATION_GLOSSARY),
        configure_args.setup_command = arg_rint0("c", NULL, TWT_SETUP_CMD_DATATYPE, 0,
            TWT_MAX_SETUP_COMMAND_VAL, TWT_SETUP_CMD_GLOSSARY),
        arg_rem(NULL, "1: suggest"),
        arg_rem(NULL, "2: demand"),
        arg_rem(NULL, "3: grouping"),
        arg_rem(NULL, "4: accept"),
        arg_rem(NULL, "5: alternate"),
        arg_rem(NULL, "6: dictate"),
        arg_rem(NULL, "7: reject"));

    MM_INIT_ARGTABLE(&remove_cmd, "Remove TWT agreement",
        remove_args.flow_id = arg_rint0("f", NULL, TWT_FLOW_ID_DATATYPE, 0, TWT_MAX_FLOW_ID_VAL,
            TWT_FLOW_ID_GLOSSARY));

#if !defined(MORSE_CLIENT)
    MM_INIT_ARGTABLE(&install, "Install TWT agreement",
        install_args.flow_id = arg_rint0("f", NULL, TWT_FLOW_ID_DATATYPE, 0, TWT_MAX_FLOW_ID_VAL,
            TWT_FLOW_ID_GLOSSARY),
        install_args.wake_interval = arg_llong0("w", NULL, TWT_WAKE_INTERVAL_DATATYPE,
            TWT_WAKE_INTERVAL_GLOSSARY),
        install_args.wake_duration = arg_rint0("d", NULL, TWT_WAKE_DURATION_DATATYPE, 0,
            TWT_WAKE_DURATION_MAX_US, TWT_WAKE_DURATION_GLOSSARY),
        install_args.wake_time = arg_llong0("t", NULL, "<target wake time>",
            "Target wake time (TSF) for the first TWT service period"));

    MM_INIT_ARGTABLE(&explicit, "Install explicit TWT agreement",
        explicit_args.wake_duration = arg_rint0("d", NULL, TWT_WAKE_DURATION_DATATYPE, 0,
            TWT_WAKE_DURATION_MAX_US, TWT_WAKE_DURATION_GLOSSARY),
        explicit_args.mantissa = arg_int0("m", NULL, "<mantissa>", "Wake interval mantissa"),
        explicit_args.exponent = arg_rint0("e", NULL, "<exponent>", 0,
            TWT_WAKE_INTERVAL_EXPONENT_MAX_VAL, "Wake interval exponent"),
        explicit_args.setup_command = arg_rint0("c", NULL, TWT_SETUP_CMD_DATATYPE, 0,
            TWT_MAX_SETUP_COMMAND_VAL, TWT_SETUP_CMD_GLOSSARY),
        explicit_args.flow_id = arg_rint0("f", NULL, TWT_FLOW_ID_DATATYPE, 0, TWT_MAX_FLOW_ID_VAL,
            TWT_FLOW_ID_GLOSSARY),
        arg_rem(NULL, "0: request"),
        arg_rem(NULL, "1: suggest"),
        arg_rem(NULL, "2: demand"),
        arg_rem(NULL, "3: grouping"),
        arg_rem(NULL, "4: accept"),
        arg_rem(NULL, "5: alternate"),
        arg_rem(NULL, "6: dictate"),
        arg_rem(NULL, "7: reject"));
#endif
    return 0;
}

int twt_help(void)
{
    mm_help_argtable("twt conf", &configure);
    mm_help_argtable("twt remove", &remove_cmd);
#if !defined(MORSE_CLIENT)
    mm_help_argtable("twt install", &install);
    mm_help_argtable("twt explicit", &explicit);
#endif
    return 0;
}

static int twt_get_cmd(const char str[])
{
    if (strcmp("conf", str) == 0) return TWT_CONF_SUBCMD_CONFIGURE;
    else if (strcmp("remove", str) == 0) return TWT_CONF_SUBCMD_REMOVE_AGREEMENT;
#if !defined(MORSE_CLIENT)
    if (strcmp("install", str) == 0) return TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT;
    else if (strcmp("explicit", str) == 0) return TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT;
#endif
    else
    {
        return -1;
    }
}

int twt(struct morsectrl *mors, int argc, char *argv[])
{
    int cmd_id;
    int ret = -1;
    int i;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    struct command_twt_req *twt_cmd = NULL;
    uint8_t flow_id = 0; /* flow id always set to 0 for now */
    uint32_t wake_duration_us = 0;
    uint64_t wake_interval_us = 0;
    uint64_t target_wake_time = 0;
#if !defined(MORSE_CLIENT)
    uint16_t wake_interval_mantissa = 0;
    uint8_t wake_interval_exponent = 0;
#endif
    uint8_t setup_cmd = 0;

    cmd_id = twt_get_cmd(args.command->sval[0]);

    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);
    if (!rsp_tbuff)
        goto exit;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*twt_cmd));
    if (!cmd_tbuff)
        goto exit;

    twt_cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_twt_req);
    switch (cmd_id)
    {
        case TWT_CONF_SUBCMD_CONFIGURE:
        {
            ret = mm_parse_argtable("twt conf", &configure, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            if (configure_args.flow_id->count +
                configure_args.setup_command->count +
                configure_args.wake_duration->count +
                configure_args.wake_interval->count == 0)
            {
                mctrl_print("At least one of -w, -d or -c is required\n");
                ret = -1;
                goto exit;
            }

            if (configure_args.flow_id->count)
            {
                flow_id = configure_args.flow_id->ival[0];
            }
            if (configure_args.wake_interval->count)
            {
                wake_interval_us = configure_args.wake_interval->ival[0];
            }
            if (configure_args.wake_duration->count)
            {
                wake_duration_us = configure_args.wake_duration->ival[0];
            }
            if (configure_args.setup_command->count)
            {
                setup_cmd = configure_args.setup_command->ival[0];
            }

            twt_cmd->flow_id = flow_id;
            twt_cmd->cmd = cmd_id;
            twt_cmd->set_twt_conf.wake_interval_us = wake_interval_us;
            twt_cmd->set_twt_conf.wake_duration_us = wake_duration_us;
            twt_cmd->set_twt_conf.twt_setup_command = setup_cmd;
            break;
        }
        case TWT_CONF_SUBCMD_REMOVE_AGREEMENT:
        {
            ret = mm_parse_argtable("twt remove", &remove_cmd, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }
            if (remove_args.flow_id->count)
            {
                flow_id = remove_args.flow_id->ival[0];
            }
            twt_cmd->flow_id = flow_id;
            twt_cmd->cmd = cmd_id;
        }
        break;
#if !defined(MORSE_CLIENT)
        case TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT:
        {
            ret = mm_parse_argtable("twt explicit", &explicit, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }

            if (explicit_args.exponent->count +
                explicit_args.mantissa->count +
                explicit_args.setup_command->count +
                explicit_args.wake_duration->count == 0)
            {
                mctrl_print("At least one of -d, -m, -e, or -c is required\n");
                ret = -1;
                goto exit;
            }

            if (explicit_args.flow_id->count)
            {
                flow_id = explicit_args.flow_id->ival[0];
            }
            if (explicit_args.mantissa->count)
            {
                wake_interval_mantissa = explicit_args.mantissa->ival[0];
            }
            if (explicit_args.exponent->count)
            {
                wake_interval_exponent = explicit_args.exponent->ival[0];
            }
            if (explicit_args.wake_duration->count)
            {
                wake_duration_us = explicit_args.wake_duration->ival[0];
            }
            if (explicit_args.setup_command->count)
            {
                setup_cmd = explicit_args.setup_command->ival[0];
            }

            twt_cmd->flow_id = flow_id;
            twt_cmd->cmd = cmd_id;
            /* Set here for logging later on */
            wake_interval_us = wake_interval_mantissa * (1ULL << wake_interval_exponent);
            twt_cmd->set_twt_conf.explicit.wake_interval_exponent = wake_interval_exponent;
            twt_cmd->set_twt_conf.explicit.wake_interval_mantissa = wake_interval_mantissa;
            twt_cmd->set_twt_conf.wake_duration_us = wake_duration_us;
            twt_cmd->set_twt_conf.twt_setup_command = setup_cmd;
            break;
        }
        case TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT:
        {
            ret = mm_parse_argtable("twt install", &install, argc, argv);
            if (ret != 0)
            {
                goto exit;
            }

            if (install_args.flow_id->count +
                install_args.wake_duration->count +
                install_args.wake_interval->count +
                install_args.wake_time->count == 0)
            {
                mctrl_print("At least one of -f, -w, -d, -t is required\n");
                ret = -1;
                goto exit;
            }

            if (install_args.flow_id->count)
            {
                flow_id = install_args.flow_id->ival[0];
            }
            if (install_args.wake_duration->count)
            {
                wake_duration_us = install_args.wake_duration->ival[0];
            }
            if (install_args.wake_interval->count)
            {
                wake_interval_us = install_args.wake_interval->ival[0];
            }
            if (install_args.wake_time->count)
            {
                target_wake_time = install_args.wake_time->ival[0];
            }

            twt_cmd->flow_id = flow_id;
            twt_cmd->cmd = cmd_id;

            twt_cmd->set_twt_conf.wake_interval_us = wake_interval_us;
            twt_cmd->set_twt_conf.wake_duration_us = wake_duration_us;
            twt_cmd->set_twt_conf.twt_setup_command = setup_cmd;
            twt_cmd->set_twt_conf.target_wake_time = target_wake_time;
            break;
        }

#endif
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_TWT_SET_CONF, cmd_tbuff,
            rsp_tbuff);

exit:
    /* Check if the reason we got here is because --help was given */
    if (mm_check_help_argtable(subcmds, MORSE_ARRAY_SIZE(subcmds)))
    {
        ret = 0;
    }
    else if (ret == 0 &&
             (cmd_id == TWT_CONF_SUBCMD_CONFIGURE ||
              cmd_id == TWT_CONF_SUBCMD_CONFIGURE_EXPLICIT ||
              cmd_id == TWT_CONF_SUBCMD_FORCE_INSTALL_AGREEMENT))
    {
        if (twt_cmd != NULL)
        {
            mctrl_print("Installed TWT Agreement[flowid:%d]\n", flow_id);
            mctrl_print("    Wake interval: %" PRId64 " us\n",
                wake_interval_us);
            mctrl_print("    Wake duration: %d us\n", wake_duration_us);
            mctrl_print("    Target Wake Time: %" PRId64 "\n",
                target_wake_time);
            mctrl_print("    Implict: true\n");
        }
    }
    else if (cmd_id == TWT_CONF_SUBCMD_REMOVE_AGREEMENT)
    {
        mctrl_print("Removed TWT Agreement[flowid:%d]\n", twt_cmd->flow_id);
    }

    if (cmd_tbuff)
    {
        morsectrl_transport_buff_free(cmd_tbuff);
    }
    if (rsp_tbuff)
    {
        morsectrl_transport_buff_free(rsp_tbuff);
    }

    for (i = 0; i < MORSE_ARRAY_SIZE(subcmds); i++)
    {
        mm_free_argtable(subcmds[i]);
    }
    return ret;
}

MM_CLI_HANDLER_CUSTOM_HELP(twt, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
