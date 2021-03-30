/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 "Andreas Valder" <andreas.valder@serioese.gmbh>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"
#include "modesp32.h"
#include "mphalport.h"

#include "esp32_servo.h"

typedef struct _esp32_servo_obj_t {
    mp_obj_base_t base;
    gpio_num_t pin;
    uint8_t active;
    uint8_t channel;
} esp32_servo_obj_t;

STATIC int chan_gpio[MCPWM_CHANNEL_MAX];

// Config of timer upon which we run all servo GPIO pins
STATIC bool servo_inited = false;

STATIC mcpwm_config_t servo_cfg = {
    .frequency = MCPWM_FREQ,
    .cmpr_a = MCPWM_DUTY_CYCLE,
    .cmpr_b = MCPWM_DUTY_CYCLE,
    .counter_mode = MCPWM_COUNTER_MODE,
    .duty_mode = MCPWM_DUTY_MODE
};

// we have to have init code
STATIC void servo_init(void) {

    // Initial condition: no channels assigned
    for (int x = 0; x < MCPWM_CHANNEL_MAX; ++x) {
        chan_gpio[x] = -1;
    }

    // Init with default timer params
    mcpwm_init(MCPWM_UNIT, MCPWM_TIMER_0, &servo_cfg);
    mcpwm_init(MCPWM_UNIT, MCPWM_TIMER_1, &servo_cfg);
    mcpwm_init(MCPWM_UNIT, MCPWM_TIMER_2, &servo_cfg);
}

// MicroPython bindings for PWM

STATIC void esp32_servo_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    esp32_servo_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Servo(%u", self->pin);
    if (self->active) {
        mcpwm_timer_t timer = self->channel / 2;
        mcpwm_operator_t oper = self->channel % 2;
        float duty_f;
        duty_f = mcpwm_get_duty(MCPWM_UNIT, timer, oper);
        mp_printf(print, ", duty=%u", (int)(duty_f / 100 * 20000));
    }
    mp_printf(print, ")");
}

STATIC void esp32_servo_init_helper(esp32_servo_obj_t *self,
    size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_duty };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_duty, MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    int channel;
    int avail = -1;

    // Find a free Servo channel, also spot if our pin is
    //  already mentioned.
    for (channel = 0; channel < MCPWM_CHANNEL_MAX; ++channel) {
        if (chan_gpio[channel] == self->pin) {
            break;
        }
        if ((avail == -1) && (chan_gpio[channel] == -1)) {
            avail = channel;
        }
    }
    if (channel >= MCPWM_CHANNEL_MAX) {
        if (avail == -1) {
            mp_raise_ValueError(MP_ERROR_TEXT("out of Servo channels"));
        }
        channel = avail;
    }
    self->channel = channel;

    mcpwm_timer_t timer = channel / 2;
    mcpwm_operator_t oper = channel % 2;

    // New Servo assignment
    self->active = 1;
    if (chan_gpio[channel] == -1) {
        if (mcpwm_gpio_init(MCPWM_UNIT, channel, self->pin) != ESP_OK) {
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("Servo not supported on pin %d"), self->pin);
        }
        // set duty to 0
        if (mcpwm_set_duty_in_us(MCPWM_UNIT, timer, oper, 0) != ESP_OK) {
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("Servo duty arg error chan %d unit %d timer %d oper %d"),
                channel, MCPWM_UNIT, timer, oper);
        }
        //  set the duty mode
        if (mcpwm_set_duty_type(MCPWM_UNIT, timer, oper, MCPWM_DUTY_MODE) != ESP_OK) {
            mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("Servo duty type arg error chan %d unit %d timer %d oper %d"),
                channel, MCPWM_UNIT, timer, oper);
        }
        chan_gpio[channel] = self->pin;
    }

    // Set duty cycle?
    int dval = args[ARG_duty].u_int;
    if (dval != -1) {
        if (mcpwm_set_duty_in_us(MCPWM_UNIT, timer, oper, (uint32_t)dval) != ESP_OK) {
            mp_raise_msg_varg(&mp_type_ValueError,
                MP_ERROR_TEXT("Servo duty arg error chan %d unit %d timer %d oper %d"),
                channel, MCPWM_UNIT, timer, oper);
        }
    }
}

STATIC mp_obj_t esp32_servo_make_new(const mp_obj_type_t *type,
    size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, MP_OBJ_FUN_ARGS_MAX, true);
    gpio_num_t pin_id = machine_pin_get_id(args[0]);

    // create Servo object from the given pin
    esp32_servo_obj_t *self = m_new_obj(esp32_servo_obj_t);
    self->base.type = &esp32_servo_type;
    self->pin = pin_id;
    self->active = 0;
    self->channel = -1;

    // start the Servo subsystem if it's not already running
    if (!servo_inited) {
        servo_init();
        servo_inited = true;
    }

    // start the Servo running for this channel
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    esp32_servo_init_helper(self, n_args - 1, args + 1, &kw_args);

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t esp32_servo_init(size_t n_args,
    const mp_obj_t *args, mp_map_t *kw_args) {
    esp32_servo_init_helper(args[0], n_args - 1, args + 1, kw_args);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(esp32_servo_init_obj, 1, esp32_servo_init);

STATIC mp_obj_t esp32_servo_deinit(mp_obj_t self_in) {
    esp32_servo_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int chan = self->channel;

    // Valid channel?
    if ((chan >= 0) && (chan < MCPWM_CHANNEL_MAX)) {
        // Mark it unused, and tell the hardware to stop routing
        chan_gpio[chan] = -1;
        mcpwm_timer_t timer = chan / 2;
        mcpwm_operator_t oper = chan % 2;
        mcpwm_set_signal_low(MCPWM_UNIT, timer, oper);
        self->active = 0;
        self->channel = -1;
        gpio_matrix_out(self->pin, SIG_GPIO_OUT_IDX, false, false);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp32_servo_deinit_obj, esp32_servo_deinit);

STATIC mp_obj_t esp32_servo_duty(size_t n_args, const mp_obj_t *args) {
    esp32_servo_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int duty;

    mcpwm_timer_t timer = self->channel / 2;
    mcpwm_operator_t oper = self->channel % 2;

    if (n_args == 1) {
        // get
        float duty_f;
        duty_f = mcpwm_get_duty(MCPWM_UNIT, timer, oper);
        duty = (int)(duty_f / 100 * 20000);
        return MP_OBJ_NEW_SMALL_INT(duty);
    }

    // set
    duty = mp_obj_get_int(args[1]);
    mcpwm_set_duty_in_us(MCPWM_UNIT, timer, oper, duty);

    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp32_servo_duty_obj, 1, 2, esp32_servo_duty);

STATIC const mp_rom_map_elem_t esp32_servo_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&esp32_servo_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&esp32_servo_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_duty), MP_ROM_PTR(&esp32_servo_duty_obj) },
};

STATIC MP_DEFINE_CONST_DICT(esp32_servo_locals_dict,
    esp32_servo_locals_dict_table);

const mp_obj_type_t esp32_servo_type = {
    { &mp_type_type },
    .name = MP_QSTR_SERVO,
    .print = esp32_servo_print,
    .make_new = esp32_servo_make_new,
    .locals_dict = (mp_obj_dict_t *)&esp32_servo_locals_dict,
};

