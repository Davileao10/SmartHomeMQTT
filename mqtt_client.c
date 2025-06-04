/* AULA IoT - Ricardo Prates - 001 - Cliente MQTT - Publisher:/Temperatura; Subscribed:/led
 * Adaptado por Davi Leao para controle de LEDs RGB (pinos 11, 12, 13), dois buzzers (pinos 10, 21) com melodia de Parabéns
 * e joystick (eixo X em GPIO 27, eixo Y em GPIO 26) como sensor de presença
 * Material de suporte - 04/06/2025
 * Código adaptado de: https://github.com/raspberrypi/pico-examples/tree/master/pico_w/wifi/mqtt
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

// Definições
#define WIFI_SSID "ABRAAO-10-2G"
#define WIFI_PASSWORD "rosana5478"
#define MQTT_SERVER "192.168.0.11"
#define MQTT_USERNAME "davi"
#define MQTT_PASSWORD "davi"
#define TEMPERATURE_UNITS 'C'
#define MQTT_TOPIC_LEN 100

// Pinos
#define LED_R 13  // GP13, PWM6B
#define LED_G 11  // GP11, PWM5B
#define LED_B 12  // GP12, PWM6A
#define BUZZER 10 // GP10, PWM5A
#define BUZZER2 21 // GP21, PWM2B
#define JOYSTICK_X_PIN 27 // GP27, ADC1
#define JOYSTICK_Y_PIN 26 // GP26, ADC0

// Configurações PWM
#define PWM_MAX_VALUE 65535
#define PWM_DEFAULT_FREQ 1000 // Frequência padrão para LEDs (1 kHz)
#define BUZZER_DUTY_CYCLE 75  // Duty cycle ajustado para o volume

// Configurações do Joystick
#define JOYSTICK_THRESHOLD 3000 // Limiar para detectar movimento na diagonal direita
#define JOYSTICK_CENTER 2048    // Valor central do ADC (12 bits, 0-4095)

// Frequências e durações para "Parabéns" (durações ajustadas para ser um pouco mais rápida)
const int notes[] = {
    261, 261, 293, 261, 349, 330,
    261, 261, 293, 261, 392, 349,
    261, 261, 523, 440, 349, 330,
    261, 261, 523, 440, 392, 349
};
const int durations[] = {
    360, 360, 720, 720, 720, 720,  // Reduzido de 400/800 para 360/720 (10% mais rápido)
    360, 360, 720, 720, 720, 720,
    360, 360, 720, 720, 720, 720,
    360, 360, 720, 720, 720, 720
};
const int melody_length = sizeof(notes) / sizeof(notes[0]);

// Variáveis globais
static bool light_state = false;
static bool music_state = false;
static bool presence_state = false; // Estado de presença (inicialmente sem presença)
static uint slice_num_red, slice_num_green, slice_num_blue;
static uint channel_red, channel_green, channel_blue;
static uint slice_num_buzzer, channel_buzzer;
static uint slice_num_buzzer2, channel_buzzer2;
static int note_index = 0;
static absolute_time_t next_note_time;

typedef struct {
    mqtt_client_t* mqtt_client;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_addr;
    bool mqtt_connected;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

#define INFO_printf printf
#define ERROR_printf printf
#define WARN_printf printf

#define TEMP_WORKER_INTERVAL_S 10
#define MQTT_KEEP_ALIVE_S 60
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0 // Desativar retenção de mensagens
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MESSAGE "0"
#define MQTT_WILL_QOS 1
#define MQTT_DEVICE_NAME "pico"

// Prototipos das funções
static void init_rgb_pwm(void);
static void init_buzzer_pwm(void);
static void init_joystick(void);
static void check_presence(MQTT_CLIENT_DATA_T *state);
static void update_music_state(bool on);
static void play_music(void);
static void update_leds(MQTT_CLIENT_DATA_T *state);
static float read_onboard_temperature(void);
static void pub_request_cb(void *arg, err_t err);
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);
static void control_led(MQTT_CLIENT_DATA_T *state, bool on);
static void publish_temperature(MQTT_CLIENT_DATA_T *state);
static void sub_request_cb(void *arg, err_t err);
static void unsub_request_cb(void *arg, err_t err);
static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void start_client(MQTT_CLIENT_DATA_T *state);
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

// Implementações das funções
static void init_rgb_pwm(void) {
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);

    slice_num_red = pwm_gpio_to_slice_num(LED_R);
    slice_num_green = pwm_gpio_to_slice_num(LED_G);
    slice_num_blue = pwm_gpio_to_slice_num(LED_B);

    channel_red = pwm_gpio_to_channel(LED_R);
    channel_green = pwm_gpio_to_channel(LED_G);
    channel_blue = pwm_gpio_to_channel(LED_B);

    INFO_printf("PWM Config: Red Slice=%u, Channel=%u\n", slice_num_red, channel_red);
    INFO_printf("PWM Config: Green Slice=%u, Channel=%u\n", slice_num_green, channel_green);
    INFO_printf("PWM Config: Blue Slice=%u, Channel=%u\n", slice_num_blue, channel_blue);

    // Configurar divisor de clock para todos os slices
    uint32_t clock = 125000000; // 125 MHz
    uint32_t divider = clock / (PWM_DEFAULT_FREQ * PWM_MAX_VALUE);
    if (divider < 1) divider = 1;
    if (divider > 255) divider = 255;

    pwm_set_clkdiv(slice_num_red, divider);
    pwm_set_clkdiv(slice_num_green, divider);
    pwm_set_clkdiv(slice_num_blue, divider);

    pwm_set_wrap(slice_num_red, PWM_MAX_VALUE);
    pwm_set_wrap(slice_num_green, PWM_MAX_VALUE);
    pwm_set_wrap(slice_num_blue, PWM_MAX_VALUE);

    pwm_set_chan_level(slice_num_red, channel_red, 0);
    pwm_set_chan_level(slice_num_green, channel_green, 0);
    pwm_set_chan_level(slice_num_blue, channel_blue, 0);

    pwm_set_enabled(slice_num_red, true);
    pwm_set_enabled(slice_num_green, true);
    pwm_set_enabled(slice_num_blue, true);

    // Reforçar estado inicial do LED verde para garantir que o slice 5 esteja ativo
    pwm_set_chan_level(slice_num_green, channel_green, 0);
}

static void init_buzzer_pwm(void) {
    // Configurar o primeiro buzzer (GPIO 10)
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    slice_num_buzzer = pwm_gpio_to_slice_num(BUZZER);
    channel_buzzer = pwm_gpio_to_channel(BUZZER);
    pwm_set_wrap(slice_num_buzzer, PWM_MAX_VALUE);
    pwm_set_chan_level(slice_num_buzzer, channel_buzzer, 0);

    // Configurar o segundo buzzer (GPIO 21)
    gpio_set_function(BUZZER2, GPIO_FUNC_PWM);
    slice_num_buzzer2 = pwm_gpio_to_slice_num(BUZZER2);
    channel_buzzer2 = pwm_gpio_to_channel(BUZZER2);
    pwm_set_wrap(slice_num_buzzer2, PWM_MAX_VALUE);
    pwm_set_chan_level(slice_num_buzzer2, channel_buzzer2, 0);

    INFO_printf("PWM Config: Buzzer1 Slice=%u, Channel=%u\n", slice_num_buzzer, channel_buzzer);
    INFO_printf("PWM Config: Buzzer2 Slice=%u, Channel=%u\n", slice_num_buzzer2, channel_buzzer2);
}

static void init_joystick(void) {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN); // Inicializa GPIO 27 (ADC1)
    adc_gpio_init(JOYSTICK_Y_PIN); // Inicializa GPIO 26 (ADC0)
    INFO_printf("Joystick inicializado: X=GPIO%d (ADC1), Y=GPIO%d (ADC0)\n", JOYSTICK_X_PIN, JOYSTICK_Y_PIN);
}

static void check_presence(MQTT_CLIENT_DATA_T *state) {
    // Selecionar e ler o eixo X (ADC1)
    adc_select_input(1); // ADC1 é GPIO 27
    uint16_t x_value = adc_read();

    // Selecionar e ler o eixo Y (ADC0)
    adc_select_input(0); // ADC0 é GPIO 26
    uint16_t y_value = adc_read();

    // Verificar se o joystick foi movido para a diagonal direita
    bool new_presence = (x_value > JOYSTICK_THRESHOLD && y_value > JOYSTICK_THRESHOLD);

    // Publicar apenas se o estado de presença mudou
    if (new_presence != presence_state) {
        presence_state = new_presence;
        const char* message = presence_state ? "1" : "0";
        INFO_printf("Presença: %s\n", message);
        mqtt_publish(state->mqtt_client, full_topic(state, "/presenca"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

static void update_music_state(bool on) {
    if (music_state == on) {
        INFO_printf("Música já está %s\n", on ? "ligada" : "desligada");
        return;
    }

    music_state = on;
    if (!on) {
        pwm_set_chan_level(slice_num_buzzer, channel_buzzer, 0);
        pwm_set_chan_level(slice_num_buzzer2, channel_buzzer2, 0);
        // Não desativar o slice 5 para preservar o LED verde (PWM5B)
        if (slice_num_buzzer != slice_num_green) {
            pwm_set_enabled(slice_num_buzzer, false);
        }
        pwm_set_enabled(slice_num_buzzer2, false);
        note_index = 0;
        INFO_printf("Música desligada\n");
    } else {
        note_index = 0;
        next_note_time = get_absolute_time();
        INFO_printf("Música ligada\n");
    }
}

static void play_music(void) {
    if (!music_state) return;

    if (absolute_time_diff_us(get_absolute_time(), next_note_time) <= 0) {
        int freq = notes[note_index];
        int duration = durations[note_index];

        // Configurar os buzzers
        if (freq == 0) {
            pwm_set_chan_level(slice_num_buzzer, channel_buzzer, 0);
            pwm_set_chan_level(slice_num_buzzer2, channel_buzzer2, 0);
            if (slice_num_buzzer != slice_num_green) {
                pwm_set_enabled(slice_num_buzzer, false);
            }
            pwm_set_enabled(slice_num_buzzer2, false);
        } else {
            uint32_t clock = 125000000; // 125 MHz
            uint32_t wrap = clock / freq;
            if (wrap > PWM_MAX_VALUE) wrap = PWM_MAX_VALUE;
            if (wrap < 100) wrap = 100; // Evitar valores muito pequenos
            uint32_t divider = (clock / (freq * wrap));
            if (divider < 1) divider = 1;
            if (divider > 255) divider = 255;

            pwm_set_clkdiv(slice_num_buzzer, divider);
            pwm_set_wrap(slice_num_buzzer, wrap);
            pwm_set_chan_level(slice_num_buzzer, channel_buzzer, (wrap * BUZZER_DUTY_CYCLE) / 100);
            pwm_set_enabled(slice_num_buzzer, true);

            pwm_set_clkdiv(slice_num_buzzer2, divider);
            pwm_set_wrap(slice_num_buzzer2, wrap);
            pwm_set_chan_level(slice_num_buzzer2, channel_buzzer2, (wrap * BUZZER_DUTY_CYCLE) / 100);
            pwm_set_enabled(slice_num_buzzer2, true);

            INFO_printf("Tocando nota %d Hz, wrap=%u, divider=%u, duty=%d%%, duração %d ms, índice %d\n", freq, wrap, divider, BUZZER_DUTY_CYCLE, duration, note_index);
        }

        next_note_time = make_timeout_time_ms(duration);
        note_index = (note_index + 1) % melody_length; // Loop contínuo
    }
}

static void update_leds(MQTT_CLIENT_DATA_T *state) {
    if (light_state) {
        pwm_set_chan_level(slice_num_red, channel_red, PWM_MAX_VALUE);   // Vermelho
        pwm_set_chan_level(slice_num_green, channel_green, PWM_MAX_VALUE); // Verde
        pwm_set_chan_level(slice_num_blue, channel_blue, PWM_MAX_VALUE);  // Azul
        INFO_printf("LEDs ligados: R=%u, G=%u, B=%u\n", PWM_MAX_VALUE, PWM_MAX_VALUE, PWM_MAX_VALUE);
        const char* message = "LIGADO";
        mqtt_publish(state->mqtt_client, full_topic(state, "/luz/estado"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        INFO_printf("LEDs atualizados: %s (branco)\n", message);
    } else {
        pwm_set_chan_level(slice_num_red, channel_red, 0);
        pwm_set_chan_level(slice_num_green, channel_green, 0);
        pwm_set_chan_level(slice_num_blue, channel_blue, 0);
        INFO_printf("LEDs desligados: R=0, G=0, B=0\n");
        const char* message = "DESLIGADO";
        mqtt_publish(state->mqtt_client, full_topic(state, "/luz/estado"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        INFO_printf("LEDs atualizados: %s\n", message);
    }
}

static float read_onboard_temperature(void) {
    // Garantir que o canal correto (ADC4) seja selecionado para o sensor de temperatura
    adc_select_input(4); // ADC4 é o canal do sensor de temperatura onboard
    const float conversion_factor = 3.3f / (1 << 12); // 12 bits, 0-4095 para 0-3.3V
    float adc = (float)adc_read() * conversion_factor;
    float temperature = 27.0f - ((adc - 0.706f) / 0.011f); // Fórmula ajustada com coeficiente 0.011

    // Verificação para evitar valores inválidos
    if (temperature < -10.0f || temperature > 100.0f) {
        INFO_printf("Temperatura: 0.0\n");
        return 0.0f; // Valor padrão em caso de erro
    }

    INFO_printf("Temperatura: %.2f\n", temperature);
    return temperature;
}

static void pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) ERROR_printf("Publish failed: %d\n", err);
}

static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
    static char topic[MQTT_TOPIC_LEN];
    snprintf(topic, sizeof(topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return topic;
}

static void control_led(MQTT_CLIENT_DATA_T *state, bool on) {
    const char* message = on ? "Ligado" : "Desligado";
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on ? 1 : 0);
    mqtt_publish(state->mqtt_client, full_topic(state, "/led/estado"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

static void publish_temperature(MQTT_CLIENT_DATA_T *state) {
    static float last_temp = -1;
    float temp = read_onboard_temperature();
    if (temp != last_temp) {
        last_temp = temp;
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temp);
        INFO_printf("Publicando temperatura: %s\n", temp_str);
        mqtt_publish(state->mqtt_client, "/temperatura", temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != ERR_OK) panic("Subscrição falhou: %d", err);
    state->subscribe_count++;
    INFO_printf("Subscrito, total=%d\n", state->subscribe_count);
}

static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != ERR_OK) panic("Dessubscrição falhou: %d", err);
    state->subscribe_count--;
    if (state->subscribe_count <= 0 && state->stop_client) mqtt_disconnect(state->mqtt_client);
}

static void sub_unsub_topics(MQTT_CLIENT_DATA_T *state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client, full_topic(state, "/led/estado"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_set_inpub_callback(state->mqtt_client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    mqtt_sub_unsub(state->mqtt_client, full_topic(state, "/luz/ligar"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client, full_topic(state, "/musica/ligar"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    char* topic = state->topic;
    strncpy(state->data, (const char*)data, len);
    state->data[len] = '\0';
    INFO_printf("Recebido: Tópico=%s, Payload=%s\n", topic, state->data);

    if (strcmp(topic, full_topic(state, "/led/estado")) == 0) {
        INFO_printf("Processando /led/estado\n");
        if (strcmp(state->data, "1") == 0 || strcasecmp(state->data, "Ligado") == 0) control_led(state, true);
        else if (strcmp(state->data, "0") == 0 || strcasecmp(state->data, "Desligado") == 0) control_led(state, false);
    } else if (strcmp(topic, full_topic(state, "/luz/ligar")) == 0) {
        INFO_printf("Processando /luz/ligar\n");
        if (strcmp(state->data, "1") == 0 || strcasecmp(state->data, "LIGADO") == 0) {
            light_state = true;
            update_leds(state);
        } else if (strcmp(state->data, "0") == 0 || strcasecmp(state->data, "DESLIGADO") == 0) {
            light_state = false;
            update_leds(state);
        }
    } else if (strcmp(topic, full_topic(state, "/musica/ligar")) == 0) {
        INFO_printf("Processando /musica/ligar\n");
        if (strcmp(state->data, "1") == 0 || strcasecmp(state->data, "LIGADO") == 0) {
            update_music_state(true);
        } else if (strcmp(state->data, "0") == 0 || strcasecmp(state->data, "DESLIGADO") == 0) {
            update_music_state(false);
        }
    } else {
        INFO_printf("Tópico desconhecido: %s\n", topic);
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_temperature(state);
    async_context_add_at_time_worker_in_ms(context, worker, TEMP_WORKER_INTERVAL_S * 1000);
}
static async_at_time_worker_t temperature_worker = { .do_work = temperature_worker_fn };

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->mqtt_connected = true;
        INFO_printf("Conexão MQTT aceita\n");
        sub_unsub_topics(state, true);
        mqtt_publish(state->mqtt_client, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        mqtt_publish(state->mqtt_client, full_topic(state, "/musica/ligar"), "0", 1, MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
        temperature_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &temperature_worker, 0);
    } else {
        panic("Conexão MQTT falhou: %d", status);
    }
}

static void start_client(MQTT_CLIENT_DATA_T *state) {
    state->mqtt_client = mqtt_client_new();
    if (!state->mqtt_client) panic("Unable to create mqtt client");
    INFO_printf("Conectando ao servidor MQTT: %s\n", ipaddr_ntoa(&state->mqtt_server_addr));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client, &state->mqtt_server_addr, MQTT_PORT, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("Unable to connect to mqtt broker");
    }
    mqtt_set_inpub_callback(state->mqtt_client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_addr = *ipaddr;
        start_client(state);
    } else {
        panic("Unable to resolve hostname: %s\n", hostname);
    }
}

int main(void) {
    stdio_init_all();
    INFO_printf("Starting MQTT client: %s\n", __TIME__);

    // Inicializar ADC para temperatura e joystick
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(JOYSTICK_X_PIN); // Inicializa GPIO 27 (ADC1)
    adc_gpio_init(JOYSTICK_Y_PIN); // Inicializa GPIO 26 (ADC0)

    init_rgb_pwm();
    init_buzzer_pwm();
    init_joystick();

    static MQTT_CLIENT_DATA_T mqtt_state;
    if (cyw43_arch_init()) {
        panic("Unable to init wifi");
    }

    char unique_id[5];
    pico_get_unique_board_id_string(unique_id, sizeof(unique_id));
    for (size_t i = 0; i < sizeof(unique_id)-1; i++) {
        unique_id[i] = tolower(unique_id[i]);
    }
    char client_id[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id)];
    strcpy(client_id, MQTT_DEVICE_NAME);
    strcat(client_id, unique_id);
    INFO_printf("Client ID: %s\n", client_id);

    mqtt_state.mqtt_client_info.client_id = client_id;
    mqtt_state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
    mqtt_state.mqtt_client_info.client_user = MQTT_USERNAME;
    mqtt_state.mqtt_client_info.client_pass = MQTT_PASSWORD;
    mqtt_state.mqtt_client_info.will_topic = full_topic(&mqtt_state, MQTT_WILL_TOPIC);
    mqtt_state.mqtt_client_info.will_msg = MQTT_WILL_MESSAGE;
    mqtt_state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    mqtt_state.mqtt_client_info.will_retain = true;

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Unable to connect to wifi");
    }
    INFO_printf("Connected to wifi\n");

    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &mqtt_state.mqtt_server_addr, dns_found, &mqtt_state);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) {
        start_client(&mqtt_state);
    } else if (err != ERR_INPROGRESS) {
        panic("Unable to resolve DNS: %d\n", err);
    }

    while (!mqtt_state.mqtt_connected || mqtt_client_is_connected(mqtt_state.mqtt_client)) {
        if (music_state) {
            play_music();
        }
        check_presence(&mqtt_state); // Verificar presença com o joystick
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }

    INFO_printf("Done\n");
    return 0;
}