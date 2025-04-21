/*
 * Copyright 2022 Morse Micro
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#include "command.h"
#include "utilities.h"
#include "transport/transport.h"
#include "channel.h"

/** The maximum allowed length of a user specified payload (bytes) in the status frames */
#define STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN  (64)
/** The maximum allowed length of a user filter to apply to wake frames */
#define STANDBY_WAKE_FRAME_USER_FILTER_MAX_LEN     (64)

/** Max line length for a config file line */
#define MAX_LINE_LENGTH     (255)

typedef enum
{
    /** The external host is indicating that it's now awake */
    STANDBY_MODE_CMD_EXIT = 0x0,
    /** The external host is indicating that it's going into standby mode */
    STANDBY_MODE_CMD_ENTER,
    /** This version of the config command has since been deprecated */
    STANDBY_MODE_CMD_SET_CONFIG_V1_DEPRECATED,
    /** The external host provides a payload that gets appended to status frames */
    STANDBY_MODE_CMD_SET_STATUS_PAYLOAD,
    /** The external host provides a filter to be applied to incoming standby wake frames */
    STANDBY_MODE_CMD_SET_WAKE_FILTER,
    /** The external host sets a number of configuration options for standby mode */
    STANDBY_MODE_CMD_SET_CONFIG_V2,

    /** Force enum to UINT32 */
    STANDBY_MODE_CMD_MAX = UINT32_MAX,
} standby_mode_commands_t;

struct PACKED command_standby_set_config
{
    /** Interval for transmitting Standby status packets */
    uint32_t notify_period_s;
    /** Time for firmware to wait during beacon loss before entering deep sleep in seconds */
    uint32_t bss_inactivity_before_deep_sleep_s;
    /** Time for firmware to remain in deep sleep in seconds */
    uint32_t deep_sleep_period_s;
    /** Source IP address */
    ipv4_addr_t src_ip;
    /** Destination IP address */
    ipv4_addr_t dst_ip;
    /** Destination UDP Port */
    uint16_t dst_port;
    /** pad to word boundary */
    uint8_t pad[2];
    /** Time in seconds to increment each successive deep sleep */
    uint32_t deep_sleep_increment_s;
    /** Max time to deep sleep for */
    uint32_t deep_sleep_max_s;
};

struct PACKED command_standby_set_wake_filter
{
    /** The length of the wake filter */
    uint32_t len;
    /** The offset at which to apply the wake filter */
    uint32_t offset;
    /** The user-defined filter */
    uint8_t filter[STANDBY_WAKE_FRAME_USER_FILTER_MAX_LEN];
};

struct PACKED command_standby_set_status_payload
{
    /** The length of the payload */
    uint32_t len;
    /** The payload */
    uint8_t payload[STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN];
};

struct PACKED command_standby_enter
{
    /** The BSSID to monitor for activity (or lack thereof) before entering deep sleep */
    uint8_t bssid[MAC_ADDR_LEN];
};

/**
 * Structure for Configuring MM standby mode
 */
struct PACKED command_standby_mode_req
{
    /** Standby Mode subcommands, see @ref standby_mode_commands_t */
    uint32_t cmd;
    union {
        uint8_t opaque[0];
        /** Valid for STANDBY_MODE_CMD_SET_CONFIG cmd */
        struct command_standby_set_config config;
        /** Valid for STANDBY_MODE_CMD_SET_STATUS_PAYLOAD cmd */
        struct command_standby_set_status_payload set_payload;
        /** Valid for STANDBY_MODE_CMD_ENTER cmd*/
        struct command_standby_enter enter;
        /** Valid for STANDBY_MODE_CMD_SET_WAKE_FILTER cmd */
        struct command_standby_set_wake_filter set_filter;
    };
};

typedef enum
{
    /** No specific reason for exiting standby mode */
    STANDBY_MODE_EXIT_REASON_NONE,
    /** The STA has received the wakeup frame */
    STANDBY_MODE_EXIT_REASON_WAKEUP_FRAME,
    /** The STA needs to associate */
    STANDBY_MODE_EXIT_REASON_ASSOCIATE,
    /** The STA's external input pin has fired */
    STANDBY_MODE_EXIT_REASON_EXT_INPUT,
    /** Whitelisted packet received */
    STANDBY_MODE_EXIT_REASON_WHITELIST_PKT,
    /** TCP connection lost */
    STANDBY_MODE_EXIT_REASON_TCP_CONNECTION_LOST,
    /** HW scan is not enabled */
    STANDBY_MODE_EXIT_REASON_HW_SCAN_NOT_ENABLED,
    /** HW scan failed to start */
    STANDBY_MODE_EXIT_REASON_HW_SCAN_FAILED_TO_START,
} standby_mode_exit_reason_t;


struct command_standby_mode_exit {
    /** Valid for a response to STANDBY_MODE_CMD_EXIT, see @ref standby_mode_exit_reason_t */
    uint8_t reason;
    /** STA state at the time of exit */
    uint8_t sta_state;
};
/**
 * Response structure for MM standby mode command
 */
struct PACKED command_standby_mode_cfm
{
    union {
        uint8_t opaque[0];
        struct command_standby_mode_exit info;
    };
};

struct standby_config_parse_context
{
    struct command_standby_set_config *set_cfg;
    struct command_standby_set_wake_filter *filter_cfg;
};

struct standby_session_parse_context
{
    uint8_t *bssid;
    struct command_set_channel_req *req;
};

static struct {
    struct arg_rex *command;
} args;

static struct mm_argtable enter;
static struct mm_argtable exit_cmd;
static struct mm_argtable payload;
static struct mm_argtable config;
static struct mm_argtable store;

static struct mm_argtable *subcmds[] =
{
    &enter, &exit_cmd, &payload, &config, &store
};

static struct {
    struct arg_file *session_dir;
} enter_args;

static struct {
    struct arg_rem *desc;
    struct arg_lit *json_format;
} exit_args;

static struct {
    struct arg_str *data;
} payload_args;

static struct {
    struct arg_file *file;
} config_args;

static struct {
    struct arg_rex *bssid;
    struct arg_file *dir;
} store_args;

static int standby_get_cmd(const char str[])
{
    if (strcmp("enter", str) == 0) return STANDBY_MODE_CMD_ENTER;
    else if (strcmp("exit", str) == 0) return STANDBY_MODE_CMD_EXIT;
    else if (strcmp("config", str) == 0) return STANDBY_MODE_CMD_SET_CONFIG_V2;
    else if (strcmp("payload", str) == 0) return STANDBY_MODE_CMD_SET_STATUS_PAYLOAD;
    else
    {
        return -1;
    }
}

int standby_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    if (mors->debug)
    {
        MM_INIT_ARGTABLE(mm_args, "Control standby state and configuration",
            args.command = arg_rex1(NULL, NULL, "(enter|exit|payload|config|store)",
                "{enter|exit|config|payload|store}", 0, "Standby subcommand"));
    }
    else
    {
        MM_INIT_ARGTABLE(mm_args, "Control standby state and configuration",
            args.command = arg_rex1(NULL, NULL, "(enter|exit|payload|config|store)",
                "{enter|exit|config|payload}", 0, "Standby subcommand"));
    }
    args.command->hdr.flag |= ARG_STOPPARSE;

    MM_INIT_ARGTABLE(&enter, "Put the STA FW into standby mode",
        enter_args.session_dir = arg_file0(NULL, NULL, "<session dir>",
            "The full directory path for storing persistent sessions"),
        arg_rem(NULL, "Obtained from wpa_supplicant standby_config_dir configuration parameter"),
        arg_rem(NULL, "No longer required and is retained for backwards compatibility"));

    MM_INIT_ARGTABLE(&exit_cmd, "Tell the firmware that the host is awake",
        exit_args.desc = arg_rem(NULL, "Firmware responds with one of the following reason codes"),
                        arg_rem(NULL, "0 - none"),
                        arg_rem(NULL, "1 - wake-up frame received"),
                        arg_rem(NULL, "2 - association lost"),
                        arg_rem(NULL, "3 - external input pin fired"),
                        arg_rem(NULL, "4 - whitelisted packet received"),
                        arg_rem(NULL, "6 - TCP connection lost"),
                        arg_rem(NULL, "A message is printed in the following format."),
                        arg_rem(NULL, "Standby mode exited with reason <code> - <description>"),
        exit_args.json_format = arg_lit0("j", "json", "Print the exit message in JSON format"));

    MM_INIT_ARGTABLE(&payload, "Data to append to standby status frames",
        payload_args.data = arg_str1(NULL, NULL, "<hex string>",
            "Hex string of user data to append to standby status frames"));

    MM_INIT_ARGTABLE(&config, "Configure standby mode",
        config_args.file = arg_file1(NULL, NULL, "<config file>",
            "Path to file containing standby mode configuration parameters"));

    MM_INIT_ARGTABLE(&store,
        "Store session information when associated (internal use only)",
            store_args.bssid = arg_rex1("b", NULL, "([a-f0-9]{2}:){5}([a-f0-9]{2})",
                "<BSSID MAC Address>", ARG_REX_ICASE, "Association BSSID"),
            store_args.dir = arg_file1("d", NULL, "<dir>",
                "The full directory path for storing persistent sessions"));

    return 0;
}

int standby_help(void)
{
    mm_help_argtable("standby enter", &enter);
    mm_help_argtable("standby exit", &exit_cmd);
    mm_help_argtable("standby payload", &payload);
    mm_help_argtable("standby config", &config);
    mm_help_argtable("standby store", &store);
    return 0;
}

static int parse_standby_config_keyval(struct morsectrl *mors, void *context, const char *key,
                                    const char *val)
{
    struct standby_config_parse_context *config = context;
    uint32_t temp;
    ipv4_addr_t temp_ip;

    if (mors->debug)
    {
        mctrl_print("standby_config: %s - %s\n", key, val);
    }

    if (strcmp("notify_period_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->notify_period_s = htole32(temp);
        return 0;
    }
    else if (strcmp("bss_inactivity_before_deep_sleep_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->bss_inactivity_before_deep_sleep_s = htole32(temp);
        return 0;
    }
    else if (strcmp("deep_sleep_period_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_period_s = htole32(temp);
        return 0;
    }
    else if (strcmp("src_ip", key) == 0)
    {
        if (str_to_ip(val, &temp_ip) < 0)
        {
            goto error;
        }
        memcpy(&config->set_cfg->src_ip, &temp_ip, sizeof(config->set_cfg->src_ip));
        return 0;
    }
    else if (strcmp("dest_ip", key) == 0)
    {
        if (str_to_ip(val, &temp_ip) < 0)
        {
            goto error;
        }
        memcpy(&config->set_cfg->dst_ip, &temp_ip, sizeof(config->set_cfg->dst_ip));
        return 0;
    }
    else if (strcmp("dest_port", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->dst_port = htole16((uint16_t) temp);
        return 0;
    }
    else if (strcmp("deep_sleep_increment_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_increment_s = htole32(temp);
        return 0;
    }
    else if (strcmp("deep_sleep_max_s", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->set_cfg->deep_sleep_max_s = htole32(temp);
        return 0;
    }
    else if (strcmp("wake_packet_filter", key) == 0)
    {
        uint32_t len = MIN(strlen(val) / 2, MORSE_ARRAY_SIZE(config->filter_cfg->filter));
        hexstr2bin(val, config->filter_cfg->filter, len);
        config->filter_cfg->len = htole32(len);
        return 0;
    }
    else if (strcmp("wake_packet_filter_offset", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        config->filter_cfg->offset = htole32(temp);
        return 0;
    }
    mctrl_err("Key is not a recognised parameter: %s\n", key);
    return 0;

error:
    mctrl_err("Failed to parse value for %s (val: %s)\n", key, val);
    return -1;
}

static int parse_standby_session_keyval(struct morsectrl *mors, void *context, const char *key,
                                            const char *val)
{
    struct standby_session_parse_context *ctx = context;
    uint32_t temp;

    if (mors->debug)
    {
        mctrl_print("standby_session: %s - %s\n", key, val);
    }

    if (strcmp("bssid", key) == 0)
    {
        if (str_to_mac_addr(ctx->bssid, val) < 0)
        {
            goto error;
        }
        return 0;
    }
    else if (strcmp("op_chan_freq", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->operating_channel_freq_hz = htole32(temp);
        return 0;
    }
    else if (strcmp("op_chan_bw", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->operating_channel_bw_mhz = (uint8_t) temp;
        return 0;
    }
    else if (strcmp("pri_chan_bw", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->primary_channel_bw_mhz = (uint8_t) temp;
        return 0;
    }
    else if (strcmp("pri_1mhz_chan", key) == 0)
    {
        if (str_to_uint32(val, &temp) < 0)
        {
            goto error;
        }
        ctx->req->primary_1mhz_channel_index = (uint8_t) temp;
        return 0;
    }

    mctrl_err("Key is not a recognised parameter: %s\n", key);
    return 0;

error:
    mctrl_err("Failed to parse value for %s (val: %s)\n", key, val);
    return -1;
}

/**
 * @brief Generic config file parser.
 * Scans lines and calls a handler function for each `key=value` it finds.
 * Lines starting with a `#` will be interpreted as a comment and ignored.
 *
 * @param mors Morsectrl object
 * @param conf_file File to parse
 * @param keyval_process Callback function called for each key value
 * @param context Context pointer for callback function
 * @return int 0 if successfully parsed, otherwise negative value
 */
static int config_parse(struct morsectrl *mors, const char *conf_file,
                        int (*keyval_process)(
                                    struct morsectrl *mors,
                                    void *context,
                                    const char *key,
                                    const char *val),
                        void *context)
{
    int ret;
    FILE *cfg_file;
    char line[MAX_LINE_LENGTH];
    uint16_t line_num = 0;

    if (!conf_file || !keyval_process || is_dir(conf_file))
        return -1;

    cfg_file = fopen(conf_file, "r");

    if (cfg_file == NULL)
        return -1;

    while (fgets(line, MAX_LINE_LENGTH, cfg_file))
    {
        char *key, *val;

        line_num++;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        key = strip(line);
        val = strchr(line, '=');

        if (val)
            *val++ = '\0';

        if (key && key[0] && val && val[0])
        {
            ret = keyval_process(mors, context, key, val);
            if (ret)
            {
                fclose(cfg_file);
                return ret;
            }
        }
        else
        {
            mctrl_err("No key=value on line %d\n", line_num);
            fclose(cfg_file);
            return -1;
        }
    }

    fclose(cfg_file);
    return 0;
}

static int standby_session_store(struct morsectrl *mors, const char *ifname, const uint8_t *bssid,
    const char *standby_session_dir, struct command_get_channel_cfm *rsp)
{
    FILE *f;
    char dir[MORSE_FILENAME_LEN_MAX];
    char fname[MORSE_FILENAME_LEN_MAX];
    struct stat buf;

    if (snprintf(dir, sizeof(dir), "%s", standby_session_dir) < 0)
    {
        mctrl_err("%s: Failed to set dir name (%d)\n",
                dir, errno);
        return -1;
    }

    if (stat(dir, &buf) != 0) {
        if (mkdir_path(dir) < 0)
        {
            mctrl_err("%s: Failed to create %s (%d)\n",
                    ifname, dir, errno);
            return -1;
        }
    }

    if (snprintf(fname, sizeof(fname), "%s/%s", dir, ifname) < 0) {
        mctrl_err("%s: Failed to set file name (%d)\n",
                ifname, errno);
        return -1;
    }

    f = fopen(fname, "w");
    if (!f) {
        mctrl_err("%s: Failed to open %s\n",
                ifname, fname);
        return -1;
    }

    fprintf(f, "bssid=" MACSTR "\n", MAC2STR(bssid));
    fprintf(f, "op_chan_freq=%u\n", rsp->operating_channel_freq_hz);
    fprintf(f, "op_chan_bw=%u\n", rsp->operating_channel_bw_mhz);
    fprintf(f, "pri_chan_bw=%u\n", rsp->primary_channel_bw_mhz);
    fprintf(f, "pri_1mhz_chan=%u\n", rsp->primary_1mhz_channel_index);

    fclose(f);

    if (mors->debug)
    {
        mctrl_print("%s: Created %s\n", ifname, fname);
    }

    return 0;
}

static int standby_session_load(struct morsectrl *mors, const char *standby_session_dir,
        uint8_t *bssid, struct command_set_channel_req *req)
{
    const char *ifname = morsectrl_transport_get_ifname(mors->transport);
    struct standby_session_parse_context context = {
        .bssid = bssid,
        .req = req
    };

    char session_path[MORSE_FILENAME_LEN_MAX];

    if (snprintf(session_path, sizeof(session_path),
                "%s/%s", standby_session_dir, ifname) < 0)
    {
        mctrl_err("%s: Failed to set session path name (%d)\n",
                ifname, errno);
        return -1;
    }

    if (config_parse(mors, session_path, parse_standby_session_keyval, &context))
    {
        goto err_parse;
    }

    return 0;

err_parse:
    mctrl_err("%s: Failed to parse %s\n", ifname, standby_session_dir);
    return -1;
}

static int process_standby_enter(struct morsectrl *mors,
                                    struct command_standby_mode_req *standby_cmd,
                                    int argc, char *argv[])
{
    const char *standby_session_dir = NULL;
    int ret;
    struct command_set_channel_req *ch_cmd;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    ret = mm_parse_argtable("standby enter", &enter, argc, argv);
    if (ret != 0)
    {
        goto exit;
    }

    if (enter_args.session_dir->count)
    {
        standby_session_dir = enter_args.session_dir->filename[0];
    }
    else
    {
        return 0;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*ch_cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, 0);

    if (!cmd_tbuff || !rsp_tbuff)
    {
        mctrl_err("Alloc failure\n");
        ret = -1;
        goto exit;
    }

    ch_cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);

    /* load the saved standby parameters */
    ret = standby_session_load(mors, standby_session_dir, standby_cmd->enter.bssid, ch_cmd);
    if (ret < 0)
    {
        mctrl_err("Failed to load session info\n");
        goto exit;
    }

    if (mors->debug)
    {
        mctrl_print("Loaded session info:\n");
        mctrl_print("bssid " MACSTR "\n", MAC2STR(standby_cmd->enter.bssid));
        mctrl_print("op ch freq %d\n", ch_cmd->operating_channel_freq_hz);
        mctrl_print("op ch bw %d\n", ch_cmd->operating_channel_bw_mhz);
        mctrl_print("pri ch bw %d\n", ch_cmd->primary_channel_bw_mhz);
        mctrl_print("pri 1mhz idx %d\n", ch_cmd->primary_1mhz_channel_index);
    }

    /* Set the channel before we go to sleep */
    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_SET_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        mctrl_err("Failed to set channel info\n");
        goto exit;
    }

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

static int process_standby_exit(struct morsectrl *mors,
                                    int argc, char *argv[])
{
    return mm_parse_argtable("standby exit", &exit_cmd, argc, argv);
}

/* Standby store commands are invoked from wpa_supplicant, so provide some context for error
 * messages.
 */
static void standby_store_print_msg(const char *msg)
{
    mctrl_err("morsectrl standby store failed - %s\n", msg);
}

static int standby_store_session_cmd(struct morsectrl *mors, int argc, char *argv[])
{
    uint8_t bssid[MAC_ADDR_LEN] = { 0 };
    const char *standby_session_dir = NULL;
    int ret;
    const char *ifname = morsectrl_transport_get_ifname(mors->transport);
    struct command_set_channel_req *cmd;
    struct command_get_channel_cfm *rsp;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;

    ret = mm_parse_argtable("standby store", &store, argc, argv);
    if (ret != 0)
    {
        goto exit;
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (ifname == NULL)
    {
        standby_store_print_msg("no interface - transport not supported");
        ret = -1;
        goto exit;
    }

    if (!cmd_tbuff || !rsp_tbuff)
    {
        standby_store_print_msg("alloc failure");
        ret = -1;
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_set_channel_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_get_channel_cfm);

    if (str_to_mac_addr(bssid, store_args.bssid->sval[0]) < 0)
    {
        /* Shouldn't get here with regexp parsing above */
        ret = -1;
        goto exit;
    }

    standby_session_dir = store_args.dir->filename[0];

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_GET_FULL_CHANNEL,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        standby_store_print_msg("failed to get channel info");
        ret = -1;
        goto exit;
    }

    ret = standby_session_store(mors, ifname, bssid, standby_session_dir, rsp);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);

    return ret;
}

static int send_wake_filter_cmd(struct morsectrl *mors,
                                    struct command_standby_set_wake_filter *wake_cmd)
{
    int ret = -1;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct command_standby_mode_req *cmd;
    struct command_standby_mode_cfm *rsp = NULL;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_standby_mode_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_standby_mode_cfm);

    cmd->cmd = STANDBY_MODE_CMD_SET_WAKE_FILTER;

    memcpy(&cmd->set_filter, wake_cmd, sizeof(*wake_cmd));

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_STANDBY_MODE,
        cmd_tbuff, rsp_tbuff);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

static int process_set_config_cmd(struct morsectrl *mors, struct command_standby_mode_req* cmd,
                                    int argc, char *argv[])
{
    struct command_standby_set_wake_filter wake_filter = {0};
    static const struct command_standby_set_config config_default = {
        .bss_inactivity_before_deep_sleep_s = 60,
        .deep_sleep_period_s = 120,
        .notify_period_s = 15,
        .dst_ip.as_u32 = 0,
        .dst_port = 22000,
        .src_ip.as_u32 = 0,
        .deep_sleep_increment_s = 0, /* Default no increment */
        .deep_sleep_max_s = UINT32_MAX, /* Default max int / no max */
    };
    int ret;

    ret = mm_parse_argtable("standby config", &config, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    memcpy(&cmd->config, &config_default, sizeof(config_default));

    struct standby_config_parse_context context = {
        .filter_cfg = &wake_filter,
        .set_cfg = &cmd->config
    };

    if (config_parse(mors, config_args.file->filename[0], parse_standby_config_keyval, &context))
    {
        mctrl_err("Failed to parse config file\n");
        return -1;
    }

    /* If the wake filter has been configured, send the command to set it here. */
    if (wake_filter.len != 0)
    {
        return send_wake_filter_cmd(mors, &wake_filter);
    }

    return 0;
}

static int process_set_status_payload(struct command_standby_mode_req* cmd, int argc, char *argv[])
{
    uint32_t payload_len;

    int ret;
    ret = mm_parse_argtable("standby payload", &payload, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    payload_len = strlen(payload_args.data->sval[0]);
    if ((payload_len % 2) != 0)
    {
        mctrl_err("Invalid hex string, length must be a multiple of 2\n");
        return -1;
    }

    payload_len = (payload_len / 2);
    if (payload_len > STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN)
    {
        mctrl_err("Supplied payload is too large: %d > %d\n", payload_len,
            STANDBY_STATUS_FRAME_USER_PAYLOAD_MAX_LEN);
        return -1;
    }

    cmd->set_payload.len = payload_len;
    for (int i = 0; i < payload_len; i++)
    {
        ret = sscanf(&(payload_args.data->sval[0][i * 2]), "%2hhx", &cmd->set_payload.payload[i]);
        if (ret != 1)
        {
            mctrl_err("Invalid hex string\n");
            return -1;
        }
    }

    return 0;
}

static const char *standby_exit_reason_to_str(int reason)
{
    switch (reason) {
    case STANDBY_MODE_EXIT_REASON_NONE:
        return "none";
    case STANDBY_MODE_EXIT_REASON_WAKEUP_FRAME:
        return "wake-up frame received";
    case STANDBY_MODE_EXIT_REASON_ASSOCIATE:
        return "association lost";
    case STANDBY_MODE_EXIT_REASON_EXT_INPUT:
        return "external input pin fired";
    case STANDBY_MODE_EXIT_REASON_WHITELIST_PKT:
        return "whitelisted packet received";
    case STANDBY_MODE_EXIT_REASON_TCP_CONNECTION_LOST:
        return "TCP connection lost";
    case STANDBY_MODE_EXIT_REASON_HW_SCAN_NOT_ENABLED:
        return "HW scan not enabled";
    case STANDBY_MODE_EXIT_REASON_HW_SCAN_FAILED_TO_START:
        return "HW scan failed to start";
    default:
        return "unknown";
    }
}

int standby(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    int i = 0;
    bool json = false;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    struct command_standby_mode_req *cmd;
    struct command_standby_mode_cfm *rsp = NULL;

    /* Local-only command - not sent to firmware */
    if (strcmp("store", args.command->sval[0]) == 0)
    {
        return standby_store_session_cmd(mors, argc, argv);
    }

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*rsp));

    if (!cmd_tbuff || !rsp_tbuff)
    {
        goto exit;
    }

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_standby_mode_req);
    rsp = TBUFF_TO_RSP(rsp_tbuff, struct command_standby_mode_cfm);

    cmd->cmd = standby_get_cmd(args.command->sval[0]);

    switch (cmd->cmd)
    {
        case STANDBY_MODE_CMD_SET_CONFIG_V2:
        {
            ret = process_set_config_cmd(mors, cmd, argc, argv);
            if (ret)
            {
                goto exit;
            }

            if (mors->debug)
            {
                mctrl_print("Setting standby configuration:\n");
                mctrl_print("deep sleep inactivity period: %d\n",
                        cmd->config.bss_inactivity_before_deep_sleep_s);
                mctrl_print("deep_sleep period: %d\n", cmd->config.deep_sleep_period_s);
                mctrl_print("notify period : %d\n", cmd->config.notify_period_s);
                mctrl_print("dst port: %d\n", cmd->config.dst_port);
                mctrl_print("dst ip: " IPSTR "\n", IP2STR(cmd->config.dst_ip.octet));
                mctrl_print("src ip: " IPSTR "\n", IP2STR(cmd->config.src_ip.octet));
            }

            break;
        }
        case STANDBY_MODE_CMD_SET_STATUS_PAYLOAD:
        {
            ret = process_set_status_payload(cmd, argc, argv);
            if (ret)
            {
                goto exit;
            }

            break;
        }
        case STANDBY_MODE_CMD_ENTER:
        {
            ret = process_standby_enter(mors, cmd, argc, argv);
            if (ret)
            {
                goto exit;
            }
            break;
        }
        case STANDBY_MODE_CMD_EXIT:
            ret = process_standby_exit(mors, argc, argv);
            if (ret)
            {
                goto exit;
            }
            json = (exit_args.json_format->count > 0);
            break;
        default:
            break;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_STANDBY_MODE,
        cmd_tbuff, rsp_tbuff);

    if (cmd->cmd == STANDBY_MODE_CMD_EXIT && ret == 0)
    {
        if (json)
        {
            mctrl_print("[");
            mctrl_print("{\"Standby mode exited with reason\": %u - %s}",
                    rsp->info.reason, standby_exit_reason_to_str(rsp->info.reason));
            mctrl_print("]\n");
        }
        else
        {
            mctrl_print("Standby mode exited with reason %u - %s\n", rsp->info.reason,
                standby_exit_reason_to_str(rsp->info.reason));
        }
    }

exit:
    /* Check if the reason we got here is because --help was given */
    if (mm_check_help_argtable(subcmds, MORSE_ARRAY_SIZE(subcmds)))
    {
        ret = 0;
    }

    for (i = 0; i < MORSE_ARRAY_SIZE(subcmds); i++)
    {
        mm_free_argtable(subcmds[i]);
    }

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER_CUSTOM_HELP(standby, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
