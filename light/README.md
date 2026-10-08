# Light

Controlador ESP32-S2 com seis reles independentes via Matter, seis entradas
de 60 Hz por optoacopladores e portal web para Wi-Fi, nomes e OTA.

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

## ESP32-S2: teste Matter sem tela

O workflow seleciona `esp32s2`. Esse alvo usa o perfil `hollow` do ESP-Matter:
o LCD e o toque foram removidos do projeto. O rele e o interruptor usam GPIOs conforme descrito abaixo. O estado
da luz e seus atributos podem ser controlados pelo Matter e consultados pelo
console serial.

A ESP32-S2 nao possui Bluetooth. Para comissionar, configure o Wi-Fi pelo
console serial e use um controlador com comissionamento Matter na rede IP
(on-network). O pareamento inicial por Bluetooth nao funciona nesse alvo.

O firmware ESP32-S2 deve ser gravado somente na nova placa ESP32-S2.
O firmware de seis canais e o portal destinam-se a ESP32-S2.

## Seis interruptores e reles na ESP32-S2

Cada canal e um endpoint Matter On/Off independente, na ordem abaixo:

| Canal | Saida do rele (S) | Entrada do interruptor (E) |
| --- | --- | --- |
| 1 | GPIO 39 | GPIO 40 |
| 2 | GPIO 37 | GPIO 38 |
| 3 | GPIO 35 | GPIO 36 |
| 4 | GPIO 33 | GPIO 34 |
| 5 | GPIO 18 | GPIO 21 |
| 6 | GPIO 16 | GPIO 17 |

As saidas sao ativas em LOW. Cada entrada recebe o coletor (pino 4) de
um PC817, com emissor (pino 3) ao GND do ESP32 e pull-up para 3,3 V.
O pull-up interno fica habilitado; recomenda-se pull-up externo de 10 kohms.
GPIO 2 e GPIO 4 deixam de ser usados; GPIO 19/20 ficam para USB nativo.

A entrada detecta pulsos ativos em LOW de meia onda a 60 Hz, com periodos
entre 14 e 19 ms, tolerando ate dois ciclos perdidos. Bordas espurias com
menos de 2 ms sao ignoradas. Quatro periodos validos qualificam o sinal;
500 ms sem pulsos validos indicam ausencia. Uma mudanca entre presenca
e ausencia precisa permanecer estavel por mais 200 ms antes de alternar
o rele (aproximadamente 700 ms para reconhecer a retirada da rede).
Um nivel LOW constante nao
conta como presenca. Retificacao de onda completa (120 Hz) nao e suportada
por esta configuracao. Interrupcoes capturam os pulsos independentemente
da tarefa de controle.

Cada transicao entre presenca e ausencia inverte somente o rele correspondente.
O sinal mantido nao repete comandos nem impede comandos pelo Matter.
A leitura inicial, apos 750 ms, nao altera os estados Matter restaurados.
Os comandos passam pela tarefa Matter.

O circuito de rede de 127 V precisa de protecao reversa para o LED do PC817
(diodo em antiparalelo: catodo no pino 1, anodo no pino 2). O limite reverso
do PC817 e 6 V. Com 47 kohms, a dissipacao aproximada do conjunto resistivo
com esse diodo e 127^2/47000 = 0,34 W. Dimensione potencia com margem,
tensao de trabalho, isolacao e distancias da placa para a rede; o esquema
original sem diodo nao deve ser ligado a rede. Nao conecte neutro ao GND
do ESP32. Os GPIOs recebem somente o lado isolado de 3,3 V.

Os seis reles iniciam inativos antes de aplicar seus estados restaurados;
para uma instalacao nova, os estados iniciais sao desligados. Brilho e cor
nao sao anunciados pelos endpoints de rele. RELAY_ACTIVE_LOW permite mudar
a polaridade das seis saidas juntas no menuconfig.

O limite de endpoints foi ampliado para sete: raiz e seis canais. Ao migrar
do firmware de um canal, o controlador Matter pode precisar redescobrir ou
adicionar novamente o dispositivo para apresentar os seis endpoints.

Use modulos rele compativeis com sinais de 3,3 V e optoacopladores nas entradas.
Nao ligue bobinas diretamente aos GPIOs e nunca aplique rede eletrica neles.

Validacao na placa: acionar cada interruptor nos dois sentidos e confirmar
somente seu rele; controlar cada endpoint pelo Matter; testar mudancas
simultaneas, pulsos com ruido e reinicializacao com restauracao de estados.

## Portal Wi-Fi, nomes e OTA web (ESP32-S2)

Depois de 30 segundos sem conexao Wi-Fi, aparece a rede `Light-XXXXXX`.
Conecte usando a senha inicial `configurar123`; o portal captive deve abrir.
Se nao abrir automaticamente, acesse http://192.168.4.1/ no navegador.
Entre no painel com a mesma senha. Configure uma rede de 2,4 GHz, o nome
do modulo e os nomes dos seis canais. Os canais aceitam ate 16 bytes
(Matter Fixed Label); o modulo aceita ate 32 bytes (Matter NodeLabel).
Letras acentuadas podem ocupar mais de um byte. Os nomes iniciais sao
`Light 6 canais` e `Luz 1` a `Luz 6`.

O painel permite mudar sua senha (8 a 63 caracteres ASCII), que tambem
protege a rede de configuracao. Depois de salvar, a placa reinicia.
Deixar a senha Wi-Fi vazia mantem a senha da mesma rede; para trocar
para uma rede aberta, marque explicitamente `A nova rede nao tem senha`.
Os nomes e a senha de acesso ficam na NVS; a rede e a senha Wi-Fi ficam
no armazenamento Wi-Fi da ESP-IDF. Nenhuma senha e devolvida pela API.

Quando conectada, abra http://IP_DA_PLACA/ para configurar ou atualizar.
A rede de configuracao e desativada apos 30 segundos online e sem clientes;
ela reaparece se o Wi-Fi permanecer desconectado por 30 segundos.
Os controles Matter e as entradas de 60 Hz continuam funcionando.
Os nomes sao expostos no Matter, mas Alexa/Google/Home Assistant podem
manter nomes locais e exigir renomeacao no aplicativo.

Na secao `Atualizar firmware`, envie somente `light.bin` do artifact
`light-firmware` do GitHub. Nao envie bootloader, tabela de particoes ou
uma imagem completa mesclada. A API recebe o binario bruto em POST
`/api/ota` com cabecalho `X-Portal-Key` e Content-Type
`application/octet-stream`. A imagem deve ser do projeto `light`, alvo
ESP32-S2, e caber na particao de 0x1E0000 bytes (1.875 MiB).
O boot slot so muda depois de validar a imagem inteira. Upload incompleto
ou firmware invalido preserva a versao atual. Uma atualizacao que falhe
antes de concluir a inicializacao usa rollback do bootloader.
No perfil ESP32-S2, o OTA web substitui o requestor OTA Matter para evitar
atualizacoes concorrentes na mesma particao.

A primeira instalacao desta versao precisa ser pela serial, pois o firmware
anterior nao possui servidor web e o bootloader precisa habilitar rollback.
As proximas atualizacoes podem ser pela pagina. O painel usa HTTP na rede
local e exige senha para ler configuracoes, salvar ou atualizar.

Validacao na placa: configurar uma rede valida e uma senha errada (portal
reaparece); conferir os sete nomes depois de reiniciar; parear/controlar
os seis endpoints Matter; atualizar um light.bin valido; rejeitar arquivo
invalido, de outra placa e upload interrompido sem trocar o boot slot.
