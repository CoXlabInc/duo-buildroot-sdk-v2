/*
 * Copyright 2023 Morse Micro
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "command.h"
#include "utilities.h"
#include "transport/transport.h"
#include "portable_endian.h"

/** Configure the bitmask for the commands with states defined by gpio_control_cmds_t */
#define GPIO_CTRL_CMD_MASK  (0b11)

/** Width of the pin_mask found in gpio_config */
#define PIN_MASK_MAX_WIDTH  (32)
#define PIN_MASK_MIN_WIDTH  (0)

#define GPIO_PIN_GLOSSARY "Pins to configure"
#define GPIO_PIN_REM1 "The <pin> parameter accepts two formats:"
#define GPIO_PIN_REM2 "Space-separated decimal numbers: 0 1 3 represents pins 0, 1 and 3"
#define GPIO_PIN_REM3 \
    "A single hexadecimal bitmask prefixed with '0x': 0x0B represents pins 0, 1 and 3"

struct PACKED gpio_config
{
    /** Command flags defined by gpio_control_flags_t */
    uint32_t flags;
    /** Pin flags specifying which GPIO to configure/fetch */
    uint32_t pin_mask;
};

typedef enum
{
    /**
     *  Configure the state for the pins in the mask. If this flag is not set,
     *  the pins are configured with a LOW state
     */
    GPIO_CTRL_FLAG_HIGH         = BIT(2),
    /**
     *  Configure the mode for the pins in the mask. If this flag is not set, the pins are
     *  configured as an INPUT
     */
    GPIO_CTRL_FLAG_OUTPUT       = BIT(3),
    /**
     *  Configure the drive speed for the pins in the mask. If this flag is not set, the pins are
     *  configured for a slow drive-speed
     */
    GPIO_CTRL_FLAG_SPEED        = BIT(4),
    /**
     *  Configure the pins in the mask as pull up resistors. If this flag is not set, the pins are
     *  configured for no pull up
     */
    GPIO_CTRL_FLAG_PULLUP       = BIT(5),
} gpio_ctrl_flag_t;

/* Enum for types of commands types */
typedef enum
{
    GPIO_CTRL_CMD_STATE      = 1U,
    GPIO_CTRL_CMD_MODE       = 2U,
    GPIO_CTRL_CMD_INFO       = 3U,
} gpio_control_cmds_t;

static struct {
    struct arg_rex *command;
} args;

static struct mm_argtable state;
static struct mm_argtable mode;
static struct mm_argtable info;

static struct mm_argtable *subcmds[] =
{
    &state, &mode, &info
};

static struct {
    struct arg_rex *high;
    struct arg_str *pins;
} state_args;

static struct {
    struct arg_rex *direction;
    struct arg_lit *speed;
    struct arg_lit *pullup;
    struct arg_str *pins;
} mode_args;

static struct {
    struct arg_lit *json;
    struct arg_str *pins;
} info_args;

int gpio_control_init(struct morsectrl *mors, struct mm_argtable *mm_args)
{
    MM_INIT_ARGTABLE(mm_args, "Get (default) or set the state of GPIO pins",
        args.command = arg_rex1(NULL, NULL, "(state|mode|info)",
            "{state|mode|info}", 0, "GPIO subcommand"));

    args.command->hdr.flag |= ARG_STOPPARSE;

    MM_INIT_ARGTABLE(&state, "Configure state for <pin>",
        state_args.high = arg_rex1(NULL, NULL, "(high|low)", "{high|low}", 0,
            "Drive <pin> high or low"),
        state_args.pins = arg_strn(NULL, NULL, "<pin>", 1, PIN_MASK_MAX_WIDTH,
            GPIO_PIN_GLOSSARY),
        arg_rem(NULL, GPIO_PIN_REM1),
        arg_rem(NULL, GPIO_PIN_REM2),
        arg_rem(NULL, GPIO_PIN_REM3));

    MM_INIT_ARGTABLE(&mode, "Configure mode for <pin>",
        mode_args.direction = arg_rex1(NULL, NULL, "(output|input)", "{output|input}", 0,
            "Set pin direction"),
        mode_args.speed = arg_lit0("s", NULL, "Set drive speed to fast (for output mode only)"),
        mode_args.pullup = arg_lit0("p", NULL,
            "Set pull-up resistor to enabled (for input mode only)"),
        mode_args.pins = arg_strn(NULL, NULL, "<pin>", 1, PIN_MASK_MAX_WIDTH,
            GPIO_PIN_GLOSSARY),
        arg_rem(NULL, GPIO_PIN_REM1),
        arg_rem(NULL, GPIO_PIN_REM2),
        arg_rem(NULL, GPIO_PIN_REM3));

    MM_INIT_ARGTABLE(&info, "Get the current configuration for <pin>",
        info_args.json = arg_lit0("j", NULL, "Print configuration of pins in JSON format"),
        info_args.pins = arg_strn(NULL, NULL, "<pin>", 1, PIN_MASK_MAX_WIDTH,
            GPIO_PIN_GLOSSARY),
        arg_rem(NULL, GPIO_PIN_REM1),
        arg_rem(NULL, GPIO_PIN_REM2),
        arg_rem(NULL, GPIO_PIN_REM3));
    return 0;
}

int gpio_control_help(void)
{
    mm_help_argtable("gpio_control state", &state);
    mm_help_argtable("gpio_control mode", &mode);
    mm_help_argtable("gpio_control info", &info);
    return 0;
}

/**
 * @brief Checks if the pin exceeds the bitsize of the pin_mask flag
 * @param pin number which represents the GPIO pin number
 * @return true if pin is within bitsize of gpio_config parameter pin_mask, false otherwise
 */
static inline bool is_valid_pin(int32_t pin)
{
    return (pin >= PIN_MASK_MIN_WIDTH && pin < PIN_MASK_MAX_WIDTH) ? true : false;
}

/**
 * @brief Checks if the output mode flag is set
 * @param flags bit flags from gpio_control_flags_t
 * @return true if output flag is set, false otherwise
 */
static inline bool is_mode_output(uint32_t flags)
{
    return !!(flags & GPIO_CTRL_FLAG_OUTPUT);
}

/**
 * @brief Prints a row with the sizing adjusted to the print_header function.
 *
 * @param pin gpio number
 * @param mode gpio direction
 * @param state gpio state
 * @param speed gpio drive speed
 * @param pullup gpio input pull-up resistor
 */
static void print_row(int32_t pin, char *mode, char *state, char *speed, char *pullup, bool json)
{
    if (json)
    {
        mctrl_print(
            "{\"Pin\":%d,\"Mode\":\"%s\",\"State\":\"%s\",\"Speed\":\"%s\",\"Pull-up\":\"%s\"}",
            pin, mode, state, speed, pullup);
    }
    else
    {
        mctrl_print("%-3d\t%-6s\t%-5s\t%-5s\t%-7s\n", pin, mode, state, speed, pullup);
    }
}

/**
 * @brief Prints a header for the with fields corresponding to GPIO configuration
 */
static void print_header(bool json)
{
    if (json)
    {
        mctrl_print("[");
    }
    else
    {
        mctrl_print("Pin\tMode\tState\tSpeed\tPull-up\n");
    }
}

/**
 * @brief Identifies the command type and sets the appropriate flag
 *
 * @param cmd flags to set
 *
 * @return 0 if success, otherwise negative return code
 */
static int get_cmd(uint32_t *cmd)
{
    uint32_t mode;

    if (strcmp(args.command->sval[0], "state") == 0)
    {
        mode = GPIO_CTRL_CMD_STATE;
    }
    else if (strcmp(args.command->sval[0], "mode") == 0)
    {
        mode = GPIO_CTRL_CMD_MODE;
    }
    else if (strcmp(args.command->sval[0], "info") == 0)
    {
        mode = GPIO_CTRL_CMD_INFO;
    }
    else
    {
        mctrl_err("Invalid subcommand - %s\n", args.command->sval[0]);
        return -ENXIO;
    }

    *cmd |= BMSET(mode, GPIO_CTRL_CMD_MASK);

    return 0;
}

/**
 * @brief Sets the pin mask based on the specified action
 *
 * @param argc argument counter
 * @param argv string list of arguments
 * @param pin_mask pin indices to set and send to the firmware
 * @param pin_offset different starting locations based on command type
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_pin_handler(struct arg_str *pins, uint32_t *pin_mask)
{
    uint64_t pin;
    const char **values = pins->sval;

    for (int i = 0; i < pins->count; i++)
    {
        if (str_to_uint64(values[i], &pin) < 0)
        {
            mctrl_err("Invalid argument - %s\n", values[i]);
            return -EINVAL;
        }

        /* Hexadecimal handler */
        if (strncasecmp(values[i], "0x", strlen("0x")) == 0)
        {
            /* Check that only one hex mask is provided and no decimal arguments are also present*/
            if ((i != (pins->count - 1)) || i != 0)
            {
                mctrl_err("Too many arguments\n");
                return -EINVAL;
            }

            if (pin > UINT32_MAX)
            {
                mctrl_err("Invalid hexadecimal string %s - must be between 0x01 and 0x%lX\n",
                            values[i], UINT32_MAX);
                return -EINVAL;
            }

            *pin_mask = pin;
            break;
        }

        /* Decimal handler */
        if (pin > (PIN_MASK_MAX_WIDTH - 1) || values[i][0] == '-')
        {
            mctrl_err("Pin position %s is invalid - must be between %d and %d\n", values[i],
                        PIN_MASK_MIN_WIDTH, (PIN_MASK_MAX_WIDTH - 1));
            return -ENODEV;
        }

        *pin_mask |= BIT(pin);
    }

    return 0;
}


/**
 * @brief Handler for the returned data for the INFO command
 *
 * @param req contains information about the command
 * @param cfm contains the return data
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_cfm_info(struct gpio_config *req, struct gpio_config *cfm, bool json)
{
    int32_t pin;

    /* Get first set bit */
    pin = ctz(req->pin_mask);

    /* If pin index exceeds the bitwidth of the command structure or index does not exist */
    if (pin < 0 || !is_valid_pin(pin))
    {
        return -ENXIO;
    }

    if (!cfm->pin_mask && !cfm->flags)
    {
        print_row(
            pin,
            "invalid",
            "-",
            "-",
            "-",
            json);
        return 0;
    }

    if (MORSE_IS_BIT_SET(cfm->pin_mask, pin))
    {
        if (is_mode_output(cfm->flags))
        {
            print_row(
                pin,
                "output",
                (cfm->flags & GPIO_CTRL_FLAG_HIGH) ? "high" : "low",
                (cfm->flags & GPIO_CTRL_FLAG_SPEED) ? "fast" : "slow",
                "-",
                json);
        }
        else
        {
            print_row(
                pin,
                "input",
                (cfm->flags & GPIO_CTRL_FLAG_HIGH) ? "high" : "low",
                "-",
                (cfm->flags & GPIO_CTRL_FLAG_PULLUP) ? "enabled" : "disabled",
                json);
        }
    }
    else if (MORSE_IS_BIT_SET(req->pin_mask, pin))
    {
        print_row(
            pin,
            is_mode_output(cfm->flags) ? "iof" : "none",
            "-",
            "-",
            "-",
            json);
    }

    return 0;
}

/**
 * @brief Handler for the returned data for the mode and state command
 *
 * Prints an error message indicating the failed to set pin.
 *
 * @param req contains information about the command
 * @param cfm contains the return data
 */
static void gpio_control_cfm_set(struct gpio_config *req, struct gpio_config *cfm)
{
    for (int i = PIN_MASK_MIN_WIDTH; i < PIN_MASK_MAX_WIDTH; i++)
    {
        if (!MORSE_IS_BIT_SET(cfm->pin_mask, i) && MORSE_IS_BIT_SET(req->pin_mask, i))
        {
            mctrl_err("Failed to set pin %d\n", i);
        }
    }
}

/**
 * @brief Command handler for the returned data
 *
 * @param req contains information about the command
 * @param results contains the return data
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_cfm_handler(struct gpio_config *req, struct gpio_config *results)
{
    int ret = 0;
    struct gpio_config cfm;

    cfm.flags = le32toh(results->flags);
    cfm.pin_mask = le32toh(results->pin_mask);

    switch (BMGET(req->flags, GPIO_CTRL_CMD_MASK))
    {
        case GPIO_CTRL_CMD_STATE:
            gpio_control_cfm_set(req, &cfm);
            break;
        case GPIO_CTRL_CMD_MODE:
            gpio_control_cfm_set(req, &cfm);
            break;
        default:
            ret = -ESRCH;
            break;
    }

    return ret;
}

/**
 * @brief Handler for the information command
 *
 *  Info command sends individual pins to the firmware due to the command structure. Error handling
 *  will be self-contained.
 *
 * @param argc argument counter
 * @param argv string list of arguments
 * @param pin_mask pin indices to set and send to the firmware
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_cmd_info(struct morsectrl *mors, int argc, char *argv[], uint32_t *flags,
    struct gpio_config *req, struct gpio_config *cfm,
    struct morsectrl_transport_buff *cmd, struct morsectrl_transport_buff *resp)
{
    uint32_t pin = 0;
    bool json = false;
    uint8_t pins;
    uint8_t count = 1;
    struct gpio_config results;

    int ret = mm_parse_argtable("gpio_control info", &info, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    json = info_args.json->count > 0 ? true : false;
    ret = gpio_pin_handler(info_args.pins, &pin);
    if (ret < 0)
    {
        return ret;
    }

    /* Track pin status to determine JSON/header prints */
    pins = popcount(pin);

    for (int i = PIN_MASK_MIN_WIDTH; i < PIN_MASK_MAX_WIDTH; i++)
    {
        if (MORSE_IS_BIT_SET(pin, i))
        {
            cfm->flags = 0;
            cfm->pin_mask = 0;
            req->pin_mask = htole32(BIT(i));
            req->flags = htole32(*flags);

            ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_GPIO,
                                        cmd, resp);
            /* ENXIO is an invalid pin - do not exit early */
            if (ret < 0 && ret != MORSE_RET_ENXIO)
            {
                return ret;
            }

            ret = 0;

            if (count == 1)
            {
                print_header(json);
            }

            results.flags = le32toh(cfm->flags);
            results.pin_mask = le32toh(cfm->pin_mask);

            ret = gpio_control_cfm_info(req, &results, json);
            if (ret < 0)
            {
                return ret;
            }

            if (json)
            {
                if (count < pins)
                {
                    mctrl_print(",");
                }
                else if (count == pins)
                {
                    mctrl_print("]");
                }
            }

            /*
             * Response transport buffer length is trimmed when using the FTDI over SPI transport
             * this causes failures for subsequent commmands which use the same transport as the
             * resp->data_len is smaller than the required size.
             */
            count++;
        }
    }

    return 0;
}

/**
 * @brief Handler for the mode command
 *
 * If the action is identified as set an additional argument will be parsed and the pin_offset for
 * the gpio_pin_handler() will be updated. Optional args may need to be parsed.
 *
 * @param argc argument counter
 * @param flags flags to set
 * @param pin_mask pin to set
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_cmd_mode(int argc, char *argv[], uint32_t *flags,
                                  uint32_t *pin_mask)
{
    int ret = mm_parse_argtable("gpio_control mode", &mode, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    if (strcmp(mode_args.direction->sval[0], "input") == 0)
    {
        *flags &= ~GPIO_CTRL_FLAG_OUTPUT;
    }
    else
    {
        *flags |= GPIO_CTRL_FLAG_OUTPUT;
    }

    if (mode_args.pullup->count)
    {
        *flags |= GPIO_CTRL_FLAG_PULLUP;
    }
    if (mode_args.speed->count)
    {
        *flags |= GPIO_CTRL_FLAG_SPEED;
    }
    ret = gpio_pin_handler(mode_args.pins, pin_mask);

    return ret;
}

/**
 * @brief Handler for the state command
 *
 * If the action is identified as set an additional argument will be parsed and the pin_offset for
 * the gpio_pin_handler() will be updated.
 *
 * @param argc argument counter
 * @param argv string list of arguments
 * @param flags flags to set
 * @param pin_mask pin to set
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_cmd_state(int argc, char *argv[], uint32_t *flags,
                                  uint32_t *pin_mask)
{
    int ret = mm_parse_argtable("gpio_control state", &state, argc, argv);
    if (ret != 0)
    {
        return ret;
    }

    if (strcmp(state_args.high->sval[0], "high") == 0)
    {
        *flags |= GPIO_CTRL_FLAG_HIGH;
    }
    else
    {
        *flags &= ~GPIO_CTRL_FLAG_HIGH;
    }
    ret = gpio_pin_handler(state_args.pins, pin_mask);
    return ret;
}

/**
 * @brief Command handler based on the command flag
 *
 * @param mors morsectrl object
 * @param argc argument counter
 * @param argv string of command arguments
 * @param req structure to store the command request
 *
 * @return 0 if success, otherwise negative return code
 */
static int gpio_control_command_handler(
    struct morsectrl *mors, int argc, char *argv[],
    struct gpio_config *req, struct gpio_config *cfm,
    struct morsectrl_transport_buff *cmd, struct morsectrl_transport_buff *resp)
{
    int ret = 0;
    uint32_t flags = 0;
    uint32_t pin_mask = 0;

    ret = get_cmd(&flags);
    if (ret < 0)
    {
        return ret;
    }

    switch (BMGET(flags, GPIO_CTRL_CMD_MASK))
    {
        case GPIO_CTRL_CMD_STATE:
            ret = gpio_control_cmd_state(argc, argv, &flags, &pin_mask);
            break;
        case GPIO_CTRL_CMD_MODE:
            ret = gpio_control_cmd_mode(argc, argv, &flags, &pin_mask);
            break;
        case GPIO_CTRL_CMD_INFO:
            ret = gpio_control_cmd_info(mors, argc, argv, &flags, req, cfm, cmd, resp);
            return ret;
        default:
            mctrl_err("Invalid command\n");
            ret = -ENXIO;
            break;
    }

    if (ret < 0)
    {
        return ret;
    }

    req->flags = htole32(flags);
    req->pin_mask = htole32(pin_mask);
    return ret;
}

/**
 * @brief Handler for gpio_control command
 *
 * @param mors morsectrl object
 * @param argc argument counter
 * @param argv command line string of args
 *
 * @return 0 if success, otherwise a negative return code
 */
int gpio_control(struct morsectrl *mors, int argc, char *argv[])
{
    int ret = 0;
    int i;
    struct morsectrl_transport_buff *cmd_tbuff = NULL;
    struct morsectrl_transport_buff *rsp_tbuff = NULL;
    struct gpio_config *req;
    struct gpio_config *cfm;

    cmd_tbuff = morsectrl_transport_cmd_alloc(mors->transport, sizeof(*req));
    rsp_tbuff = morsectrl_transport_resp_alloc(mors->transport, sizeof(*cfm));
    if (!cmd_tbuff || !rsp_tbuff)
    {
        ret = -ENOMEM;
        goto exit;
    }

    req = TBUFF_TO_CMD(cmd_tbuff, struct gpio_config);
    cfm = TBUFF_TO_RSP(rsp_tbuff, struct gpio_config);

    ret = gpio_control_command_handler(mors, argc, argv, req, cfm, cmd_tbuff, rsp_tbuff);
    if (ret != 0)
    {
        goto exit;
    }

    /* Info command is sent in gpio_control_command_handler */
    if (BMGET(req->flags, GPIO_CTRL_CMD_MASK) == GPIO_CTRL_CMD_INFO)
    {
        goto exit;
    }

    ret = morsectrl_send_command(mors->transport, MORSE_TEST_COMMAND_GPIO,
                                 cmd_tbuff, rsp_tbuff);
    if (ret < 0)
    {
        /* Print errors generated by the firmware */
        if (ret == MORSE_RET_EPERM)
        {
            mctrl_err("Unable to set all specified pins. Not all pins were in output mode\n");
        }
        else if (ret == MORSE_RET_ENXIO)
        {
            mctrl_err("Invalid pin\n");
        }
        else if (ret == MORSE_RET_EINVAL)
        {
            mctrl_err("Invalid command arguments\n");
        }
        goto exit;
    }

    ret = gpio_control_cfm_handler(req, cfm);

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

MM_CLI_HANDLER_CUSTOM_HELP(gpio_control, MM_INTF_REQUIRED, MM_DIRECT_CHIP_SUPPORTED);
