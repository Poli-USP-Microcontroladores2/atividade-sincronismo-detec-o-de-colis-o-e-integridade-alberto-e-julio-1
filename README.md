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

Reflita inicialmente se vocês consideram o sincronismo feito por botão algo perfeito, ou se ele pode falhar.
_Será que é necessário fazer um sincronismo periódico?_

Nos casos em que há problemas de sincronismo, podemos ter o cenário de colisão: quando as duas placas tentam transmitir ao mesmo tempo.
Para lidar com este problema, a proposta é elaborar uma detecção de colisão: logo antes de transmitir a mensagem completa, ou após transmitir cada caractere, podemos ouvir o canal (modo de recepção) para verificar se não há alguém já transmitindo, e não iniciar a transmissão caso o canal de comunicação esteja ocupado.

_Elabore um diagrama de transição de estados (versão 2) para modelar como as duas placas irão interagir com o sincronismo por botão e a detecção de colisão, considerando os diversos estados possíveis e os eventos que determinam as transições de estados (vocês podem utilizar o D2 diagrams visto em atividade anterior: https://play.d2lang.com/)_.

_Descreva um teste para verificação de correto funcionamento do sistema considerando este requisito de detecção de colisão, contemplando pré-condição, etapas do teste e pós-condição, de forma similar ao realizado em atividades anteriores (Dica: é possível mapear os estados mais relevantes a comportamentos do led da placa para observar o seu funcionamento?)_.

---

## 1.4. Verificação de Integridade

Reflita inicialmente o que ocorre com as mensagens transmitidas e recebidas em caso de colisão.

Nos casos em que há problemas de colisão, as mensagens podem não ser recebidas de forma completa.
Para lidar com este problema, a proposta é elaborar uma verificação de integridade: no início da mensagem, podemos enviar um hash da mensagem ou pelo menos o tamanho total da mansagem em caracteres, para que o receptor possa verificar se recebeu todos os caracteres de forma íntegra.
Questão para reflexão: _a verificação de integridade de conteúdo é suportada pela verificação de tamanho da mensagem recebida em caracteres?_

_Elabore um diagrama de transição de estados (versão 3) para modelar como as duas placas irão interagir com o sincronismo por botão, a detecção de colisão e a verificação de integridade, considerando os diversos estados possíveis e os eventos que determinam as transições de estados (vocês podem utilizar o D2 diagrams visto em atividade anterior: https://play.d2lang.com/)_.

_Descreva um teste para verificação de correto funcionamento do sistema considerando este requisito de verificação de integridade, contemplando pré-condição, etapas do teste e pós-condição, de forma similar ao realizado em atividades anteriores (Dica: podemos mapear a correta verificação de integridade a comportamentos da placa?)_.

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

    
 
## 2.3. Detecção de Colisão

Insira aqui as descrições dos resultados e referencie as fotos e capturas de tela que mostram o funcionamento.

## 2.4. Verificação de Integridade

Insira aqui as descrições dos resultados e referencie as fotos e capturas de tela que mostram o funcionamento.
