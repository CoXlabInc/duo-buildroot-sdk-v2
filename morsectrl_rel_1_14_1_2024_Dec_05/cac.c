/*
 * Copyright 2023 Morse Micro
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"

#define CAC_CFG_CHANGE_RULE_MAX         (8)
#define CAC_CFG_ARFS_MAX                (99)
#define CAC_CFG_CHANGE_MAX              (99)
#define CAC_CFG_CHANGE_STEP             (5)

enum cac_command {
    CAC_COMMAND_DISABLE = 0,
    CAC_COMMAND_ENABLE = 1,
    CAC_COMMAND_CFG_GET = 2,
    CAC_COMMAND_CFG_SET = 3
};

static struct {
    struct arg_rex *subcmd;
    struct arg_rex *decrease;
    struct arg_rex *increase;
    struct arg_lit *verbose;
} args;

/** CAC threshold change rule */
struct PACKED cac_threshold_change_rule {
    /* Threshold in Authentication Request Frames per Second */
    uint16_t arfs;
    /** Percent change in threshold to apply if condition is matched */
    int16_t threshold_change;
};

struct PACKED command_cac_req {
    /** CAC subcommand */
    uint8_t cmd;
    /** Number of rules */
    uint8_t rule_tot;
    /** Threshold change rule */
    struct cac_threshold_change_rule rule[CAC_CFG_CHANGE_RULE_MAX];
};

struct PACKED command_cac_cfm {
    /** Number of rules */
    uint8_t rule_tot;
    /** Threshold change rule */
    struct cac_threshold_change_rule rule[CAC_CFG_CHANGE_RULE_MAX];
};

int cac_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Configure Centralized Authentication Control",
        args.subcmd = arg_rex0(NULL, NULL, "(get|set|enable|disable)", "{get|set|enable|disable}",
            1, "Subcommand"),
        arg_rem(NULL,
            "get - get configured rules"),
        arg_rem(NULL,
            "set - set rules (default)"),
        arg_rem(NULL,
            "enable|disable - for internal use by wpa_supplicant only"),
        args.decrease = arg_rexn("d", "decrease", "[0-9]{1,3},[0-9]{1,2}",
            "<ARFS>,<percent>", 0, CAC_CFG_CHANGE_RULE_MAX, 0,
            "Auth Req Frames per Sec above which to decrease threshold by <percent>"),
        arg_rem(NULL,
            "Decrease rules must be specified in descending ARFS order (match highest first)"),
        args.increase = arg_rexn("i", "increase", "[0-9]{1,3},[0-9]{1,2}",
            "<ARFS>,<percent>", 0, CAC_CFG_CHANGE_RULE_MAX, 0,
            "Auth Req Frames per Sec below which to increase threshold by <percent>"),
        arg_rem(NULL,
            "Increase rules must be specified in ascending ARFS order (match lowest first)"),
        arg_rem(NULL,
            "Up to 8 decrease or increase rules can be configured"),
        args.verbose = arg_lit0("v", "verbose", "Verbose output"));
    return 0;
}

static int cac_cmd_enable_or_disable(struct command_cac_req *cac_req)
{
    if (args.decrease->count || args.increase->count)
    {
        /* Assume user error - request from wpa_supplicant would not do this */
        mctrl_err("enable and disable are for internal use only\n");
        return -EINVAL;
    }

    return 0;
}

static void cac_print_rule(struct cac_threshold_change_rule *rule)
{
    bool is_decrease = (rule->threshold_change < 0);

    mctrl_print("When ARFS is %s %u, %s threshold by %d%%\n",
                is_decrease ? "greater than" : "less than",
                rule->arfs,
                is_decrease ? "decrease" : "increase",
                abs(rule->threshold_change));
}

static void cac_print_rules(struct command_cac_cfm *cac_cfm)
{
    int i;
    struct cac_threshold_change_rule *rule;

    for (i = 0; i < cac_cfm->rule_tot && i < MORSE_ARRAY_SIZE(cac_cfm->rule); i++)
    {
        rule = &cac_cfm->rule[i];
        cac_print_rule(rule);
    }
}

static int cac_add_rule_to_cmd(struct cac_threshold_change_rule *rule,
                               const char *rule_str, bool is_decrease,
                               uint16_t *arfs_prev, uint16_t *threshold_change_prev)
{
    int ret;
    unsigned int arfs;
    unsigned int threshold_change;

    ret = sscanf(rule_str, "%u,%u\n", &arfs, &threshold_change);
    if (ret != 2)
    {
        mctrl_err("Unexpected rule parse error in %s\n", rule_str);
    }

    if (arfs < 1 || arfs > CAC_CFG_ARFS_MAX)
    {
        mctrl_err("ARFS value (%u) is not between 1 and %d\n", arfs, CAC_CFG_ARFS_MAX);
        return -EINVAL;
    }

    if (threshold_change < 1 || threshold_change > CAC_CFG_CHANGE_MAX)
    {
        mctrl_err("Threshold change (%d) is not between 1%% and %d%%\n",
                  threshold_change, CAC_CFG_CHANGE_MAX);
        return -EINVAL;
    }

    if (threshold_change >= *threshold_change_prev)
    {
        mctrl_err("Threshold value (%u) for %s rule is not in descending order\n",
                  threshold_change, is_decrease ? "decrease" : "increase");
        return -EINVAL;
    }

    rule->arfs = arfs;
    if (is_decrease)
    {
        if (arfs >= *arfs_prev)
        {
            mctrl_err("ARFS value (%u) for decrease rule is not in descending order\n", arfs);
            return -EINVAL;
        }
        rule->threshold_change = -threshold_change;
    }
    else
    {
        if (arfs <= *arfs_prev)
        {
            mctrl_err("ARFS value (%u) for increase rule is not in ascending order\n", arfs);
            return -EINVAL;
        }
        rule->threshold_change = threshold_change;
    }
    *arfs_prev = arfs;
    *threshold_change_prev = threshold_change;

    if (args.verbose->count)
    {
        cac_print_rule(rule);
    }

    return 0;
}

static int cac_cmd_get(struct command_cac_req *cac_req)
{
    if (args.decrease->count || args.increase->count)
    {
        /* Assume user error - request from wpa_supplicant would not do this */
        mctrl_err("Decrease and increase parameters are invalid for the get subcommand\n");
        return -EINVAL;
    }

    return 0;
}

static int cac_cmd_set(struct command_cac_req *cac_req)
{
    int ret = 0;
    uint16_t arfs_prev;
    uint16_t threshold_change_prev;
    const char *rule_str;
    int rule_idx = 0;
    struct cac_threshold_change_rule *rule;
    int i;

    cac_req->cmd = CAC_COMMAND_CFG_SET;
    cac_req->rule_tot = args.decrease->count + args.increase->count;

    if (cac_req->rule_tot == 0)
    {
        mctrl_err("No rules specified\n");
        return -EINVAL;
    }

    if (cac_req->rule_tot > MORSE_ARRAY_SIZE(cac_req->rule))
    {
        mctrl_err("Max number of decrease and increase rules is %d\n",
                  (int)MORSE_ARRAY_SIZE(cac_req->rule));
        return -EINVAL;
    }

    /* Add decrease rules to the command
     * - ARFS must be in descending order
     * - Threshold change must be in descending order
     */
    arfs_prev = CAC_CFG_ARFS_MAX + 1;
    threshold_change_prev = CAC_CFG_CHANGE_MAX + 1;
    for (i = 0; i < args.decrease->count && ret == 0; i++)
    {
        rule_str = args.decrease->sval[i];
        rule = &cac_req->rule[rule_idx];
        ret = cac_add_rule_to_cmd(rule, rule_str, true, &arfs_prev, &threshold_change_prev);
        rule_idx++;
    }

    /* Add increase rules to the command
     * - ARFS must be in ascending order
     * - Threshold change must be in descending order
     */
    arfs_prev = 0;
    threshold_change_prev = CAC_CFG_CHANGE_MAX + 1;
    for (i = 0; i < args.increase->count && ret == 0; i++)
    {
        rule_str = args.increase->sval[i];
        rule = &cac_req->rule[rule_idx];
        ret = cac_add_rule_to_cmd(rule, rule_str, false, &arfs_prev, &threshold_change_prev);
        rule_idx++;
    }

    return ret;
}

static int cac_handle_command(struct command_cac_req *cac_req)
{
    if (strcmp(args.subcmd->sval[0], "enable") == 0)
    {
        cac_req->cmd = CAC_COMMAND_ENABLE;
        return cac_cmd_enable_or_disable(cac_req);
    }

    if (strcmp(args.subcmd->sval[0], "disable") == 0)
    {
        cac_req->cmd = CAC_COMMAND_DISABLE;
        return cac_cmd_enable_or_disable(cac_req);
    }

    if (strcmp(args.subcmd->sval[0], "get") == 0)
    {
        cac_req->cmd = CAC_COMMAND_CFG_GET;
        return cac_cmd_get(cac_req);
    }

    if (args.subcmd->count == 0 ||
        strcmp(args.subcmd->sval[0], "set") == 0)
    {
        cac_req->cmd = CAC_COMMAND_CFG_SET;
        return cac_cmd_set(cac_req);
    }

    return -EINVAL;
}

int cac(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -EINVAL;
    struct command_cac_req *cac_req = NULL;
    struct command_cac_cfm *cac_cfm = NULL;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cac_req));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cac_cfm));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cac_req = TBUFF_TO_CMD(cmd_tbuff, struct command_cac_req);
    cac_cfm = TBUFF_TO_RSP(rsp_tbuff, struct command_cac_cfm);

    ret = cac_handle_command(cac_req);
    if (ret < 0)
    {
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_CAC, cmd_tbuff,
            rsp_tbuff);

    if (ret == 0 && cac_req->cmd == CAC_COMMAND_CFG_GET)
    {
        cac_print_rules(cac_cfm);
    }

exit:
    if (cmd_tbuff)
    {
        morsectrl_transport_buff_free(cmd_tbuff);
    }

    if (rsp_tbuff)
    {
        morsectrl_transport_buff_free(rsp_tbuff);
    }

    return ret;
}

MM_CLI_HANDLER(cac, MM_INTF_REQUIRED, MM_DIRECT_CHIP_NOT_SUPPORTED);
