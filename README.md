# Telemetria Base
Esse projeto foi criado com o intuito de auxiliar a visualização de dados do barco solar, conhecido como Fênix,
mantido pela equipe Hurakan da Universidade do Estado de Santa Catarina (UDESC) CCT. Os integrantes conseguem ver, em
tempo real, tensão e corrente da bateria do barco, a energia consumida e a autonomia estimada, tanto por **BLE**
(aplicativo no celular) quanto por uma **página web** servida pelo próprio ESP32 por Wi-Fi. Os dados também são
registrados em um cartão SD.

Este repositório contém apenas o firmware do ESP32. O aplicativo do celular não faz parte dele.

# Instalação
- Clone este repositório **com os submódulos** (a biblioteca MAVLink é um submódulo em `components/mavlink/c_library_v2`):
```
git clone --recursive <url-deste-repositorio>
```
Em um clone que já existe: `git submodule update --init`.

- Clone o repositório esp-idf criado pela Espressif Systems.
```
git clone --recursive https://github.com/espressif/esp-idf.git
```
Esse processo pode demorar um pouco. O projeto foi compilado e testado com o ESP-IDF 6.x (checkout `v6.1-dev-6940`,
com `sdkconfig` gerado como 6.2.0).

- Execute o script de instalação adequado, ex:
```
./install.sh
```

- Carregue as variáveis de ambiente do ESP-IDF em cada novo terminal que for usado para compilar/gravar o projeto:
```
. ./export.sh
```

# Configuração
O `sdkconfig` é gerado e não é versionado (está no `.gitignore`). As opções que o firmware exige estão no
`sdkconfig.defaults`, versionado na raiz, e são aplicadas automaticamente na primeira compilação. Em um clone novo basta:

```
idf.py set-target esp32
idf.py build
```

O `set-target` só é necessário na primeira vez (ou ao trocar de placa) e cria o `sdkconfig` a partir do
`sdkconfig.defaults`. Não é preciso abrir o `menuconfig` para compilar.

O que o `sdkconfig.defaults` define, e por quê:

| Opção | Valor | Motivo |
|---|---|---|
| `CONFIG_BT_ENABLED`, `CONFIG_BT_NIMBLE_ENABLED` | `y` | BLE com a pilha NimBLE (não vem habilitada por padrão; sem ela os headers do NimBLE não são encontrados) |
| `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE` | `y` | Com Wi-Fi + BLE o binário tem cerca de 1,2 MB e **não cabe** na partição padrão de 1 MB. A partição grande (1,5 MB) cabe nos 2 MB de flash. Não há OTA |
| `CONFIG_ESP_WIFI_IRAM_OPT`, `CONFIG_ESP_WIFI_RX_IRAM_OPT` | desligadas | Ligadas, junto com o BLE, a link-edição falha por falta de IRAM (`iram0_0_seg overflowed`). O custo é só um pouco de desempenho do Wi-Fi, irrelevante aqui |

Observações:
- O `sdkconfig.defaults` só vale quando **não existe** um `sdkconfig`. Se você já tinha um de antes, aplique com:
```
rm sdkconfig
idf.py set-target esp32
```
- Para ajustes pontuais use `idf.py menuconfig` (grava só no `sdkconfig` local, que não é versionado). Se a opção
  for exigida pelo firmware, coloque-a também no `sdkconfig.defaults`.
- As opções do ponto de acesso Wi-Fi ficam em `Wi-Fi AP (telemetria)` no `menuconfig` (veja a seção Wi-Fi abaixo).

# Uso
- Compile o projeto (depois do `set-target` da seção anterior):
```
idf.py build
```

- Grave o firmware no ESP32 e acompanhe os logs pela porta serial (ajuste a porta conforme o seu sistema, ex: `/dev/ttyACM0`):
```
idf.py -p /dev/ttyACM0 flash monitor
```
Para sair do monitor: `Ctrl+]`. Se a gravação falhar com "port is busy", outro monitor está com a porta aberta.

## Conexões de hardware
| Função | ESP32 | Observação |
|---|---|---|
| UART2 RX (dados do barco) | GPIO16 | ligar no TX do barco; nível 3,3 V; GND comum |
| UART2 TX | GPIO17 | configurado, mas o firmware atual só recebe |
| SD MOSI | GPIO23 | |
| SD MISO | GPIO19 | |
| SD CLK | GPIO18 | |
| SD CS | GPIO5 | cartão formatado em FAT (o firmware não formata) |

Pinos do SD em `components/sd_card/include/sd_card.h`; pinos da UART em `components/serial/serial.c`.

## BLE
O ESP32 anuncia um dispositivo chamado `ESP32_BASE`.

- Serviço: `709b54b0-7f10-ae9a-444b-f54b459e366f`
- Característica: `719b54b0-7f10-ae9a-444b-f54b459e366f` (`READ` e `INDICATE`, sem escrita)
- Valor: texto ASCII separado por vírgulas, enviado a cada 1 s:

```
corrente,tensão,energia_usada,autonomia,energia_restante
12.34,14.80,3.52,350,1820.48
```

| Campo | Unidade | Formato |
|---|---|---|
| corrente | A | 2 casas decimais |
| tensão | V | 2 casas decimais |
| energia_usada | Wh | 2 casas decimais (consumo desde a última carga completa) |
| autonomia | min | inteiro |
| energia_restante | Wh | 2 casas decimais |

Um valor desconhecido (ainda sem dados, ou autonomia não calculável) vai como **campo vazio**, ex.: `,,0.00,,1824.00`.
O aplicativo deve tratar campo vazio como "sem dado". Apenas a última conexão que se inscreve recebe as indicações.

## Wi-Fi (ponto de acesso + página web)
O ESP32 cria uma rede Wi-Fi própria (modo AP, WPA2). Conecte o celular ou o notebook nela e abra
`http://192.168.4.1`.

| Opção (`menuconfig` → `Wi-Fi AP (telemetria)`) | Padrão |
|---|---|
| `WIFI_AP_SSID` | `Fenix Telemetria` |
| `WIFI_AP_PASSWORD` | definida em `components/wifi_ap/Kconfig.projbuild` (mínimo 8 caracteres); troque se o repositório for público |
| `WIFI_AP_CHANNEL` | 1 |
| `WIFI_AP_MAX_CONN` | 4 |

Rotas HTTP:

| Rota | Descrição |
|---|---|
| `GET /` | Painel com tensão, corrente, potência, energia usada, energia restante, autonomia e estado do link (atualiza a cada 1 s) |
| `GET /data` | JSON: `voltage`, `current`, `used_energy`, `time_remaining_min`, `energy_remaining`, `link_ok`; valor desconhecido vai como `null` |
| `POST /reset` | Zera a energia usada (usado pelo botão "Bateria carregada" do painel) |

## Cartão SD
O histórico é gravado em `/telemetryData/data.txt` (modo append), uma linha por amostra:

```
contador, corrente,tensão,energia_usada,ms_desde_o_boot
```

As linhas são acumuladas em um buffer de 1 KB e gravadas quando ele enche (cerca de 24 amostras, ~5 s a 5 Hz); o que está no buffer
se perde se a energia cair. Se o cartão não estiver presente ou a gravação falhar, o buffer é descartado (o firmware
continua funcionando sem o histórico). A cada boot, `/telemetryData/hello.txt` é reescrito como teste do cartão.

## Fluxo de funcionamento
```
barco --UART2/MAVLink--> read_serial_task --data_boat_queue--> telemetry_task_calc --data_telemetry_queue--> SD
                                |                                     |
                                +-- HEARTBEAT / V / I --> telemetry_store (mutex) <-- energia, autonomia
                                                               |
                                              BLE (1 Hz)  <----+----> página web / JSON
```

1. A tarefa `uart_task` decodifica o MAVLink e enfileira as amostras de tensão e corrente.
2. `telemetry_task_calc` integra a energia, calcula a autonomia e publica tudo em `telemetry_store`, de onde o BLE e a
   página web leem. As amostras também seguem para a tarefa que grava no SD.
