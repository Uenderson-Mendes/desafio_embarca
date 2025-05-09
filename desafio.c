#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// === CONFIGURAÇÕES ===
#define WIFI_SSID "nome wifi"
#define WIFI_PASS "senha wifi"
#define SERVER_HOST "ip do servidor"
#define SERVER_PORT 8000
#define API_ENDPOINT "/dados"

#define BUTTON1_PIN 5
#define BUTTON2_PIN 6
#define JOYSTICK_X 27
#define JOYSTICK_Y 26

// === VARIÁVEIS GLOBAIS ===
char button1_message[20] = "solto";
char button2_message[20] = "solto";
char temperature_message[30] = "Temperatura: N/A";
char joystick_direction[20] = "Centro";
int joy_x = 0, joy_y = 0;

// === STRUCT TCP ===
typedef struct {
    struct tcp_pcb *pcb;
    char *data;
    u16_t len;
    bool connected;
} tcp_connection_t;

// === FUNÇÕES DE SENSOR ===
const char* map_joystick_to_direction(int x, int y) {
    const int low = 1500, high = 2500;
    if (x < low && y > high) return "Noroeste";
    if (x > high && y > high) return "Nordeste";
    if (x < low && y < low) return "Sudoeste";
    if (x > high && y < low) return "Sudeste";
    if (x < low) return "Oeste";
    if (x > high) return "Leste";
    if (y > high) return "Norte";
    if (y < low) return "Sul";
    return "Centro";
}

float read_temperature_celsius() {
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / 4095.0f;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void monitor_sensors() {
    bool b1 = !gpio_get(BUTTON1_PIN);
    bool b2 = !gpio_get(BUTTON2_PIN);
    snprintf(button1_message, sizeof(button1_message), b1 ? "pressionado" : "solto");
    snprintf(button2_message, sizeof(button2_message), b2 ? "pressionado" : "solto");

    adc_select_input(1); joy_x = adc_read();
    adc_select_input(0); joy_y = adc_read();

    snprintf(joystick_direction, sizeof(joystick_direction), "%s", map_joystick_to_direction(joy_x, joy_y));

    float temp = read_temperature_celsius();
    if (!isnan(temp)) {
        snprintf(temperature_message, sizeof(temperature_message), "%.2f", temp);
    } else {
        snprintf(temperature_message, sizeof(temperature_message), "Erro de leitura");
    }
}

// === CALLBACKS TCP ===
static err_t tcp_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(pcb);
        tcp_connection_t *conn = (tcp_connection_t *)arg;
        mem_free(conn->data);
        mem_free(conn);
        return ERR_OK;
    }
    printf("Resposta: %.*s\n", p->len, (char*)p->payload);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_sent_cb(void *arg, struct tcp_pcb *pcb, u16_t len) {
    printf("Dados enviados com sucesso (%d bytes).\n", len);
    return ERR_OK;
}

void tcp_error_cb(void *arg, err_t err) {
    printf("Erro TCP: %d\n", err);
}

static err_t tcp_connected_cb(void *arg, struct tcp_pcb *pcb, err_t err) {
    tcp_connection_t *conn = (tcp_connection_t *)arg;
    if (err != ERR_OK) {
        printf("Erro na conexão: %d\n", err);
        tcp_abort(pcb);
        mem_free(conn->data);
        mem_free(conn);
        return err;
    }

    conn->connected = true;
    tcp_recv(pcb, tcp_recv_cb);
    tcp_sent(pcb, tcp_sent_cb);

    err_t wr_err = tcp_write(pcb, conn->data, conn->len, TCP_WRITE_FLAG_COPY);
    if (wr_err != ERR_OK) {
        printf("Erro ao enviar dados: %d\n", wr_err);
        tcp_abort(pcb);
        mem_free(conn->data);
        mem_free(conn);
        return wr_err;
    }

    tcp_output(pcb);
    return ERR_OK;
}

// === ENVIO JSON ===
void send_sensor_data() {
    char json[512];
    snprintf(json, sizeof(json),
        "{"
        "\"botao1\":\"%s\","
        "\"botao2\":\"%s\","
        "\"temperatura\":%s,"
        "\"joystick\":{"
        "\"x\":%d,"
        "\"y\":%d,"
        "\"direcao\":\"%s\""
        "}"
        "}",
        button1_message, button2_message, temperature_message,
        joy_x, joy_y, joystick_direction);

    char request[1024];
    int req_len = snprintf(request, sizeof(request),
    "POST %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n\r\n%s",
    API_ENDPOINT, SERVER_HOST, (int)strlen(json), json);

    ip_addr_t ip;
    err_t err = dns_gethostbyname(SERVER_HOST, &ip, NULL, NULL);
    if (err != ERR_OK) {
        printf("Erro ao resolver DNS: %d\n", err);
        return;
    }

    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;

    tcp_connection_t *conn = (tcp_connection_t *)mem_malloc(sizeof(tcp_connection_t));
    if (!conn) {
        tcp_abort(pcb);
        return;
    }

    conn->data = (char *)mem_malloc(req_len);
    memcpy(conn->data, request, req_len);
    conn->len = req_len;
    conn->pcb = pcb;
    conn->connected = false;

    tcp_arg(pcb, conn);
    tcp_err(pcb, tcp_error_cb);

    err = tcp_connect(pcb, &ip, SERVER_PORT, tcp_connected_cb);
    if (err != ERR_OK) {
        printf("Erro ao conectar: %d\n", err);
        mem_free(conn->data);
        mem_free(conn);
        tcp_abort(pcb);
    }
}

// === MAIN ===
int main() {
    stdio_init_all();
    sleep_ms(3000);

    if (cyw43_arch_init()) return 1;
    cyw43_arch_enable_sta_mode();

    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        printf("Tentando conectar ao Wi-Fi...\n");
        sleep_ms(5000);
    }

    printf("Conectado ao Wi-Fi. IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    printf("Botao1: %s, Botao2: %s, Temperatura: %s, Direcao: %s\n", 
        button1_message, button2_message, temperature_message, joystick_direction);

    gpio_init(BUTTON1_PIN); gpio_set_dir(BUTTON1_PIN, GPIO_IN); gpio_pull_up(BUTTON1_PIN);
    gpio_init(BUTTON2_PIN); gpio_set_dir(BUTTON2_PIN, GPIO_IN); gpio_pull_up(BUTTON2_PIN);
    adc_init(); adc_set_temp_sensor_enabled(true);
    adc_gpio_init(JOYSTICK_X); adc_gpio_init(JOYSTICK_Y);

    absolute_time_t last_post = get_absolute_time();

    while (true) {
        cyw43_arch_poll();
        monitor_sensors();

        if (absolute_time_diff_us(last_post, get_absolute_time()) >= 3 * 1000000) {
            send_sensor_data();
            last_post = get_absolute_time();
        }

        sleep_ms(1000);
    }

    return 0;
}
