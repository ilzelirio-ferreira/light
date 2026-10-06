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
