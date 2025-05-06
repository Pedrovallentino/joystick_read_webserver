#include "pico/stdlib.h"               // Biblioteca padrão da Raspberry Pi Pico
#include "hardware/adc.h"              // Controle do ADC (Conversor Analógico-Digital)
#include "pico/cyw43_arch.h"           // Biblioteca para controle do Wi-Fi integrado
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"                 // Manipulação de pacotes de buffer da pilha TCP/IP
#include "lwip/tcp.h"                  // Funções do protocolo TCP
#include "lwip/netif.h"                // Interface de rede

#define WIFI_SSID "ANDREA"        // Nome da rede Wi-Fi
#define WIFI_PASSWORD "03051998pedro"    // Senha da rede Wi-Fi

#define ADC_JOYSTICK_X 0               // Canal ADC para o eixo X do joystick
#define ADC_JOYSTICK_Y 1               // Canal ADC para o eixo Y do joystick

// Converte valor ADC (0–4095) para porcentagem (0–100)
int adc_to_percent(uint16_t raw) {
    return (raw * 100) / 4095;
}

// Retorna o nome da direção com base no ângulo
const char* direcao_nome(int angulo) {
    switch (angulo) {
        case 0: return "Norte";
        case 45: return "Nordeste";
        case 90: return "Leste";
        case 135: return "Sudeste";
        case 180: return "Sul";
        case 225: return "Sudoeste";
        case 270: return "Oeste";
        case 315: return "Noroeste";
        default: return "Centro";
    }
}

// Determina o ângulo baseado nas porcentagens de X e Y
int calcular_angulo(int x, int y) {
    if (x > 70 && y > 70) return 45;
    if (x < 30 && y > 70) return 315;
    if (x > 70 && y < 30) return 135;
    if (x < 30 && y < 30) return 225;
    if (x > 70) return 90;
    if (x < 30) return 270;
    if (y > 70) return 0;
    if (y < 30) return 180;
    return -1; // Centro
}

// Função de callback chamada ao receber dados TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Copia e imprime a requisição recebida
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';
    printf("Request: %s\n", request);

    // Leitura dos valores do joystick (X e Y)
    adc_select_input(ADC_JOYSTICK_X);
    uint16_t raw_x = adc_read();
    adc_select_input(ADC_JOYSTICK_Y);
    uint16_t raw_y = adc_read();

    // Conversão para porcentagem e cálculo da direção
    int pos_x = adc_to_percent(raw_x);
    int pos_y = adc_to_percent(raw_y);
    int angulo = calcular_angulo(pos_x, pos_y);
    const char* direcao = direcao_nome(angulo);

    // Geração de página HTML com agulha da bússola
    char html[4096];
    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html lang=\"pt-BR\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<title>Joystick Read</title>\n"
        "<style>\n"
        "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
        "h1 { font-size: 48px; margin-bottom: 20px; }\n"
        "p { font-size: 28px; }\n"
        ".compass {\n"
        "  position: relative;\n"
        "  width: 200px;\n"
        "  height: 200px;\n"
        "  border: 8px solid #333;\n"
        "  border-radius: 50%%;\n"
        "  margin: 40px auto;\n"
        "  background: white;\n"
        "}\n"
        ".needle {\n"
        "  position: absolute;\n"
        "  top: 50%%;\n"
        "  left: 50%%;\n"
        "  width: 4px;\n"
        "  height: 90px;\n"
        "  background: red;\n"
        "  transform: translate(-50%%, -100%%) rotate(%ddeg);\n"
        "  transform-origin: 50%% 100%%;\n"
        "  transition: transform 0.3s ease-in-out;\n"
        "}\n"
        ".direction-label {\n"
        "  font-weight: bold;\n"
        "  font-size: 32px;\n"
        "  margin: 10px;\n"
        "}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<h1>Joystick</h1>\n"
        "<p class=\"direction-label\">Você está apontando para: <strong>%s</strong></p>\n"
        "<p>Position X: %d %%</p>\n"
        "<p>Position Y: %d %%</p>\n"
        "<div class=\"compass\">\n"
        "  <div class=\"needle\"></div>\n"
        "</div>\n"
        "</body>\n"
        "</html>\n",
        angulo == -1 ? 0 : angulo, direcao, pos_x, pos_y);

    // Envia a resposta HTML ao cliente
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Libera recursos
    free(request);
    pbuf_free(p);
    return ERR_OK;
}

// Função chamada ao aceitar uma nova conexão TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);  // Define a função de recebimento
    return ERR_OK;
}

int main() {
    stdio_init_all();  // Inicializa entrada/saída padrão

    // Inicializa e conecta ao Wi-Fi
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");
    if (netif_default) {
        printf("IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Cria e configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);               // Escuta por conexões
    tcp_accept(server, tcp_server_accept);     // Define função de aceitação
    printf("Servidor ouvindo na porta 80\n");

    adc_init();  // Inicializa o ADC para ler joystick

    // Loop principal de verificação da pilha de rede
    while (true) {
        cyw43_arch_poll();
    }

    cyw43_arch_deinit(); // (Nunca alcançado, mas encerra Wi-Fi)
    return 0;
}
