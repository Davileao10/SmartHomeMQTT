SmartHomeMQTT - Sistema IoT para Automação Residencial
  
Bem-vindo ao repositório SmartHomeMQTT! Este projeto foi desenvolvido como parte da disciplina de Comunicação em IoT, com o objetivo de criar um sistema IoT para automação residencial utilizando o protocolo MQTT. O sistema permite o monitoramento de temperatura e presença, além do controle remoto de luzes (simuladas por um LED RGB) e música (simulada por buzzers), tudo integrado via MQTT.
Descrição do Projeto
O SmartHomeMQTT é um sistema IoT voltado para casas inteligentes, implementado na placa BitDogLab (baseada no Raspberry Pi Pico W). Ele utiliza o protocolo MQTT para comunicação entre dispositivos, permitindo o controle e monitoramento remoto. As principais funcionalidades incluem:

Monitoramento de Temperatura: Lê a temperatura ambiente (em °C) a cada 10 segundos e publica no tópico /temperatura.
Detecção de Presença: Um joystick é adaptado como sensor de presença, detectando movimentos na diagonal direita (eixos X e Y acima de 3000). O estado (1 para presente, 0 para ausente) é publicado no tópico /picoe661/presenca.
Controle de Lâmpada: Um LED RGB (pinos 11, 12, 13) simula uma lâmpada residencial, controlado via MQTT pelo tópico /picoe661/luz/ligar (valores "1"/"LIGADO" ou "0"/"DESLIGADO"). O LED brilha em branco quando ligado.
Reprodução de Música: Dois buzzers (pinos 10 e 21) simulam uma caixa de som, tocando a melodia "Parabéns pra Você". A música é controlada via MQTT pelo tópico /pico/musica/ligar (valores "1"/"LIGADO" ou "0"/"DESLIGADO").

Estrutura do Sistema
O sistema é composto por:

Placa BitDogLab (Raspberry Pi Pico W): Cliente MQTT que executa o código principal.
Broker MQTT (Mosquitto): Rodando em um celular Android via Termux.
Cliente MQTT Secundário: Um computador pessoal para gestão e visualização (ex.: usando IoT MQTT Panel).
Periféricos Utilizados:
LED RGB (pinos 11, 12, 13) como lâmpada.
Buzzers (pinos 10, 21) como caixa de som.
Joystick (eixos X em GPIO 27/ADC1 e Y em GPIO 26/ADC0) como sensor de presença.
Sensor de temperatura onboard (ADC4).



Requisitos
Hardware

Placa BitDogLab (Raspberry Pi Pico W)
Celular Android com Termux e Mosquitto instalado
Computador pessoal para cliente MQTT secundário
LED RGB, buzzers e joystick conectados conforme os pinos especificados

Software

Pico SDK (versão 2.1.1 ou superior)
CMake e Ninja para compilação
Mosquitto (broker MQTT)
Termux (para rodar o broker no Android)
IoT MQTT Panel (opcional, para visualização)
Ferramentas de desenvolvimento: VS Code ou outro IDE compatível

Instalação e Configuração
1. Configurar o Ambiente de Desenvolvimento

Clone o repositório:git clone https://github.com/Davileao10/SmartHomeMQTT.git
cd SmartHomeMQTT


Configure o Pico SDK no seu ambiente (consulte a documentação oficial).
Certifique-se de que PICO_SDK_PATH está definido no ambiente.

2. Configurar o Broker MQTT

No celular Android, instale o Termux e o Mosquitto:pkg install mosquitto
mosquitto -d


Verifique o IP do celular na rede (ex.: 192.168.0.11) e atualize o #define MQTT_SERVER no arquivo mqtt_client.c.

3. Configurar o Wi-Fi

Atualize as credenciais Wi-Fi no arquivo mqtt_client.c:#define WIFI_SSID "SEU_SSID"
#define WIFI_PASSWORD "SUA_SENHA"



4. Compilar e Gravar o Código

No diretório do projeto, crie o diretório build e compile:mkdir build
cd build
cmake ..
ninja


Copie o arquivo gerado mqtt_client.uf2 para o Raspberry Pi Pico W (conecte a placa segurando o botão BOOTSEL e solte para montar como unidade USB).

Uso

Iniciar o Broker MQTT:

No Termux, certifique-se de que o Mosquitto está rodando:mosquitto -d




Conectar o Pico W:

Ligue o Raspberry Pi Pico W. Ele se conectará automaticamente ao Wi-Fi e ao broker MQTT.
Abra o Serial Monitor (115200 baud) para ver os logs:Temperatura: 36.69
Presença: 1




Controlar o Sistema:

Use um cliente MQTT (ex.: IoT MQTT Panel ou um script em um computador) para se conectar ao broker.
Publique mensagens nos tópicos:
/picoe661/luz/ligar: "1" ou "LIGADO" para ligar o LED RGB (lâmpada), "0" ou "DESLIGADO" para desligar.
/picoe661/musica/ligar: "1" ou "LIGADO" para tocar a música, "0" ou "DESLIGADO" para parar.


Inscreva-se nos tópicos /temperatura e /picoe661/presenca para receber os dados de temperatura e presença.



Estrutura do Código

mqtt_client.c: Código principal que implementa o cliente MQTT, controle de periféricos e lógica do sistema.
Outros arquivos (como CMakeLists.txt) estão incluídos para a compilação com o Pico SDK.



