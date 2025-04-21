/*
 * Copyright 2022 Morse Micro
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

/** TX power adjustments available in the system, values can be or'ed together to create a mask */
enum tx_pwr_adj_type {
    /** MCS based adjustment */
    TX_PWR_ADJ_MCS             = (1 << 0),
    /** Sub band adjustment */
    TX_PWR_ADJ_SUBBAND         = (1 << 1),
    /** Temperature adjustment */
    TX_PWR_ADJ_TEMPERATURE     = (1 << 2),
    /** Channel based adjustment */
    TX_PWR_ADJ_CHANNEL_PWR_LIM = (1 << 3),
    /** Transmit power control */
    TX_PWR_ADJ_TX_PWR_CTRL     = (1 << 4),
    /** Channel power adjustment */
    TX_PWR_ADJ_PWR_FREQ        = (1 << 5),
    /** MAX */
    TX_PWR_ADJ_MAX = UINT32_MAX
};

/** Structure to send power adjustment commands to chip */
struct PACKED tx_pwr_adj_command
{
    /** The flags to se/get power adjustments */
    uint8_t flag;
    /** Bitmask to enable/disable power adujstments */
    uint32_t en_tx_pwr_adj_mask;
};
/** Old structure to store response from chip */
struct PACKED tx_pwr_adj_cfm_v1
{
    /** Bit mask variable to specify enable/disable of tx power adjustment */
    uint32_t en_tx_pwr_adj_mask;
    /** Sub-band 1in8 scale values in qdB */
    int32_t sb_1in_8[4];
    /** Sub-band 2in8 scale values in qdB */
    int32_t sb_2in_8[2];
    /** Teperature based scaler value in linear scale */
    int32_t temp_power_scaler;
    /** Current sub-band scale value in linear scale */
    int32_t subband_scale;
    /** txscaler value in linear scale */
    int32_t tx_linear_power_scaler;
    /** Transmit power control */
    uint32_t tx_power_drift_lin;
    /** Chip's base power in qdBm */
    int8_t base_power_qdbm;
    /** Chip's max power in qdBm */
    int8_t max_tx_power_qdbm;
    /** Current transmit power in qdBm */
    int8_t current_tx_power_qdbm;
    /** Scale values of MCS 0-10 */
    int8_t scale_value_db[11];
    /** Board's antenna gain in dBi */
    int8_t tx_antenna_gain_dbi;
    /** Regulatory domain's transmit power limit in dBm */
    int8_t regulatory_limit_dbm;
};

/** Structure to store response from chip
  * Changed int8_t to int16_t for
  * base_power_qdbm,
  * max_tx_power_qdbm,
  * current_tx_power_qdbm and
  * regulatory_limit_dbm
  */
struct PACKED tx_pwr_adj_cfm_v2
{
    /** Bit mask variable to specify enable/disable of tx power adjustment */
    uint32_t en_tx_pwr_adj_mask;
    /** Sub-band 1in8 scale values in qdB */
    int32_t sb_1in_8[4];
    /** Sub-band 2in8 scale values in qdB */
    int32_t sb_2in_8[2];
    /** Teperature based scaler value in linear scale */
    int32_t temp_power_scaler;
    /** Current sub-band scale value in linear scale */
    int32_t subband_scale;
    /** txscaler value in linear scale */
    int32_t tx_linear_power_scaler;
    /** Transmit power control */
    uint32_t tx_power_drift_lin;
    /** Chip's base power in qdBm */
    int16_t base_power_qdbm;
    /** Chip's max power in qdBm */
    int16_t max_tx_power_qdbm;
    /** Current transmit power in qdBm */
    int16_t current_tx_power_qdbm;
    /** Regulatory domain's transmit power limit in dBm */
    int16_t regulatory_limit_dbm;
    /** Scale values of MCS 0-10 */
    int8_t scale_value_db[11];
    /** Board's antenna gain in dBi */
    int8_t tx_antenna_gain_dbi;
};

static struct
{
    struct arg_int *mcs;
    struct arg_int *subband;
    struct arg_int *temp;
    struct arg_int *channel;
    struct arg_int *drift;
    struct arg_int *frequency;
} args;

int tx_pwr_adj_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args,
        "Get (default) or enable/disable TX power state adjustments",
        args.mcs = arg_rint0("m", NULL, "{0|1}", 0, 1,
            "Enable/disable MCS based adjustment"),
        args.subband = arg_rint0("s", NULL, "{0|1}", 0, 1,
            "Enable/disable subband based adjustment"),
        args.temp = arg_rint0("t", NULL, "{0|1}", 0, 1,
            "Enable/disable temperature based adjustment"),
        args.channel = arg_rint0("c", NULL, "{0|1}", 0, 1,
            "Enable/disable per channel power limit"),
        args.drift = arg_rint0("d", NULL, "{0|1}", 0, 1,
            "Enable/disable per power drift correction"),
        args.frequency = arg_rint0("f", NULL, "{0|1}", 0, 1,
            "Enable/disable power across frequency adjustment"));
    return 0;
}

static void process_power_state(struct tx_pwr_adj_cfm_v2* cfm)
{
    mctrl_print("TX power state information\n");
    mctrl_print("\tBase power: %.3f dBm\n\tMax power: %.3f dBm\n\tCurrent power: %.3f dBm\n",
            ((float) cfm->base_power_qdbm)/4.0,
            ((float) cfm->max_tx_power_qdbm)/4.0,
            ((float) cfm->current_tx_power_qdbm)/4.0);
    mctrl_print("\tRegulatory limit: %.3f dBm\n",
            ((float) cfm->regulatory_limit_dbm)/4.0);
    mctrl_print("\tSubband power adjustment: %.3f dB\n",
            (float) 10*log10((cfm->subband_scale/65536.0)));
    mctrl_print("\tTemperature power adjustment: %.3f dB\n",
            (float) 10*log10((cfm->temp_power_scaler/65536.0)));
    mctrl_print("\tArbitrary txscaler: %.3f dB\n",
            (float) 20*log10((cfm->tx_linear_power_scaler/65536.0)));
    mctrl_print("\tTx power drift: %.3f dB\n",
            (float) 10*log10((cfm->tx_power_drift_lin/65536.0)));
    mctrl_print("\tTX antenna gain: %d dBi\n",
            cfm->tx_antenna_gain_dbi);
    mctrl_print("\tTX power adjustment mask: %d\n",
            cfm->en_tx_pwr_adj_mask);
    mctrl_print("\tEnable MCS based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_MCS) && 1);
    mctrl_print("\tEnable sub-band based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_SUBBAND) && 1);
    mctrl_print("\tEnable temperature based adjustment: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_TEMPERATURE) && 1);
    mctrl_print("\tEnable per channel power limit: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_CHANNEL_PWR_LIM) && 1);
    mctrl_print("\tEnable power drift correction: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_TX_PWR_CTRL) && 1);
    mctrl_print("\tEnable power adjustment across frequency: %d\n",
            (cfm->en_tx_pwr_adj_mask & TX_PWR_ADJ_PWR_FREQ) && 1);
    for (int i = 0; i < 10 ; i++)
    {
        mctrl_print("\tMCS%d: %.3f dBm\n", i,
                ((float) (cfm->scale_value_db[i]+cfm->base_power_qdbm))/4.0);
    }
}

struct tx_pwr_adj_cfm_v2* convert_cfm_v1_to_cfm_v2(struct tx_pwr_adj_cfm_v2* cfm_v2,
                            struct tx_pwr_adj_cfm_v2* cfm_v1_data)
{
    struct tx_pwr_adj_cfm_v1* cfm_v1 = (struct tx_pwr_adj_cfm_v1*) cfm_v2;
    /** Copy from V1 structure format to V2 structure format */
    cfm_v1_data->base_power_qdbm = cfm_v1->base_power_qdbm;
    cfm_v1_data->max_tx_power_qdbm = cfm_v1->max_tx_power_qdbm;
    cfm_v1_data->current_tx_power_qdbm = cfm_v1->current_tx_power_qdbm;
    cfm_v1_data->regulatory_limit_dbm = cfm_v1->regulatory_limit_dbm;
    cfm_v1_data->subband_scale = cfm_v1->subband_scale;
    cfm_v1_data->temp_power_scaler = cfm_v1->temp_power_scaler;
    cfm_v1_data->tx_linear_power_scaler = cfm_v1->tx_linear_power_scaler;
    cfm_v1_data->tx_power_drift_lin = cfm_v1->tx_power_drift_lin;
    cfm_v1_data->tx_antenna_gain_dbi = cfm_v1->tx_antenna_gain_dbi;
    cfm_v1_data->en_tx_pwr_adj_mask = cfm_v1->en_tx_pwr_adj_mask;
    for (int i = 0; i < 10 ; i++)
    {
        cfm_v1_data->scale_value_db[i] = cfm_v1->scale_value_db[i];
    }
    return cfm_v1_data;
}

int tx_pwr_adj(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;
    uint8_t flag = 0;
    uint32_t en_tx_pwr_adj_mask = 0;
    struct tx_pwr_adj_command *cmd = {0};
    struct tx_pwr_adj_cfm_v2 *cfm = {0}, cfm_v1_data = {0};
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;
    int32_t tmp;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct tx_pwr_adj_command);
    cfm = TBUFF_TO_RSP(rsp_tbuff, struct tx_pwr_adj_cfm_v2);

    if (args.mcs->count == 0 &&
        args.subband->count == 0 &&
        args.channel->count == 0 &&
        args.temp->count == 0 &&
        args.drift->count == 0 &&
        args.frequency->count == 0)
    {
        /** Get the tx power adjustments */
        flag = 1;
        en_tx_pwr_adj_mask = 0;
    }
    else
    {
        /** Set the tx power adjustments */
        flag = 2;
        /** Read the current mask and update with user input */
        cmd->flag = htole32(1);
        cmd->en_tx_pwr_adj_mask = 0;
        ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TX_PWR_ADJ,
                                 cmd_tbuff, rsp_tbuff);
        if (ret < 0)
        {
            mctrl_err("Failed to execute command\n");
            goto exit;
        }
        en_tx_pwr_adj_mask = cfm->en_tx_pwr_adj_mask;

        if (args.mcs->count)
        {
            tmp = args.mcs->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_MCS))
                | (tmp & 0x01);
            mctrl_print("MCS based power adjustment: %d\n", tmp);
        }

        if (args.subband->count)
        {
            tmp = args.subband->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_SUBBAND))
                | ((tmp & 0x01) << 1);
            mctrl_print("Subband based power adjustment: %d\n", tmp);
        }

        if (args.temp->count)
        {
            tmp = args.temp->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_TEMPERATURE))
                | ((tmp & 0x01) << 2);
            mctrl_print("Temperature based power adjustment: %d\n", tmp);
        }

        if (args.channel->count)
        {
            tmp = args.channel->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_CHANNEL_PWR_LIM))
                | ((tmp & 0x01) << 3);
            mctrl_print("Channel based power adjustment: %d\n", tmp);
        }

        if (args.drift->count)
        {
            tmp = args.drift->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_TX_PWR_CTRL))
                | ((tmp & 0x01) << 4);
            mctrl_print("Power drift correction: %d\n", tmp);
        }

        if (args.frequency->count)
        {
            tmp = args.frequency->ival[0];
            en_tx_pwr_adj_mask = (en_tx_pwr_adj_mask & (~TX_PWR_ADJ_PWR_FREQ))
                | ((tmp & 0x01) << 5);
            mctrl_print("Power adjustment across frequency: %d\n", tmp);
        }
    }

    cmd->flag = htole32(flag);
    cmd->en_tx_pwr_adj_mask = htole32(en_tx_pwr_adj_mask);
    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_TX_PWR_ADJ,
                                 cmd_tbuff, rsp_tbuff);
    if (flag == 1)
    {
        if ((rsp_tbuff->data_len - sizeof(struct response)) < sizeof(struct tx_pwr_adj_cfm_v2))
        {
            /** If older struct is received convert to newer for backwards compatibility */
            cfm = convert_cfm_v1_to_cfm_v2(cfm, &cfm_v1_data);
        }
        process_power_state(cfm);
    }
exit:

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(tx_pwr_adj, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
