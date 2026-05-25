#include <SPI.h>        // Comunicação SPI do módulo de Cartão SD
#include <SD.h>         // Manipulação de arquivos no Cartão SD
#include <Wire.h>       // Comunicação I2C do módulo RTC
#include <RTClib.h>     // Controle do Real Time Clock (RTC)

const int BOTAO = 2;          // Botão
const int LED_VERMELHO = 12;  // LED Vermelho
const int LED_VERDE = 13;     // LED Verde
const int LED_AMARELO = 8;    // LED Amarelo
const int RELE = 7;           // Relé 2 vias

const int pinoSensor = A0;    // Pino analógico A0 conectado ao sensor de umidade
const int pinoSD = 10;        // Pino digital 10 do Cartão SD

int contadorCliques = 0;        // Variável para armazenar o modo atual (0, 1 ou 2)
int estadoBotaoAnterior = LOW;   // Estado anterior do botão para detectar clique

const int valorSeco = 1023;     // Valor analógico lido com o sensor totalmente seco ( 100% )
const int valorMolhado = 323;   // Valor analógico lido com o sensor imerso em água ( 0% )

unsigned long intervaloRega = 3600000; // Tempo de intervalo entre leituras em ms (1h padrão)
unsigned long tempoAnterior = 0;    // Armazena o tempo da última execução do modo inteligente

RTC_DS1307 rtc; // Cria objeto necessário p/ gerenciar módulo RTC - Real

void setup() { // Função de configuração inicial executada uma vez ao ligar o Arduino
  Serial.begin(9600); // Inicializa a comunicação serial a 9600 bits por segundo

  // Entrada e Saídas digitais
  pinMode(BOTAO, INPUT);           // Botão 
  pinMode(LED_VERMELHO, OUTPUT);   // LED Vermelho
  pinMode(LED_VERDE, OUTPUT);      // LED Verde
  pinMode(LED_AMARELO, OUTPUT);    // LED Amarelo
  pinMode(RELE, OUTPUT);           // Relé 2 vias

  // Módulo RTC 
  if (!rtc.begin()) { // Verifica se a comunicação com o módulo RTC falhou ao iniciar
    Serial.println("Aviso: RTC nao encontrado!"); // Exibe mensagem de erro do RTC no monitor serial
  } // Fecha a estrutura de validação do RTC
  
  // Cartão SD
  if (!SD.begin(pinoSD)) { // Verifica se a inicialização do Cartão SD falhou no pino 10
    Serial.println("Aviso: Falha no Cartao SD!"); // Exibe mensagem de erro do SD no monitor serial

  } else { // Caso o Cartão SD seja inicializado com absoluto sucesso

    if (!SD.exists("irrigaca.csv")) { // Verifica se o arquivo de log ainda não existe no cartão
      File arquivoLog = SD.open("irrigaca.csv", FILE_WRITE); // Cria e abre o arquivo CSV para escrita

      if (arquivoLog) { // Verifica se o arquivo foi aberto corretamente sem falhas
        arquivoLog.println("Data e Hora;Umidade (%);Status do Rele"); // Escreve a linha de cabeçalho do CSV
        arquivoLog.close(); // Fecha o arquivo para salvar as alterações fisicamente
        Serial.println("Arquivo CSV criado com cabecalho."); // Confirma a criação do CSV na serial

      } // Fecha o bloco de verificação de abertura do arquivo
    } // Fecha o bloco de verificação de existência do arquivo
  } // Fecha o bloco condicional else do Cartão SD

  digitalWrite(LED_VERMELHO, HIGH); // Liga o LED Vermelho indicando o Estado Inicial do sistema
  digitalWrite(LED_VERDE, LOW);     // Mantém o LED Verde desligado no início do sistema
  digitalWrite(LED_AMARELO, LOW);   // Mantém o LED Amarelo desligado no início do sistema
  digitalWrite(RELE, LOW);          // Ativa o Relé no início junto com o LED 1 (Lógica Invertida)
} // Fecha a função de configuração setup()

void loop() { // Função de loop principal executada continuamente pelo Arduino
  int estadoBotaoAtual = digitalRead(BOTAO); // Lê e armazena o estado elétrico atual do botão

  if (estadoBotaoAtual == HIGH && estadoBotaoAnterior == LOW) { // Detecta se o botão foi pressionado 
    contadorCliques++; // Incrementa em 1 a variável de contagem de cliques do botão

    if (contadorCliques > 2) { // Verifica se o contador passou do limite de modos (maior que 2)
      contadorCliques = 0; // Reinicia o contador para o modo 0 (Volta ao início)
    } // Fecha a validação de limite do contador

    if (contadorCliques == 0) { // Verifica se o modo selecionado é o Modo 0 (Fechado)
      digitalWrite(LED_VERMELHO, HIGH); // Liga o LED Vermelho indicando modo manual fechado
      digitalWrite(LED_VERDE, LOW);     // Desliga o LED Verde para evitar falsas indicações
      digitalWrite(LED_AMARELO, LOW);   // Desliga o LED Amarelo para limpar a sinalização visual
      digitalWrite(RELE, HIGH);         // Desativa o Relé interrompendo o fluxo de água (Lógica Invertida)
    } // Fecha o bloco do Modo 0

    else if (contadorCliques == 1) { // Verifica se o modo selecionado é o Modo 1 (Aberto)
      digitalWrite(LED_VERMELHO, LOW);  // Desliga o LED Vermelho na troca de modo manual
      digitalWrite(LED_VERDE, HIGH);    // Liga o LED Verde indicando modo manual aberto (irrigando)
      digitalWrite(LED_AMARELO, LOW);   // Desliga o LED Amarelo mantendo apenas a indicação correta
      digitalWrite(RELE, LOW);          // Ativa o Relé abrindo a válvula solenoide (Lógica Invertida)
    } // Fecha o bloco do Modo 1

    else if (contadorCliques == 2) { // Verifica se o modo selecionado é o Modo 2 (Programado)
      digitalWrite(LED_VERMELHO, LOW);  // Desliga o LED Vermelho para a transição do sistema
      digitalWrite(LED_VERDE, LOW);     // Desliga o LED Verde limpando os modos manuais anteriores
      digitalWrite(LED_AMARELO, HIGH);  // Liga o LED Amarelo indicando que o modo inteligente está ativo
      digitalWrite(RELE, HIGH);         // Inicia com o Relé desativado por segurança antes da leitura
    } // Fecha o bloco do Modo 2

    delay(50); // Aguarda 50 milissegundos para filtrar ruídos mecânicos do botão (Debounce)
  } // Fecha o bloco condicional de clique do botão

  estadoBotaoAnterior = estadoBotaoAtual; // Atualiza o histórico salvando o estado atual do botão

  if (contadorCliques == 2) { // Condicional que monitora se o sistema está operando no Modo 2
    logicaProgramada(); // Chama a função que executa a automação inteligente e o registro de dados
  } // Fecha a condicional de verificação do Modo 2
} // Fecha a função de loop principal loop()

void logicaProgramada() { // Função isolada para a lógica do modo inteligente
  unsigned long tempoAtual = millis(); // Coleta o tempo atual de operação do Arduino em milissegundos
  
  if (tempoAtual - tempoAnterior >= intervaloRega || tempoAnterior == 0) { // Verifica se o intervalo passou ou se é a primeira execução
    tempoAnterior = tempoAtual; // Atualiza a variável com o tempo atual para a próxima contagem
    
    int leitura = analogRead(pinoSensor); // Realiza a leitura analógica do sensor capacitivo no pino A0
    int umidade = map(leitura, valorSeco, valorMolhado, 0, 100); // Mapeia o valor bruto lido para uma escala de 0 a 100%
    umidade = constrain(umidade, 0, 100); // Garante que o valor resultante fique obrigatoriamente entre 0 e 100%
    
    // Limites
    int limiteMinimo = 40; // Define a constante do limite de umidade mínima em 40% (Solo Seco)
    int limiteMaximo = 80; // Define a constante do limite de umidade máxima em 80% (Solo Úmido)
    
    // Condicional que verifica se a umidade está abaixo do limite mínimo
    if (umidade < limiteMinimo) { 
      digitalWrite(RELE, LOW);  // Liga o relé abrindo a válvula de água (Lógica Invertida)
    } // Fecha o bloco de umidade mínima
    
    // Condicional que verifica se a umidade superou o limite máximo
    else if (umidade > limiteMaximo) { 
      digitalWrite(RELE, HIGH); // Desliga o relé fechando a válvula de água (Lógica Invertida)
    } // Fecha o bloco de umidade máxima
    
    DateTime agora = rtc.now(); // Coleta a data e o horário atualizados diretamente do chip RTC
    File arquivoLog = SD.open("irrigaca.csv", FILE_WRITE); // Abre o arquivo CSV em modo de adição de escrita
    
    // Condicional que valida se o arquivo foi aberto com sucesso para gravação
    if (arquivoLog) { 
      arquivoLog.print(agora.day(), DEC); // Grava o dia atual em formato decimal no arquivo
      arquivoLog.print('/'); // Grava a barra separadora da data no arquivo CSV
      arquivoLog.print(agora.month(), DEC); // Grava o mês atual em formato decimal no arquivo
      arquivoLog.print('/'); // Grava a segunda barra separadora da data no arquivo CSV
      arquivoLog.print(agora.year(), DEC); // Grava o ano com quatro dígitos no arquivo CSV
      arquivoLog.print(' '); // Grava um espaço em branco separando a data do horário
      
      if (agora.hour() < 10) arquivoLog.print('0'); // Adiciona um zero à esquerda caso a hora seja menor que 10
      arquivoLog.print(agora.hour(), DEC); // Grava a hora atualizada no arquivo CSV
      arquivoLog.print(':'); // Grava os dois-pontos separadores de horas e minutos

      if (agora.minute() < 10) arquivoLog.print('0'); // Adiciona um zero à esquerda caso o minuto seja menor que 10
      arquivoLog.print(agora.minute(), DEC); // Grava os minutos atuais no arquivo CSV
      arquivoLog.print(';'); // Grava o caractere ponto e vírgula como delimitador da primeira coluna      
      arquivoLog.print(umidade); // Grava o valor percentual da umidade calculado na segunda coluna      
      arquivoLog.print(';'); // Grava o caractere ponto e vírgula como delimitador da segunda coluna      
      
      arquivoLog.println(digitalRead(RELE) == LOW ? "ABERTO" : "FECHADO"); // Grava o estado da válvula e quebra a linha do arquivo - 
      // Operador ternário : Condição ? Valor se for Verdadeiro : Valor se for Falso
      
      arquivoLog.close(); // Fecha o arquivo de log salvando permanentemente os dados no Cartão SD
      
      Serial.println("Dados adicionados ao arquivo CSV!"); // Imprime mensagem de sucesso no monitor serial
      
    } else { // Caso ocorra alguma falha física ou lógica na abertura do arquivo do Cartão SD
      Serial.println("Erro ao abrir o arquivo CSV para gravacao."); // Alerta de falha no monitor serial
      
    } // Fecha o bloco condicional de gravação do arquivo

  } // Fecha o bloco de controle de intervalo de tempo

} // Fecha a função isolada logicaProgramada()