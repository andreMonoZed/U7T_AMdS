#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "ssd1306.h"

#define BUZZER_PIN 21
#define BUTTON_A 5
#define BUTTON_B 6

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

volatile bool music = false;
volatile int count = 0;
volatile bool stop_program = false;
volatile bool timer_active = false;

alarm_id_t current_timer_id;

struct render_area frame_area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1
};

uint8_t ssd[ssd1306_buffer_length];

const uint rhapsody_notes[] = {450, 450, 450, 450, 410, 415, 405, 410, 420, 405, 410};
const uint note_duration[] = {125, 125, 125, 125, 250, 250, 250, 125, 250, 125, 250};

void setup_button() {
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
}

void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0);
}

void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2);
    sleep_ms(duration_ms);
    pwm_set_gpio_level(pin, 0);
    sleep_ms(50);
}

void play_rhapsody(uint pin) {
    for (int i = 0; i < sizeof(rhapsody_notes) / sizeof(rhapsody_notes[0]); i++) {
        if (rhapsody_notes[i] == 0) {
            sleep_ms(note_duration[i]);
        } else {
            play_tone(pin, rhapsody_notes[i], note_duration[i]);
        }
    }
}

void clear_oled() {
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
}

int64_t timer_callback(alarm_id_t id, void *user_data) {
    if (!timer_active) return 0;

    char *text = "Tomar remedio X";
    if (count == 2) text = "Tomar remedio Y";
    else if (count == 3) text = "Tomar remedio Z";
    
    timer_active = false;

    clear_oled();
    ssd1306_draw_string(ssd, 0, 0, text);
    ssd1306_draw_string(ssd, 0, 16, "------------");
    ssd1306_draw_string(ssd, 0, 32, "Aperte o botao A");
    render_on_display(ssd, &frame_area);
    
    music = true;
    return 0;
}

void gpio_callback(uint gpio, uint32_t events) {
    static uint32_t last_interrupt_time = 0;
    uint32_t time = to_ms_since_boot(get_absolute_time());

    if ((time - last_interrupt_time) > 200) {
        last_interrupt_time = time;

        if (gpio == BUTTON_A) {
            if(count == 3){
                stop_program = true;
            }
            else if (timer_active) {
                cancel_alarm(current_timer_id);
                timer_active = false;
                music = false;
                clear_oled();
                ssd1306_draw_string(ssd, 0, 0, "Timer cancelado");
                ssd1306_draw_string(ssd, 0, 16, "------------");
                ssd1306_draw_string(ssd, 0, 32, "Aperte o botao B");
                render_on_display(ssd, &frame_area);
            } 
             else {
                music = false;
                clear_oled();
                ssd1306_draw_string(ssd, 0, 0, "Aperte o botao B");
                ssd1306_draw_string(ssd, 0, 16, "------------");
                ssd1306_draw_string(ssd, 0, 32, "Para o proximo");
                render_on_display(ssd, &frame_area);
            }
        } 
        
        else if (gpio == BUTTON_B) {

            if (!timer_active) {
                int time = 10000;
                if (count == 1) time = 5000;
                else if (count == 2) time = 8000;
                count++;
                clear_oled();
                ssd1306_draw_string(ssd, 0, 0, "Timer iniciado");
                render_on_display(ssd, &frame_area);

                timer_active = true;
                current_timer_id = add_alarm_in_ms(time, timer_callback, NULL, false);
            }
        }
    }
}

int main() {
    stdio_init_all();
    setup_button();
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
    calculate_render_area_buffer_length(&frame_area);

    clear_oled();
    ssd1306_draw_string(ssd, 0, 0, "Aperte o botao B");
    ssd1306_draw_string(ssd, 0, 16, "Para iniciar");
    render_on_display(ssd, &frame_area);

    pwm_init_buzzer(BUZZER_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);

    while (!stop_program) {
        if (music) {
            play_rhapsody(BUZZER_PIN);
        }
        sleep_ms(10);
    }

    clear_oled();
    ssd1306_draw_string(ssd, 0, 0, "Timer Encerrado");
    render_on_display(ssd, &frame_area);

    return 0;
}

