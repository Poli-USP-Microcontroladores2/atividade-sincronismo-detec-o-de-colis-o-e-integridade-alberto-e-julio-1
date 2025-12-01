# PSI-Microcontroladores2-Aula12
Atividade: Sincronismo, Detecção de Colisão e Integridade
## Alunos: 
- Alberto Galhego Neto - NUSP: 17019141
- Júlio Cesar Braga Parro - NUSP: 16879560

# Etapa 1: Modelagem e Planejamento de Testes

## 1.1. Sincronismo por Botão

### Enunciado
  A proposta é elaborar um sincronismo entre as duas placas por meio de um botão, de forma similar ao realizado na atividade de semáforos de pedestres e veículos. Dica: provavelmente os códigos não serão os mesmos, ou algum ajuste adaptativo deve ser realizado para     que uma placa esteja no modo de transmissão após o usuário apertar o botão, e a outra placa esteja no modo de recepção.


### Diagrama D2
  <img width="335" height="500" alt="Etapa 1-1 D2" src="https://github.com/user-attachments/assets/f3780593-ecc7-48fa-979f-30c2e56918f9" />


### Testes:
  #### Teste 1-1: Funcionamento básico da placa
  - Descrição/comportamento esperado: A implementação do código base de comunicação usando UART0 e UART1. As placas conseguem inicializar o modo RX e TX e alternam os modos a cada 5s, respondendo com o LED azul quando enviam mensagem e verde quando recebem
  - Critério de Aceitação: As placas realizam a troca dos modos e respondem/sinalizam adequadamente aos comandos.
  
  #### Teste 1-2: Tipos de placa.
  - Descrição/comportamento esperado: Se a variável Board_Type for 1, definida durante a compilação, iniciar como TX, caso contrário iniciar RX.
  - Critério de Aceitação: As placas iniciam no modo correto de acordo com o valor pré-definido da variável.
  
  #### Teste 1-3: Responsividade ao botão.
  - Descrição/comportamento esperado: As placas começam em modo Idle, aguardando o botão. Ao receberem o sinal, iniciam o ciclo de TX ou RX, dependendo se é a placa A ou B. Após 5s alternam para o outro modo (se TX -> RX, se RX -> TX).
  - Critério de Aceitação: As placas respondem ao botão e alteram adequadamente o modo de operação.

---

  ## 1.2. Chat Entre Placas
  ### Enunciado:
  A proposta é elaborar um sistema onde dois computadores possam enviar mensagens entre si utilizando as 2 placas, cada uma conectada a um computador. As mensagens digitadas no console do computador A (UART0), deverão ser enviadas via UART1 para o computador B, que as repetirá via UART0 para serem exibidas no console. A comunicação deverá ser bidirecional.


### Diagrama D2:
<img width="500" height="450" alt="d2" src="https://github.com/user-attachments/assets/89d1e4d0-4cb6-4156-a7d2-4b2e224b80f2" />


### Testes:
  #### Teste 2-1: Envio de mensagens via UART1
  - Descrição/comportamento esperado: As placas conseguem enviar via UART1 as mensagens digitadas no console.
  - Critério de Aceitação: Ao digitar uma mensagem no console, a placa a envia via UART1 para o outro MCU.
  
  #### Teste 2-2: Recebimento de mensagens via UART1 e replicação no console
  - Descrição/comportamento esperado: Ao receber uma mensagem via UART1, a placa a replica no console (UART0).
  - Critério de Aceitação: A placa realiza o comportamento esperado adequadamente.

---

## 1.3. Detecção de Colisão

### Enunciado
A proposta é elaborar uma detecção de colisão: logo antes de transmitir a mensagem completa, ou após transmitir cada caractere, podemos ouvir o canal (modo de recepção) para verificar se não há alguém já transmitindo, e não iniciar a transmissão caso o canal de comunicação esteja ocupado.

### Abordagem adotada
- O sincronismo entre os microcontroladores feito pelo botão, a longo prazo, pode apresentar falhas, uma vez que o envio e processamento do sinal pode ser levemente diferente para ambos. Portanto, elaborou-se um mecanismo de detecção de colisão.
- A abordagem adotada foi de escutar o canal antes de enviar a mensagem completa, ou seja, deixar o microcontrolador que vai enviar a mensagem em RX por um curto período de tempo antes de entrar em TX.

### Diagrama D2
<img width="500" height="450" alt="image" src="https://github.com/user-attachments/assets/394b3d5f-3d57-4f0a-af79-5ff90710b0ab" />


### Testes:
  #### Teste 3-1: Sistema de detecção de colisão
  - Pré-condição: Código compilado e ambos os microcontroladores tentando enviar mensagem
  - Etapas de teste: Verificar o estado dos LED's e o serial monitor durante esse caso
  - Pós-condição esperada: O microcontrolador que começou a transmissão mais tarde acende o LED amarelo como alerta, emite um LOG_WRN e aborta a mensagem

---

## 1.4. Verificação de Integridade

### Enunciado
A proposta é elaborar uma verificação de integridade: no início da mensagem, podemos enviar um hash da mensagem ou pelo menos o tamanho total da mensagem em caracteres, para que o receptor possa verificar se recebeu todos os caracteres de forma íntegra.

### Abordagem adotada
- Criou-se um pacote de dados que envia, além da mensagem, dois bytes para o tamanho da mensagem, divididos em byte mais significativo e menos significativo, e um byte para o checksum. Dessa forma, checa-se se o tamanho da mensagem recebida é o mesmo do esperado e se a mensagem em si se mantém íntegra, por meio da checagem do checksum.

### Testes
  #### Teste 4-1: Verificação da mensagem
  - Pré-condição: Código compilado e microcontroladores sincronizados
  - Etapas de teste: Verificar o estado do LED do microcontrolador que recebeu a mensagem
  - Pós-condição esperada: Se houver conflito de integridade, o LED piscará 2 vezes rapidamente na cor vermelha


# Etapa 2: Desenvolvimento Orientado a Testes

A partir da modelagem realizada e dos testes planejados, faça o desenvolvimento da solução para contemplar os 3 requisitos e passar nos 4 testes descritos.

O uso de IA Generativa é incentivado: _veja a diferença entre fazer prompts sem fornecer os requisitos e testes planejados, ou usar prompts com os diagramas e testes planejados_.

Além dos testes de cada requisito em cada etapa, faça **testes de regressão** também, para garantir que os requisitos das etapas anteriores estão funcionando (Dica: podemos ter modos de operação diferentes para testar diferentes features e não nos confundirmos com os comportamentos dos leds em cada situação).
Isto é: se o sincronismo continua funcionando após a integração da detecção de colisão, e se o sincronismo e a detecção continuam funcionando após a adição da verificação de integridade.

_Faça o upload de todos os códigos no repositório_ (pode ser em branches diferentes, ou até organizar em pull requests as diferentes features).

_Vocês devem adicionar todas as evidências de funcionamento (como por exemplo capturas de tela e fotos) dos testes realizados, mostrando todos os testes realizados no README.
As imagens e outras evidências de funcionamento devem estar descritas no README e devem estar em uma pasta chamada "results" no repositório._

## 2.1. Sincronismo por Botão
### Teste 1-1: Funcionamento básico da placa:
- **Descrição**:
	- O código funciona adequadamente. A placa consegue inicializar o modo RX e TX, alternando o modo a cada 5s, respondendo com o LED azul quando enviam mensagem e verde quando recebem.

- **Evidencias**:
  - **"Prova 1-1 - LED"**:
    - Descrição: LED acende quando envia/recebe mensagem.
    - Imagem:
    - <img width="300" height="450" alt="d2" src="https://github.com/user-attachments/assets/a2bb996e-399e-4541-912b-fbf8e61b568d" /><br/><br/>  


  - **"Prova 1-1 - Osciloscopio"**:
    - Descrição: A mensagem enviada via UART1 no osciloscópio
    - Imagem:
    - <img width="400" height="300" alt="d2" src="https://github.com/user-attachments/assets/a1833272-abdd-4823-917f-881a1e4ef097" /><br/><br/>  

      
  - **"Prova 1-1 - TXRX - Console"**:
    -  Descrição: Console mostrando o envio/recebimento das mensagens.
    -  Imagem:
    -  <img width="500" height="150" alt="Prova 1-1 - TXRX - Console" src="https://github.com/user-attachments/assets/63ad0bc0-35ab-4d0d-ba45-b98760983663" /><br/><br/>  

	
### Teste 1-2: Tipos de placa.
- **Descrição**:
  - O código funciona adequadamente. Se a variável Board_Type for 1, a placa inicia como TX, caso contrário ela inicia como RX.

- **Evidencias**:
  - **"Prova 1-2 - Console"**:
    - Descrição: Imagem do console mostrando o tipo da placa e sua inicialização.
    - Imagem:
    - <img width="433" height="345" alt="Prova 1-2 - Console" src="https://github.com/user-attachments/assets/7f24d441-fb0c-4877-9099-318a8018c6b9" /><br/><br/>  

  
### Teste 1-3: Responsividade ao botão.
- **Descrição**:
  - O código funciona adequadamente. o MCU inicia em modo Idle, sem transmitir nem enviar. Ao receber o input do botão, a placa inicia o ciclo TX ou RX, dependendo do seu Board_Type.

- **Evidencias**:
  - **"Prova 1-3 - Console"**:
    - Descrição: Imagem do console mostrando que a placa aguarda o acionamento do botão para iniciar o modo TX ou RX.
    - Imagem:
    - <img width="368" height="346" alt="Prova 1-3 - Console" src="https://github.com/user-attachments/assets/ed12c05e-10ed-4f59-937f-356b10b757d7" /><br/><br/>  

---

## 2.2. Chat Entre Placas

### Teste 2-1: Envio de mensagens via UART1

- **Descrição**:
  - As placas conseguem enviar via UART1 as mensagens digitadas no console de maneira adequada, observa-se no osciloscópio o envio das mensagens via UART1.
    
- **Evidencias**:
  - **"Prova 2-1 - Osciloscopio"**:
    - Descrição: Imagem no osciloscópio, mostrando o envio da mensagem que havia sido digitada no console ("Chat Entre Placas")
    - Imagem:
    - <img width="600" height="450" alt="Prova - Chat Funcional" src="https://github.com/user-attachments/assets/82fc7f21-6789-45b0-80ec-f2431a1e2734" /><br/><br/>  
  
### Teste 2-2: Recebimento de mensagens via UART1 e replicação no console
- **Descrição**:
    - Ao receber uma mensagem via UART1, a placa a replica corretamente no console (UART0).
      
- **Evidencias**:
  - **"Prova 2-2 - Console"**:
    - Descrição: Imagem no console, mostrando o envio e recebimento das mensagens entre os computadores utilizando os MCU.
    - Imagem:
    - <img width="600" height="400" alt="Prova - Chat Funcional" src="https://github.com/user-attachments/assets/f2238a7e-e785-4767-94eb-f8b6c48b61ab" /><br/><br/>  

    
 ---

 ## Problemas nas atividades 3 e 4
 - Durante a etapa inicial de desenvolvimento, a configuração da interface UART1 apresentou problemas nos canais de transmissão (TX) e recepção (RX). Embora o código de referência, que foi posteriormente disponibilizado pelo professor Gustavo, tenha sanado as falhas de transmissão, a recepção de dados permaneceu impactada. Posteriormente, a nossa equipe desenvolveu, de forma independente, uma solução para esse problema de configuração, viabilizando a utilização plena do canal (registrada na branch “Teste-UART1”).
	- Devido ao tempo despendido nesta correção, o escopo do projeto foi ajustado para priorizar as funcionalidades centrais (atividades 1 e 2), conforme orientação do professor Gustavo durante a aula do dia 28/11. As atividades “extras” (3 e 4) foram mantidas com implementação original parcial devido ao cronograma remanescente, demonstrando a aderência teórica à proposta, porém com cobertura de testes e/ou funcionalidade reduzida.
 
## 2.3. Detecção de Colisão

### Teste 3-1: Sistema de detecçãao de colisão
- **Descrição***:
	- Testou-se o funcionamento da detecção da colisão apenas com uma placa, enviando mensagens diretamente pelo serial monitor
	- Quando o canal estiver ocupado, aparece uma LOG_WRN indicando o erro e a mensagem que seria enviada é descartada

- **Evidências**:
	- **"Prova 3-1 - Console"**:
 <img width="600" height="400" alt="Prova - Detecção de Colisão" src="https://github.com/user-attachments/assets/9b48830e-e54e-4af8-83c9-6da41a9c36a0" />


---

## 2.4. Verificação de Integridade

### Teste 4-1: Verificação da mensagem
- **Descrição**:
   - Devido ao conflito e problemas entre as UART’S, não foi possível realizar o teste do pacote de dados e verificação de integridade. No entanto, segue os trechos do código responsáveis pela implementação e uma breve explicação sobre o seu funcionamento.
   - Para permitir a verificação, a mensagem enviada (payload) é encapsulada em um pacote que inclui um cabeçalho contendo dados essenciais, além do próprio payload: Tamanho (MSB) + Tamanho (LSB) + Checksum + Payload (n bytes)
   - O Checksum é calculado somando-se todos os bytes do payload e armazenando apenas o byte menos significativo (os 8 bits mais à direita) do resultado. Isso garante que o valor do checksum caiba em 1 byte (uint8_t), conforme o trecho abaixo:

``` 
// Função para calcular Checksum (Soma Simples de 8-bit)
uint8_t calculate_checksum(const char *data, size_t len)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum += (uint8_t)data[i]; // Soma cumulativa do valor dos bytes
    }
    return checksum; // Retorna apenas o LSB (8-bit) da soma
}
```

   - Antes de enviar a mensagem, a função de transmissão calcula o checksum sobre o payload e então envia o cabeçalho (Tamanho em 2 bytes e o Checksum de 1 byte), seguido pelo payload.
	
```
void send_packet(char *payload)
{
    size_t len = strlen(payload);
    uint8_t checksum;
    
    // Calcula o checksum sobre toda a string
    checksum = calculate_checksum(payload, len); 

    // O header (Tamanho e Checksum) será enviado como bytes brutos
    // Tamanho (2 bytes: MSB, LSB)
    uart_poll_out(uart_dev, (uint8_t)((len >> 8) & 0xFF)); // MSB
    uart_poll_out(uart_dev, (uint8_t)(len & 0xFF));        // LSB

    // Checksum (1 byte)
    uart_poll_out(uart_dev, checksum);

    // Payload (N bytes)
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, payload[i]);
    }
}
```

   - Na recepção (dentro da serial_cb, que é executada na Interrupção da UART), o código utiliza uma Máquina de Estados (g_rx_state) para processar sequencialmente os bytes recebidos:
	  - WAIT_SIZE_MSB / WAIT_SIZE_LSB: Lê os 2 bytes e monta o tamanho (g_expected_size) do payload esperado.
	  - WAIT_CHECKSUM: Lê o valor do checksum esperado (g_expected_checksum).
	  - RECEIVING_PAYLOAD: Recebe os bytes do payload no buffer rx_buf.
   - Ao receber o número total de bytes do payload (rx_buf_pos == g_expected_size), a rotina de interrupção calcula o checksum localmente e o compara com o valor recebido no cabeçalho.
   - Se o checksum falhar, uma mensagem especial "CS_ERR" é enviada para a fila de mensagens (uart_msgq). A thread de recepção (receive_thread) detecta essa mensagem de erro e invoca a função signal_integrity_error_visual() para sinalizar o problema visualmente (piscando o LED vermelho), descartando o payload corrompido. 
