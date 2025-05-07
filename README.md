## 📌 Visão Geral
Este projeto implementa um servidor web em uma Raspberry Pi Pico W que lê as posições de um joystick analógico utilizando os canais ADC (Conversor Analógico-Digital) e exibe a direção correspondente em uma página HTML acessível via Wi-Fi.

## 🛠️ Requisitos de Hardware
-**Microcontrolador: Raspberry Pi Pico W**

-**Joystick Analógico: Com saídas para os eixos X e Y**

**Conexões:**
-**ADC0 (GPIO26): Eixo X do joystick**

-**ADC1 (GPIO27): Eixo Y do joystick**

-**Alimentação: Via cabo micro-USB**

## 🖥️ Interface no navegador
<img src=https://github.com/user-attachments/assets/095b3ed6-7661-4c1f-8e16-41192fd53171>

## 📂 Estrutura das pastas
<img src=https://github.com/user-attachments/assets/ea17f308-decd-461b-a38c-92aa219f74e7>

-**CmakeLists.txt:** Arquivo para a build do projeto.

-**joystick_read_wifi.c:** Arquivo com o código para leitura do joystick e página web.

-**lwipopts.h:** Biblioteca necessária para a utilização do módulo wifi da placa.

-**pico_sdk_import.cmake:** Arquivo necessário para Importar corretamente o SDK da Pico (pico-sdk) para o CMake, permitindo que o projeto use as bibliotecas e drivers fornecidos pelo SDK.

## 🧾Estrutura do Código

**1. Inclusão de Bibliotecas**

```c
#include "pico/stdlib.h"               // Biblioteca padrão da Raspberry Pi Pico
#include "hardware/adc.h"              // Controle do ADC (Conversor Analógico-Digital)
#include "pico/cyw43_arch.h"           // Biblioteca para controle do Wi-Fi integrado
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"                 // Manipulação de pacotes de buffer da pilha TCP/IP
#include "lwip/tcp.h"                  // Funções do protocolo TCP
#include "lwip/netif.h"                // Interface de rede
```
-**pico/stdlib.h:** Funções padrão da Raspberry Pi Pico.

-**hardware/adc.h:** Controle do ADC para leitura analógica.

-**pico/cyw43_arch.h:** Controle do módulo Wi-Fi integrado.

-**lwip:** Pilha TCP/IP leve para comunicação de rede.

**2. Definições de Constantes**
```c
#define WIFI_SSID "SUAREDEWIFI"        // Nome da rede Wi-Fi
#define WIFI_PASSWORD "SENHADAREDE"    // Senha da rede Wi-Fi
#define ADC_JOYSTICK_X 0               // Canal ADC para o eixo X do joystick
#define ADC_JOYSTICK_Y 1               // Canal ADC para o eixo Y do joystick
```
-**WIFI_SSID e WIFI_PASSWORD:** Credenciais da rede Wi-Fi.

-**ADC_JOYSTICK_X e ADC_JOYSTICK_Y:** Canais ADC conectados aos eixos X e Y do joystick.

**3. Funções Auxiliares:**
   
**a. Conversão de Valor ADC para Porcentagem**
```c
// Converte valor ADC (0–4095) para porcentagem (0–100)
int adc_to_percent(uint16_t raw) {
    return (raw * 100) / 4095;
}
```
Converte o valor bruto do ADC (0 a 4095) para uma porcentagem (0% a 100%).

**b. Mapeamento de Ângulo para Direção**
```c
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
```
Converte um ângulo inteiro (retornado pela função calcular_angulo) no nome correspondente da direção cardinal ou intermediária.

**Funcionamento:**

- A função utiliza um switch para mapear os ângulos em strings fixas

**Impacto:**

- Torna a interface amigável, exibindo a direção como texto.
- É usada na geração da página HTML para indicar ao usuário para onde o joystick está apontando.

**c. Cálculo do Ângulo com Base nas Porcentagens de X e Y**
```c
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
```
Determina o ângulo baseado nas porcentagens de X e Y.

A função verifica em qual região da "rosa dos ventos" o joystick está apontando, com base em limites predefinidos:

**impacto:**
- Facilita a tradução dos valores analógicos do joystick em direções visuais e textuais.


- Retorna -1 quando o joystick está no centro (sem movimento).


**4. Função de Callback para Recebimento de Dados TCP**
```c
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
```
1. Verificação do pacote recebido (p):


    - Se p for NULL, significa que a conexão foi encerrada.


    - Fecha a conexão com tcp_close(tpcb) e desabilita o recebimento (tcp_recv(tpcb, NULL)).

2. Leitura da requisição HTTP:


    - Os dados recebidos (geralmente uma requisição GET de navegador) são copiados para uma string request.


    - A requisição é impressa no terminal com printf.

3. Leitura dos valores do joystick:


    - O ADC é configurado para ler primeiro o eixo X, depois o eixo Y.


    - Os valores analógicos brutos são armazenados em raw_x e raw_y.

4. Conversão e interpretação:


    - Os valores são convertidos para porcentagens (0% a 100%) usando a função adc_to_percent.


    - Com essas porcentagens, calcula-se o ângulo de direção com calcular_angulo.


    - O ângulo é convertido em uma string representando a direção (ex: "Norte", "Sul") com direcao_nome.

5. Geração da resposta HTML:


    - Um buffer HTML é montado com snprintf, contendo:


    - Título e informações de posição.


    - Um círculo (bússola) e uma agulha (ponteiro) girada com rotate(%ddeg) de acordo com o ângulo.


    - O nome da direção em destaque (Norte, Nordeste, etc.).


    - Os valores de X e Y em porcentagem

6. Envio da resposta:


    - O HTML gerado é enviado ao navegador com tcp_write e tcp_output.


7. Liberação de memória:

    - A string request e o buffer p são liberados com free e pbuf_free.

**5. Função de Callback para Aceitação de Conexões TCP**

```c
// Função chamada ao aceitar uma nova conexão TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);  // Define a função de recebimento
    return ERR_OK;
}

```
tcp_server_accept é chamada automaticamente sempre que uma nova conexão TCP é aceita no servidor.
Linha principal:
tcp_recv(newpcb, tcp_server_recv);

Função: Define que, para essa nova conexão (newpcb), a função tcp_server_recv será usada para tratar os dados recebidos.


Resultado: Toda vez que o cliente enviar dados (ou fizer uma requisição HTTP, por exemplo), tcp_server_recv será chamada.


Impacto: Sem essa configuração, o servidor aceitaria conexões mas não saberia como responder, tornando a comunicação incompleta.
Retorno:
return ERR_OK;

Indica que a aceitação da conexão foi bem-sucedida.
