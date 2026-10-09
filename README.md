# Telemetria Base
Esse projeto foi criado com o intuito de auxiliar a visualização de dados do barco solar, conhecido como Fênix,
mantido pela equipe Hurakan da Universidade do Estado de Santa Catarina (UDESC) CCT. Os integrantes agora conseguem
ler dados de sensores do  barco, como tensão de bateria e temperatura diretamente do celular. Além disso,
estimativas de telemetria são calculadas, como autonomia do barco.

# Instalação
- Clone o repositório esp-idf criado pela Espressif Systems.
```
git clone --recursive https://github.com/espressif/esp-idf.git
```
Esse processo pode demorar um pouco.  

- Execute o script de instalação adequado, ex:
```
./install.sh
```

- Carregue as variáveis de ambiente do ESP-IDF em cada novo terminal que for usado para compilar/gravar o projeto:
```
. ./export.sh
```

# Configuração (menuconfig)
O firmware usa BLE através da pilha NimBLE, que **não vem habilitada por padrão** no ESP-IDF. Como o `sdkconfig` não é
versionado (está no `.gitignore`), é necessário ativá-los manualmente em um clone novo:

1. Abra o menu de configuração:
```
idf.py menuconfig
```
2. Vá em `Component config` → `Bluetooth` e ative a opção `Bluetooth` (`CONFIG_BT_ENABLED`).
3. Em `Bluetooth` → `Host`, selecione `NimBLE - BLE only` (`CONFIG_BT_NIMBLE_ENABLED`).
4. Salve (`S`) e saia (`Q`).

Sem isso o projeto não compila (os headers do NimBLE não são encontrados) ou o BLE não funciona.

# Uso
- Defina o chip alvo (necessário apenas na primeira vez ou ao trocar de placa):
```
idf.py set-target esp32
```

- Compile o projeto:
```
idf.py build
```

- Grave o firmware no ESP32 e acompanhe os logs pela porta serial (ajuste a porta conforme o seu sistema, ex: `/dev/ttyACM0`):
```
idf.py -p /dev/ttyACM0 flash monitor
```

- Conexões de hardware esperadas pelo firmware:
  - **UART**: recebe do barco os dados de tensão e corrente instantâneas (`components/serial`).
  - **Cartão SD (SPI)**: usado para registrar o histórico de telemetria em `/telemetryData`. Pinos padrão definidos em `components/sd_card/include/sd_card.h`: MOSI = GPIO23, MISO = GPIO19, CLK = GPIO18, CS = GPIO5.
  - **BLE**: o ESP32 anuncia um dispositivo chamado `ESP32_BASE`, através do qual é possível conectar pelo celular e visualizar em tempo real os dados calculados (tensão, corrente e energia consumida).

- Fluxo de funcionamento: os dados brutos do barco chegam pela UART, são processados pela tarefa de cálculo de telemetria (que estima grandezas como energia consumida e autonomia) e o resultado é, ao mesmo tempo, disponibilizado via BLE para o aplicativo do celular e gravado no cartão SD para histórico.

