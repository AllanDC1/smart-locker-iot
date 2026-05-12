# Smart Locker IOT (Arduino Uno)

Projeto de cofre inteligente para a disciplina de IoT, com foco em integração de periféricos, lógica por estados e monitoramento de segurança.

## 1) Visao geral

Este projeto implementa um smart locker com:

- LCD para orientar o usuario
- Senha por sequencia de 3 botoes (validacao automatica)
- Trava fisica por servomotor
- Bloqueio temporario apos 3 tentativas incorretas
- Monitoramento de violacao com sensor de luz (LDR)
- Modo emergencia com um botao de hardware (toggle)

## 2) Componentes

- 1x Arduino Uno
- 1x LCD 16x2
- 1x Servomotor
- 1x Buzzer
- 1x LDR + resistor 10k (divisor de tensao)
- 4x botoes
	- 3 botoes para senha
	- 1 botao de emergencia (toggle trava/destrava)
- Protoboard e jumpers

## 3) Mapeamento de pinos

### Entradas de senha

- Botao 1 -> A0 (digital)
- Botao 2 -> A1 (digital)
- Botao 3 -> A2 (digital)

### Emergencia (interrupcoes)

- EMERGENCY TOGGLE -> D2 (INT0)

### Atuadores e sensores

- Buzzer -> D8
- Servo -> D9
- LDR -> A4 (analogico)

### LCD 16x2 (LiquidCrystal paralelo)

- RS -> D12
- E -> D11
- D4 -> D7
- D5 -> D6
- D6 -> D5
- D7 -> D4

Observacao:
Todos os botoes estao configurados com INPUT_PULLUP, entao o nivel ativo do botao e LOW (pressionado).

## 4) Maquina de estados

- STATE_LOCKED:
	- Trava fechada
	- LCD pede senha
	- LDR monitorando tentativa de violacao

- STATE_OPEN:
	- Trava aberta por 8 segundos
	- LCD mostra contador regressivo
	- Fecha automaticamente ao final

- STATE_BLOCKED:
	- Acionado apos 3 erros de senha
	- Buzzer faz 3 apitos
	- LCD bloqueado por 5 segundos com contador

- STATE_ALARM:
	- Acionado por excesso de luz detectado pelo LDR por tempo minimo
	- Buzzer periodico e mensagem de alerta
	- Retorna para travado apos 10 segundos

- STATE_EMERGENCY:
	- Acionado por interrupcao no botao de emergencia
	- Trava imediatamente e ignora senha
	- O mesmo botao alterna para sair do modo emergencia

## 5) Logica de seguranca com LDR

Estrategia para monitoramento:

- Na inicializacao, o sistema calcula um baseline medio de luz ambiente.
- Em estado travado, se a leitura exceder baseline + delta por tempo continuo, dispara alerta.

No firmware atual:

- Delta de disparo: 150
- Confirmacao de exposicao: 2 segundos

Esses valores podem ser ajustados para o seu ambiente/luminosidade.

## 6) Senha

- Senha padrao: 1-2-3
- Tamanho: 3 digitos
- A validacao é automática apos o 3o digito
- Se errar a sequência, a entrada é limpa e inicia nova tentativa

Para alterar senha/tamanho, ajuste no arquivo smart-locker-iot.ino:

- PASSWORD_LENGTH
- PASSWORD

## 7) Como carregar

1. Abra smart-locker-iot.ino na IDE Arduino.
2. Selecione placa Arduino Uno e porta correta.
3. Compile e envie para a placa.
4. Monte o circuito conforme o mapeamento de pinos.