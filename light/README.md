# Light

This example creates a Color Temperature Light device using the ESP
Matter data model.

See the [docs](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html) for more information about building and flashing the firmware.

## 1. Additional Environment Setup

No additional setup is required.

## 2. Post Commissioning Setup

No additional setup is required.

## 3. Device Performance

### 3.1 Memory usage

The following is the Memory and Flash Usage.

-   `Bootup` == Device just finished booting up. Device is not
    commissionined or connected to wifi yet.
-   `After Commissioning` == Device is connected to wifi and is also
    commissioned and is rebooted.
-   device used: esp32c3_devkit_m
-   tested on:
    [6a244a7](https://github.com/espressif/esp-matter/commit/6a244a7b1e5c70b0aa1bf57254f19718b0755d95)
    (2022-06-16)

|                         | Bootup | After Commissioning |
|:-                       |:-:     |:-:                  |
|**Free Internal Memory** |108KB   |105KB                |

**Flash Usage**: Firmware binary size: 1.26MB

This should give you a good idea about the amount of free memory that is
available for you to run your application's code.

Applications that do not require BLE post commissioning, can disable it using app_ble_disable() once commissioning is complete.

## ESP32-2432S028R: tela e toque

O suporte fica ativo por padrao no ESP32 (`CONFIG_CYD_DISPLAY=y`). A tela
ILI9341 exibe LIGADA/DESLIGADA e um botao LIGAR/DESLIGAR. Toda a area de
toque funciona como esse botao, sem calibracao de coordenadas. Pressione por
pelo menos 80 ms e solte antes de tocar novamente. Segurar o dedo nao repete
o comando. Mudancas feitas por um controlador Matter atualizam a tela.

O toque usa `attribute::update` na tarefa do Matter; nao altera diretamente
a saida. A leitura do estado usa o bloqueio da pilha Matter. Nao requer LVGL
nem novos componentes externos.

Pinos internos (placa com ILI9341 e XPT2046):

| Funcao | GPIOs |
| --- | --- |
| TFT MOSI / MISO / CLK / CS / DC / backlight | 13 / 12 / 14 / 15 / 2 / 21 |
| Touch MOSI / MISO / CLK / CS / IRQ | 32 / 39 / 25 / 33 / 36 |
| Saida de teste: LED vermelho, ativo baixo | 4 |

Como a saida da iluminacao externa ainda nao foi definida, este perfil usa
somente o LED vermelho integrado como demonstracao de liga/desliga. Brilho
e cor continuam no modelo Matter, mas nao modificam esse LED neste perfil.
Em `idf.py menuconfig` > Example Configuration, configure `CYD light output
GPIO` e `CYD light output is active low` para a saida desejada. GPIOs de tela,
toque, flash e botao BOOT sao rejeitados na compilacao. Desative o suporte
CYD para voltar ao driver original da placa ESP32 DevKit.

Compile no ambiente ESP-Matter configurado:

```sh
cd light
idf.py build
idf.py -p PORTA_SERIAL flash monitor
```

Validacao na placa: conferir imagem e estado inicial; tocar para alternar;
manter pressionado por alguns segundos (uma unica alternancia); soltar e
tocar novamente; alterar liga/desliga pelo Matter e conferir tela e LED;
reiniciar e conferir o estado restaurado. A tela permanecer acesa quando a
luz estiver desligada. Nao ha acao de reset de fabrica pelo toque.

Referencias de hardware e inicializacao:
- https://esp3d.io/esp3d-tft/version_1x/hardware/esp32/sunton-28-2432/
- https://github.com/Bodmer/TFT_eSPI/blob/master/TFT_Drivers/ILI9341_Init.h

A integracao ainda requer compilacao com ESP-Matter e teste na placa.
Os binarios existentes na raiz nao incluem estas alteracoes.

## ESP32-S2: teste Matter sem tela

O workflow seleciona `esp32s2`. Esse alvo usa o perfil `hollow` do ESP-Matter:
o display CYD fica desativado. O rele e o interruptor usam GPIOs conforme descrito abaixo. O estado
da luz e seus atributos podem ser controlados pelo Matter e consultados pelo
console serial.

A ESP32-S2 nao possui Bluetooth. Para comissionar, configure o Wi-Fi pelo
console serial e use um controlador com comissionamento Matter na rede IP
(on-network). O pareamento inicial por Bluetooth nao funciona nesse alvo.

O firmware ESP32-S2 deve ser gravado somente na nova placa ESP32-S2.
A ESP32-2432S028R continua sendo alvo `esp32`.

## Interruptor e rele na ESP32-S2

- GPIO 2: interruptor de contato seco ligado entre GPIO 2 e GND; pull-up interno.
- GPIO 4: entrada IN de modulo rele compativel com sinal de 3,3 V; ativo em LOW.
- GND da placa e do modulo rele devem estar conectados.

Cada mudanca estavel de posicao do interruptor alterna o atributo OnOff do
Matter, com debounce de aproximadamente 80 ms. Um interruptor mantido na
mesma posicao nao repete comandos. Este comportamento e para interruptor
convencional de duas posicoes; um botao momentaneo alternaria ao pressionar
e ao soltar. O comando passa pela tarefa Matter, assim aplicativo e rele
permanecem sincronizados. O interruptor nao executa reset de fabrica.

O rele inicia inativo antes de aplicar o estado Matter restaurado. Em uma
instalacao nova, o estado inicial e desligado. Brilho e cor nao controlam o
rele. A opcao RELAY_ACTIVE_LOW no menuconfig permite mudar a polaridade.

Use apenas contato seco no GPIO 2. Nunca aplique tensao da rede eletrica nos
GPIOs e nao ligue a bobina de um rele diretamente ao GPIO 4: use um modulo
com driver adequado. As ligacoes da lampada ficam nos contatos do rele.
