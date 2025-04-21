/*
 * Copyright 2020 Morse Micro
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "command.h"
#include "gpioctrl.h"
#include "utilities.h"

static struct
{
    struct arg_rex *enable;
    struct arg_int *gpio;
} args;

int jtag_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Control JTAG reset through RPi GPIO pin",
    args.enable = arg_rex1(NULL, NULL, MM_ARGTABLE_ENABLE_REGEX, MM_ARGTABLE_ENABLE_DATATYPE, 0,
        "Enable/disable the GPIO"),
    args.gpio = arg_int0(NULL, NULL, "<gpio>", "GPIO to control"));
    return 0;
}

int morsectrl_jtag(int enable, int jtag_gpio)
{
    int ret;
    const char *direction = enable ? "out" : "in";

    ret = gpio_export(jtag_gpio);
    if (ret)
    {
        goto exit;
    }

    ret = gpio_set_dir(jtag_gpio, direction);
    if (ret)
    {
        goto exit;
    }

    if (enable)
    {
        ret = gpio_set_val(jtag_gpio, 1);
        if (ret)
        {
             goto exit;
        }
        sleep_ms(5);
    }

exit:
    return ret;
}

int jtag(struct morsectrl *mors, int argc, char *argv[])
{
    int jtag_gpio, enable;

    if (args.gpio->count)
    {
        jtag_gpio = args.gpio->ival[0];
    }
    else
    {
        jtag_gpio = gpio_get_env(JTAG_GPIO);
        if (jtag_gpio == -1)
        {
            mctrl_err("Couldn't identify GPIO\n"
                      "Try entering GPIO manually or export %s to your env var\n", JTAG_GPIO);
            return -1;
        }
    }

    enable = expression_to_int(args.enable->sval[0]);

    return morsectrl_jtag(enable, jtag_gpio);
}

MM_CLI_HANDLER(jtag, MM_INTF_NOT_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
