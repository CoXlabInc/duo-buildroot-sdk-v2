/*
 * Copyright 2024 Morse Micro
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "portable_endian.h"
#include "command.h"
#include "utilities.h"
#include "mm_argtable3.h"
#include "stats_format.h"

#define MAX_GAIN_CONFIGS 128

/** Calibration types available in the system, values can be or'ed together to create a mask */
typedef enum {
    /** Temperature calibration */
    CALIB_TYPE_TEMPERATURE   = BIT(0),
    /** VBAT calibration */
    CALIB_TYPE_VBAT          = BIT(1),
    /** AON clock calibration */
    CALIB_TYPE_AON_CLK       = BIT(2),
    /** DC calibration */
    CALIB_TYPE_DC            = BIT(3),
    /** I/Q calibration */
    CALIB_TYPE_TX_IQ         = BIT(4),
    /** TX power calibration */
    CALIB_TYPE_TX_POWER      = BIT(5),
    /** TX power control */
    CALIB_TYPE_TX_POWER_CTRL = BIT(6),

    /** Special calibration type for testing purposes */
    CALIB_TYPE_SPOOF_TEST  = BIT(30),

    /** Last / MAX */
    CALIB_TYPE_ALL = (CALIB_TYPE_TEMPERATURE | CALIB_TYPE_VBAT |
        CALIB_TYPE_AON_CLK | CALIB_TYPE_DC | CALIB_TYPE_TX_IQ | CALIB_TYPE_SPOOF_TEST),
    CALIB_TYPE_MAX = UINT32_MAX
} calib_type_t;

/** Structure to send phy calibration commands to chip */
struct PACKED cal_command
{
    /** Bitmask to run particular calibration */
    uint32_t cal_mask;
    /** Flag to run calibrations on demand */
    bool run_cal;
    /** Flag to indicate periodic cal setting */
    bool set_periodic_cal;
};

/** Structure to store idc and qdc values */
struct PACKED coarse_dc_t
{
    uint8_t idc;
    uint8_t qdc;
};

/** Structure to store response from chip */
struct PACKED cal_cfm
{
    uint32_t per_cal_mask;
    uint32_t vbat_mv;
    uint32_t tx_temp_pwr_scale;
    uint32_t tx_power_drift_lin;
    uint32_t aon_clk_freq_hz;
    int32_t temperature_c;
    int32_t tx_phase_imb_rad;
    int32_t tx_idc;
    int32_t tx_qdc;
    uint8_t num_dccal_entries;
    struct coarse_dc_t coarse_dc[0];
};

static struct args_struct {
    struct arg_lit *temp_cal;
    struct arg_lit *vbat_cal;
    struct arg_lit *aon_clk_cal;
    struct arg_lit *tx_iq_cal;
    struct arg_lit *tx_pwr_temp_cal;
    struct arg_lit *tx_pwr_ctrl_cal;
    struct arg_lit *rx_dc_cal;
    struct arg_rex *periodic;
} args;

int cal_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Run calibrations",
        args.temp_cal = arg_lit0("t", "temperature",
            "Measure on-chip temperature"),
        args.vbat_cal = arg_lit0("v", "vbat",
            "Measure Vbat voltage"),
        args.aon_clk_cal = arg_lit0("a", "aonclk",
            "Calibrate AON (always-on) clock"),
        args.tx_iq_cal = arg_lit0("i", "txiq",
            "Calibrate TX IQ phase and LOFT"),
        args.tx_pwr_temp_cal = arg_lit0("p", "txpwr",
            "Calibrate TX power across temperature"),
        args.tx_pwr_ctrl_cal = arg_lit0("c", "txpwrctrl",
            "Run TX power control"),
        args.rx_dc_cal = arg_lit0("d", "rxdc",
            "Calibrate RX DC offset"),
        args.periodic = arg_rex0("P", "periodic", MM_ARGTABLE_ENABLE_REGEX,
            MM_ARGTABLE_ENABLE_DATATYPE, 0, "Enable/disable specific periodic calibrations"));
    return 0;
}

static void stats_print_dccal_array(const char *key,
        struct coarse_dc_t* coarse_dc, uint32_t array_len)
{
    int indent_level = 1;
    mctrl_print("%*s%-*s: 0,%d,%d\n",
            indent_level * INDENT_LEN, "",
            LABEL_LEN - (indent_level * INDENT_LEN),
            key, coarse_dc[0].idc, coarse_dc[0].qdc);
    indent_level = 12;
    for (int i = 1; i < array_len; i++)
    {
        mctrl_print("%*s  %" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n",
                indent_level * INDENT_LEN, "", i, coarse_dc[i].idc, coarse_dc[i].qdc);
    }
}

static float rad_to_deg(int32_t phase_rad_f16)
{
    float phase_rad_f = phase_rad_f16 / 65536.0;
    float phase_deg = (phase_rad_f * 180) / 3.142;
    return phase_deg;
}

static void process_response(struct cal_command* cal_cmd,
        struct cal_cfm* cal_cfm, uint32_t cal_mask)
{
    if (!cal_cmd->run_cal)
    {
        cal_mask = CALIB_TYPE_ALL;
    }
    mctrl_print("Calibration response:\n");
    stats_print_unsigned("Periodic temperature cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_TEMPERATURE), 1);
    stats_print_unsigned("Periodic Vabt cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_VBAT), 1);
    stats_print_unsigned("Periodic AON cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_AON_CLK), 1);
    stats_print_unsigned("Periodic TX IQ & LOFT cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_TX_IQ), 1);
    stats_print_unsigned("Periodic TX power vs temp cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_TX_POWER), 1);
    stats_print_unsigned("Periodic TX power control enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_TX_POWER_CTRL), 1);
    stats_print_unsigned("Periodic RX DC cal enable",
            BMGET(cal_cfm->per_cal_mask, CALIB_TYPE_DC), 1);
    if (cal_mask & CALIB_TYPE_TEMPERATURE)
    {
        stats_print_signed("Temperature (C)", cal_cfm->temperature_c, 1);
    }
    if (cal_mask & cal_mask & CALIB_TYPE_VBAT)
    {
        stats_print_unsigned("Vbat (mV)", cal_cfm->vbat_mv, 1);
    }
    if (cal_mask & CALIB_TYPE_AON_CLK)
    {
        stats_print_unsigned("AON clock frequency (Hz)", cal_cfm->aon_clk_freq_hz, 1);
    }
    if (cal_mask & CALIB_TYPE_TX_IQ)
    {
        stats_print_float("TX phase imbalance (deg)",
                (float) rad_to_deg(cal_cfm->tx_phase_imb_rad), 1);
        stats_print_signed("TX I path DC", cal_cfm->tx_idc, 1);
        stats_print_signed("TX Q path DC", cal_cfm->tx_qdc, 1);
    }
    if (cal_mask & CALIB_TYPE_TX_POWER)
    {
        stats_print_float("TX temp based power scale (dB)",
                10 * log10(cal_cfm->tx_temp_pwr_scale / 65536.0), 1);
    }
    if (cal_mask & CALIB_TYPE_TX_POWER_CTRL)
    {
        stats_print_float("TX power drift (dB)",
                10 * log10(cal_cfm->tx_power_drift_lin / 65536.0), 1);
    }
    if (cal_mask & CALIB_TYPE_DC)
    {
        stats_print_dccal_array("Gain config, IDC, QDC", &cal_cfm->coarse_dc[0],
                cal_cfm->num_dccal_entries);
    }
}

uint32_t construct_cal_mask(struct args_struct * args, bool enable, uint32_t mask)
{
    if (args->temp_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_TEMPERATURE, enable);
    }
    if (args->vbat_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_VBAT, enable);
    }
    if (args->aon_clk_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_AON_CLK, enable);
    }
    if (args->tx_iq_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_TX_IQ, enable);
    }
    if (args->tx_pwr_temp_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_TX_POWER, enable);
    }
    if (args->tx_pwr_ctrl_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_TX_POWER_CTRL, enable);
    }
    if (args->rx_dc_cal->count)
    {
        mask = BMUPD(mask, CALIB_TYPE_DC, enable);
    }
    return mask;
}

int cal(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint32_t cal_mask = 0;
    struct cal_command *cmd = {0};
    struct cal_cfm *cfm = {0};
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm) +
        MAX_GAIN_CONFIGS * sizeof(struct coarse_dc_t));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct cal_command);
    cfm = TBUFF_TO_RSP(rsp_tbuff, struct cal_cfm);

    cmd->set_periodic_cal = false;
    cmd->run_cal = false;

    if (args.periodic->count)
    {
        ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_RUN_PHY_CAL,
                                 cmd_tbuff, rsp_tbuff);
        if (ret < 0)
        {
            goto exit;
        }
        cmd->set_periodic_cal = true;
        cal_mask = construct_cal_mask(&args,
                !strcmp("enable", args.periodic->sval[0]), cfm->per_cal_mask);
    }
    else
    {
        cal_mask = construct_cal_mask(&args, true, cal_mask);
        if (cal_mask)
        {
            cmd->run_cal = true;
        }
    }
    cmd->cal_mask = htole32(cal_mask);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_RUN_PHY_CAL,
                                 cmd_tbuff, rsp_tbuff);
    process_response(cmd, cfm, cal_mask);

exit:
    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(cal, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
