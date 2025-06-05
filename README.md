# SmartHomeMQTT - Sistema IoT para Automação Residencial

Bem-vindo ao repositório **SmartHomeMQTT**!
Este projeto foi desenvolvido como parte da disciplina de *Comunicação em IoT* e tem como objetivo criar um sistema de automação residencial baseado em **MQTT**, com funcionalidades de monitoramento e controle remoto.

## 🔧 Funcionalidades

O sistema foi implementado utilizando a placa **BitDogLab** (baseada no Raspberry Pi Pico W) e permite:

* **📈 Monitoramento de Temperatura**
  Lê a temperatura ambiente (°C) a cada 10 segundos e publica no tópico `/temperatura`.

* **🕹️ Detecção de Presença**
  Utiliza um joystick como sensor de presença. Detecta movimento na diagonal direita e publica `1` (presente) ou `0` (ausente) no tópico `/picoe661/presenca`.

* **💡 Controle de Lâmpada**
  Um LED RGB simula uma lâmpada. Controlado via `/picoe661/luz/ligar` com valores `"1"/"LIGADO"` ou `"0"/"DESLIGADO"`. O LED acende branco quando ligado.

* **🎵 Reprodução de Música**
  Dois buzzers simulam uma caixa de som, tocando "Parabéns pra Você". Controlado via `/picoe661/musica/ligar`.

## 🧩 Arquitetura do Sistema

* **Placa:** BitDogLab (Raspberry Pi Pico W)
* **Broker MQTT:** Mosquitto rodando em um Android com Termux
* **Cliente Secundário:** Computador com painel MQTT (ex: IoT MQTT Panel)

### 🛠️ Periféricos Utilizados

* LED RGB (GPIO 11, 12, 13)
* Buzzers (GPIO 10, 21)
* Joystick (GPIO 26 e 27 - ADC0 e ADC1)
* Sensor de temperatura onboard (ADC4)

---

## 📦 Requisitos

### Hardware

* Raspberry Pi Pico W (BitDogLab)
* Android com Termux + Mosquitto
* PC com cliente MQTT (opcional)
* LED RGB, buzzers, joystick

### Software

* Pico SDK (v2.1.1+)
* CMake + Ninja
* Mosquitto
* IoT MQTT Panel (opcional)
* IDE como VS Code

---

## ⚙️ Instalação

### 1. Clone o repositório

```bash
git clone https://github.com/Davileao10/SmartHomeMQTT.git
cd SmartHomeMQTT
```

### 2. Configure o SDK

Configure o **Pico SDK** e defina a variável `PICO_SDK_PATH` no seu ambiente.

### 3. Configure o Broker MQTT no Android

No Termux:

```bash
pkg install mosquitto
mosquitto -d
```

Descubra o IP do celular e atualize o `#define MQTT_SERVER` no `mqtt_client.c`.

### 4. Configure o Wi-Fi

No arquivo `mqtt_client.c`, atualize:

```c
#define WIFI_SSID "SEU_SSID"
#define WIFI_PASSWORD "SUA_SENHA"
```

### 5. Compile e grave no Pico W

```bash
mkdir build
cd build
cmake ..
ninja
```

Copie o arquivo `mqtt_client.uf2` para o Raspberry Pi Pico W (modo BOOTSEL).

---

## 🚀 Uso

### Inicie o Broker

No Termux:

```bash
mosquitto -d
```

### Conecte o Pico W

Ao ligar, o Pico se conecta automaticamente ao Wi-Fi e ao broker. Use o Serial Monitor (baud 115200) para ver logs como:

```
Temperatura: 36.69
Presença: 1
```

### Controle via MQTT

Use um painel ou script para:

* `/picoe661/luz/ligar`: `"1"` / `"LIGADO"` ou `"0"` / `"DESLIGADO"`
* `/picoe661/musica/ligar`: `"1"` / `"LIGADO"` ou `"0"` / `"DESLIGADO"`

Assine os tópicos:

* `/temperatura`
* `/picoe661/presenca`

---

## 📁 Estrutura do Código

* `mqtt_client.c`: Código principal (MQTT, sensores e atuadores)
* `CMakeLists.txt`: Script de build
* Outros arquivos de build padrão do Pico SDK

---
