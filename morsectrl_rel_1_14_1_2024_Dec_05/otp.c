/*
 * Copyright 2022 Morse Micro
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "command.h"
#include "utilities.h"

struct PACKED command_otp_req
{
    /** Bool, 1=enabled, 0=disabled */
    uint8_t write_otp;
    uint8_t bank_num;
    uint32_t bank_val;
};

struct PACKED command_otp_cfm
{
    uint32_t bank_val;
};

static struct
{
    struct arg_int *bank_num;
#if !defined(MORSE_CLIENT)
    struct arg_int *write_val;
#endif
} args;

int otp_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
#define OTP_ARGTABLE_DESC "Read OTP bank"
#define OTP_BANK_NUM_DESC "Bank number to read from"
#if !defined(MORSE_CLIENT)
#undef OTP_ARGTABLE_DESC
#undef OTP_BANK_NUM_DESC
#define OTP_ARGTABLE_DESC "Read/write OTP bank"
#define OTP_BANK_NUM_DESC "Bank number to read/write from"
#endif

    MM_INIT_ARGTABLE(mm_args, OTP_ARGTABLE_DESC,
        args.bank_num = arg_int1(NULL, NULL, "<bank num>", OTP_BANK_NUM_DESC)
#if !defined(MORSE_CLIENT)
        , args.write_val = arg_int0("w", NULL, "<value>", "Burn the provided value to the OTP bank")
#endif
    ); /* NOLINT (whitespace/parens) */
    return 0;
}

int otp(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = -1;

    struct command_otp_req *cmd;
    struct command_otp_cfm *resp;
    struct morsectrl_transport_buff *cmd_tbuff;
    struct morsectrl_transport_buff *rsp_tbuff;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*cmd));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*resp));

    if (!cmd_tbuff || !rsp_tbuff)
        goto exit;

    cmd = TBUFF_TO_CMD(cmd_tbuff, struct command_otp_req);
    resp = TBUFF_TO_RSP(rsp_tbuff, struct command_otp_cfm);
    cmd->write_otp = 0;

#if !defined(MORSE_CLIENT)
    if (args.write_val->count)
    {
        cmd->write_otp = 1;
        cmd->bank_val = htole32(args.write_val->ival[0]);
    }
#endif

    cmd->bank_num = args.bank_num->ival[0];

    ret = morsectrl_send_command(mors->transport, MORSE_COMMAND_OTP,
                                 cmd_tbuff, rsp_tbuff);

exit:
    if (ret == 0 && !cmd->write_otp)
        mctrl_print("OTP Bank %d: 0x%x\n", args.bank_num->ival[0], resp->bank_val);

    morsectrl_transport_buff_free(cmd_tbuff);
    morsectrl_transport_buff_free(rsp_tbuff);
    return ret;
}

MM_CLI_HANDLER(otp, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
