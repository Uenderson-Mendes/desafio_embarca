
# Desafio Embarcados 🚀

Este repositório contém o código desenvolvido para o **Desafio Embarcados**, utilizando a plataforma Raspberry Pi Pico W e a linguagem C/C++. O projeto tem como objetivo realizar a leitura de sensores e o envio de dados via HTTPS para um servidor remoto.

## 📦 Estrutura do Projeto

```

desafio\_embarca/
├── CMakeLists.txt         # Arquivo de configuração do CMake
├── include/               # Arquivos de cabeçalho (.h)
├── lib/                   # Bibliotecas externas utilizadas no projeto
├── src/                   # Código-fonte principal (.c)
├── build/                 # Diretório gerado após compilação
└── README.md              # Documentação do projeto

````

## 🔧 Tecnologias Utilizadas

- Raspberry Pi Pico W
- C/C++
- CMake
- Biblioteca `pico_https_client` para requisições HTTPS
- Interface de desenvolvimento: VS Code + Pico SDK

## 🚀 Funcionalidades

- Leitura de sensores conectados ao Pico W
- Coleta de dados de botões e joystick
- Envio de dados via POST para uma API remota (`/dados`)
- Comunicação via protocolo HTTPS

## 📡 Requisitos

- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Toolchain para ARM](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
- Biblioteca `pico_https_client`
- CMake

## ⚙️ Como Compilar

1. Clone o repositório:
   ```
   git clone https://github.com/Uenderson-Mendes/desafio_embarca.git
   cd desafio_embarca


2. Configure o ambiente:

   * Instale o Pico SDK e exporte as variáveis necessárias.
   * Instale o `cmake` e a toolchain ARM.

3. Compile o projeto:

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

4. Carregue o firmware no Pico W (via UF2 ou USB).

## 📬 API de Destino

O projeto faz requisições POST para a seguinte URL:


https://api-node-dash-bitdoglab.onrender.com/dados


Os dados enviados incluem:

* Temperatura
* Estado dos botões
* Direção do joystick



## 📂 Explicação do Arquivo `desafio.c`

O arquivo [`desafio.c`](./desafio.c) é o coração do projeto e contém o seguinte funcionamento:

### 🔹 Bibliotecas Importadas

```
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "pico_https_client.h"
```

* Manipulam GPIOs, ADC (temperatura), Wi-Fi e requisições HTTPS.

### 🔹 Wi-Fi

```c
#define WIFI_SSID "BitDogLab"
#define WIFI_PASSWORD "12345678"
```

* Define o SSID e senha da rede à qual o Pico W se conecta.

### 🔹 Inicialização

* Inicializa a comunicação serial e a arquitetura do Wi-Fi.
* Conecta à rede definida.

### 🔹 Configuração de GPIOs e ADC

```c
gpio_init(5); gpio_set_dir(5, GPIO_IN); // e assim por diante...
adc_init(); adc_set_temp_sensor_enabled(true);
adc_select_input(4);
```

* Pinos usados para botões e joystick (GPIO 5, 6, 26, 27, 28).
* Leitura de temperatura via ADC interno.

### 🔹 Loop Principal

Dentro do `while (true)`:

1. **Leitura de temperatura**:

   ```c
   float temperature = 27 - (voltage - 0.706) / 0.001721;
   ```

   Usa a fórmula oficial do datasheet para converter a voltagem em graus Celsius.

2. **Leitura dos GPIOs**:

   ```c
   bool button_a = gpio_get(5); // Botões e joystick
   ```

3. **Construção do JSON**:

   ```c
   snprintf(json_body, sizeof(json_body), ...);
   ```

   Formata os dados em JSON para enviar via HTTPS.

4. **Envio dos dados**:

   ```c
   send_post_request("https://api-node-dash-bitdoglab.onrender.com/dados", json_body);
   ```

5. **Delay**:

   ```c
   sleep_ms(10000);
   ```

   Aguarda 10 segundos antes de repetir o processo.

---


