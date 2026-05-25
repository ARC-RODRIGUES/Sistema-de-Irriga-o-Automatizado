# Sistema de Irrigação Automatizado com Data Logging

Este repositório contém o firmware em C/C++ desenvolvido para a plataforma Arduino, projetado para o controle automatizado de irrigação baseado em leitura capacitiva de umidade do solo e auditoria contínua de eventos via armazenamento em Cartão SD e marcação temporal em tempo real (RTC).

## 1. Arquitetura do Sistema e Hardware

O circuito foi projetado prevendo estabilidade no acionamento dos periféricos e precisão na aquisição de dados. Os pinos de I/O e barramentos de comunicação estão distribuídos conforme a seguinte especificação técnica:

* **Unidade de Processamento:** Arduino ATmega328P ou compatível.
* **Controle de Fluxo (Atuador):** Módulo Relé de 2 vias conectado ao **Pino Digital 7**. Opera sob lógica invertida (nível lógico `LOW` ativa o relé/solenoide; nível lógico `HIGH` cessa o fluxo de água).
* **Sensoriamento de Solo:** Sensor de Umidade Capacitivo conectado ao **Pino Analógico A0**.
    * *Calibração de Campo:* Limiar Seco (`100%` de aridez) parametrizado em `1023`; Limiar Úmido (`0%` de aridez / imersão total) parametrizado em `323`.
* **Barramento de Armazenamento (SPI):** Módulo de Cartão SD com pino **Chip Select (CS) mapeado no Pino Digital 10**. O sistema de arquivos adota formatação FAT para escrita direta de dados.
* **Barramento de Tempo Real (I2C):** Módulo RTC DS1307 comunicando via pinos SDA/SCL nativos para persistência de carimbo de data e hora.
* **Interface de Usuário (HMI):**
    * Push-button tátil no **Pino Digital 2** com leitura digital para comutação de estados.
    * LED Vermelho (**Pino Digital 12**) -> Sinalização de Modo Manual Fechado.
    * LED Verde (**Pino Digital 13**) -> Sinalização de Modo Manual Aberto.
    * LED Amarelo (**Pino Digital 8**) -> Sinalização de Automação Inteligente Ativa.

## 2. Máquina de Estados e Modos de Operação

O firmware implementa uma lógica de transição de estados sequencial alternada pelo acionamento do botão físico. Um filtro de *debounce* de 50 milissegundos é executado por software para prevenir leituras falsas causadas por ruídos eletromecânicos do componente táctil.

* **Modo 0: Manual Fechado (Estado Inicial de Segurança):** O relé é forçado ao estado desativado (`HIGH`), garantindo que a válvula solenoide permaneça fechada. A sinalização visual ativa indica apenas o LED Vermelho ligado.
* **Modo 1: Manual Aberto (Irrigação Forçada):** O relé é forçado ao estado ativo (`LOW`), ignorando completamente os parâmetros do sensor de umidade. Utilizado para testes de vazão em campo. A sinalização visual ativa indica apenas o LED Verde ligado.
* **Modo 2: Automação Inteligente (Monitoramento Autônomo):** O sistema passa a delegar o controle à função de lógica programada. O relé inicia desativado por padrão de segurança até o primeiro ciclo de varredura. A sinalização visual ativa indica apenas o LED Amarelo ligado.

## 3. Lógica Programada e Algoritmo de Controle

Ao operar no **Modo 2**, o algoritmo executa uma rotina cíclica baseada no contador interno `millis()` do microcontrolador. Essa abordagem evita o uso de funções bloqueantes (como `delay`), permitindo que a interface física continue respondendo instantaneamente a novos comandos do botão.

* **Periodicidade:** A amostragem do sensor e a escrita dos logs ocorrem em intervalos fixos de 1 hora (`3.600.000 ms`). Uma leitura inicial é disparada imediatamente na transição para este modo para evitar janelas cegas de amostragem.
* **Tratamento de Dados:** A leitura analógica bruta do pino A0 é processada pela função `map()` utilizando as constantes inversas de calibração. Na sequência, os valores sofrem restrição através da função `constrain()` para mitigar leituras espúrias fora do range padrão de `0%` a `100%`.
* **Controle por Histérese:**
    * Se a umidade calculada for **menor que 40%**, o solo é considerado seco. O firmware envia nível lógico `LOW` para o **Pino 7**, abrindo a linha de água.
    * Se a umidade calculada for **maior que 80%**, o solo atingiu a capacidade ideal de campo. O firmware envia nível lógico `HIGH` para o **Pino 7**, interrompendo o fluxo.
    * Valores que oscilam dentro do intervalo intermediário mantêm o último estado de atuação do relé, protegendo o atuador contra chaveamentos rápidos causados por acomodação rápida da umidade na terra.

## 4. Estrutura do Arquivo de Log (.CSV)

Durante a inicialização do circuito, o sistema verifica a presença do arquivo `irrigaca.csv` na raiz do Cartão SD. Caso o arquivo não exista, ele é criado automaticamente e estruturado com a linha de cabeçalho padrão.

A cada ciclo de amostragem no Modo 2, os dados são anexados em modo append (`FILE_WRITE`) estruturados com delimitador ponto e vírgula (`;`), facilitando a posterior importação e análise de dados em softwares de BI ou planilhas eletrônicas. O formato segue o padrão abaixo:

```csv
Data e Hora;Umidade (%);Status do Rele
24/05/2026 16:30;35;ABERTO
24/05/2026 17:30;62;ABERTO
24/05/2026 18:30;81;FECHADO