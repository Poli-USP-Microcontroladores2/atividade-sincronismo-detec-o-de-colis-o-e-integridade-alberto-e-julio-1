# PSI-Microcontroladores2-Aula07
Atividade: Semáforos de Pedestres e Veículos

## Objetivo
- Desenvolver um sistema embarcado de controle de semáforos para pedestres e veículos, utilizando threads e mutex, e validar o funcionamento do código por meio de testes utilizando o modelo V (testes unitários, de integração e de sistema).
- Opcionalmente, alunos podem utilizar IA generativa para auxiliar na elaboração de trechos de código ou na geração de planos de teste, mas a avaliação deve se concentrar na qualidade dos testes e na correta validação do sistema.

---
# Relatório da Equipe

### Integrantes:
- Júlio Cesar Braga Parro - 16879560
- Arthur J. C. Brasil - 16855560

---

## Código Júlio - Semáforo de Pedestres:
### Testes Unitários
- Implementou-se um semáforo de pedestres com o controle de duas leds (verde e vermelho) por duas threads independentes distintas, thread_led_verde e thread_led_vermelho, com acionamento alternado controlado por mutex, para garantir os tempos de acionamento especificados na atividade (4 segundos para o verde e 4 segundos para o vermelho).
- No código, foram implementados dois modos de funcionamento distintos, noturno e diurno, controlados por uma variável global, em que o modo noturno faz com que apenas o led vermelho pisque, com 1 segundo ligado e 1 segundo desligado. 
- Além disso, implementou-se um botão de travessia, no pino PTA16, para que, assim que for acionado, mantenha o led vermelho por cerca de 1 segundo, equivalente ao período amarelo do semáforo de carros e, em seguida, ligue o led verde por 4 segundos, enquanto desliga o led vermelho.
- Para correto funcionamento do botão de travessia, adicionou uma função que faz um sleep_time inteligente (smart_sleep), verificando se houve pedido de travessia a cada 50 ms, uma vez que o mutex só pode ser liberado por quem o ocupou.
- A partir do acionamento do botão de travessia, manda-se um sinal pelo pino PTE20, configurado como output, que será recebido pelo outro microcontrolador, para garantir sincronia no modo de travessia. A análise desse comportamento será feita posteriormente.
- Fez-se a implementação de um sinal de sincronização, por meio de um botão implementado no pino PTA17, que manda um sinal pelo pino PTE21, configurado como output, que será recebido pelo outro microcontrolador, de modo a garantir sincronia inicial das atividades.
- Até o botão de sincronização ser pressionado, as duas threads do meu código ficam em modo de espera e começam a executar o loop somente após serem sincronizadas. Tal funcionalidade é implementada por um semáforo contador de sincronização.
- Com base na implementação desses requisitos, especificados pelo enunciado da atividade, realizou-se um planejamento e execução de diferentes testes unitários, para garantir o funcionamento do semáforo de pedestres isoladamente, conforme descrito abaixo:
- Caso de teste 1: Ciclo dos LEDs Verde e Vermelho
  - Pré-condição: modo_noturno = false e pedido_travessia = false. Mutex inicializado e threads ativas.
  - Etapas de teste: Compilar e executar o código original. Observar a sequência de acionamento dos LEDs.
  - Pós-condição observada: LED verde acende por cerca de 4 s e apaga; em seguida, LED vermelho acende por cerca de 4 s. Nenhum LED fica aceso simultaneamente.
  - Conclusão: A sequência e os tempos de cada LED estão corretos, indicando que as threads e o mutex funcionam conforme esperado.
- Caso de teste 2: Teste do modo noturno (comportamento isolado)
  - Pré-condição: modo_noturno = true. Nenhum pedido de travessia ativo (O botão não afeta esse modo).
  - Etapas de teste: Executar o código em modo noturno. Observar a piscagem dos LEDs.
  - Pós-condição observada: LED vermelho do pedestre pisca com ciclo de 2 segundos (1 s aceso, 1 s apagado). Nenhum outro LED interfere.
  - Conclusão: Comportamento do modo noturno é correto e o mutex garante exclusão mútua entre os LEDs.
- Caso de teste 3: Teste do botão de travessia (debounce e ação)
  - Pré-condição: modo_noturno = false. Sistema em operação normal.
  - Etapas de teste: Pressionar o botão de pedestre (fisicamente ou via simulação). Observar os logs e LEDs.
  - Pós-condição observada: LED verde acende após cerca de 1 segundo, que é o tempo de luz amarela do semáforo de carro, por cerca de 4 segundos (acaba sendo por 4,01 segundos, pois entra na rotina normal por 10 ms), PTE20 é ativado durante esse tempo e pedido_travessia volta a false ao final. Pressões rápidas não geram múltiplos pedidos.
  - Conclusão: O botão de travessia é detectado corretamente, o debounce funciona e o sistema retorna ao estado normal após a travessia

#### Evidências
  - As evidências da execução de cada teste estão registradas abaixo, por meio de printscreens dos logs impressos no serial monitor.

Caso de teste 1: Ciclos dos LEDs

<img width="750" height="500" alt="Captura de tela 2025-11-02 164004" src="https://github.com/user-attachments/assets/79c3f29d-c7d1-4e5e-b018-b06709ebb8b4" />


Caso de teste 2: Modo noturno

<img width="750" height="500" alt="Captura de tela 2025-11-02 164829" src="https://github.com/user-attachments/assets/a2ba3a34-c94f-4cc9-80e2-20c27530b1b8" />


Caso de teste 3: Botão de travessia

<img width="750" height="500" alt="Captura de tela 2025-11-02 163553" src="https://github.com/user-attachments/assets/481714cb-b93e-4974-ac8a-b63c69731dfc" />


### Testes de Integração
- Após garantir que as funcionalidades do código individual estão corretamente implementadas, trabalhou-se na integração dos dois códigos. Todos os botões e sinais são enviados do microcontrolador com o código do semáforo de pedestres para o microcontrolador com o código do semáforo de carros, que apenas processa os sinais.
- Para sincronização inicial, utiliza-se o botão de sincronização, localizado no pino PTA17 do microcontrolador do semáforo de pedestres, que sincroniza a atividade dos dois semáforos. Ao acioná-lo, emite-se um sinal pelo pino PTE21 do mesmo microcontrolador, que é então processado pelo botão do outro microcontrolador.
- No entanto, caso o reset do microcontrolador do semáforo de pedestres for ativo enquanto o outro está ligado, o microcontrolador do semáforo de carros detecta esse sinal. Tal comportamento não é problema dos códigos, sendo, provavelmente, decorrente da arquitetura dos microcontroladores. Então, deve-se resetar primeiro o microcontrolador dos pedestres, resetar o microcontrolador do semáforo de carros e depois pressionar o botão de sincronização, nessa ordem.
- O botão de travessia está localizado no pino PTA16 do microcontrolador do semáforo de pedestres. Após seu acionamento, emite-se um sinal pelo pino PTE20 deste microcontrolador, que é então processado pelo outro microcontrolador (o semáforo de carros).
- Quando fizemos essa implementação, usando o PTE20, houve um grande atraso no processamento do sinal pelo microcontrolador dos carros (aproximadamente 6 segundos), porém não encontrou-se o motivo, pois testou-se o sinal com um led na protoboard e o sinal era instantâneo. Além disso, quando o sinal era mandado diretamente do botão para os dois microcontroladores, o processamento era instantâneo.
- Portanto, a comunicação do botão do pedestre é direta com os dois microcontroladores, sem ser mediado pelo microcontrolador do pedestre (como ocorre com o botão de sincronização) 
- Para acionamento dos botões, basta conectá-los ao terra do microcontrolador, não esquecendo de compartilhar os terras entre os dois microcontroladores. Neste caso, foi utilizado um pushbutton para tal, conforme registrado abaixo.


- Na imagem, os LEDS não estão conectados, mas testou-se os sinais com eles.

- Com base na implementação desses requisitos, especificados pelo enunciado da atividade, realizou-se um planejamento e execução de diferentes testes de integração, para garantir o funcionamento de ambos os semáforos em conjunto, conforme descrito abaixo:
- Caso de teste 1: Comunicação de Sincronização entre Placas
  - Pré-condição: Dois microcontroladores conectados via pinos PTE20 (semáforo de pedestres) e PTA17 (semáforo de carros).
  - Etapas de teste: Reiniciar ambos e pressionar o botão PTA17 no microcontrolador do semáforo de pedestres, que aciona o PTE20. OBS: Reiniciar o microcontrolador do semáforo de pedestres primeiro e depois o microcontrolador do semáforo de carros.
  - Pós-condição observada: Os semáforos iniciam os ciclos de forma sincronizada; o pulso de sincronização é detectado em ambas as placas.
  - Conclusão: O sistema de sincronismo entre placas funciona corretamente, garantindo alinhamento temporal dos semáforos.
- Caso de teste 2: Sincronismo entre pedestres e veículos
  - Pré-condição: Código compilado com ambos os semáforos (pedestres e veículos). Botão de sincronização já acionado. modo_noturno = false (pedestres) e modo_atual = MODO_NORMAL (carros).
  - Etapas de teste: Executar o sistema completo. Observar os logs e LEDs de ambos os semáforos.
  - Pós-condição observada: Quando o LED verde do pedestre acende, o vermelho dos veículos está aceso. Quando o pedestre está vermelho, os veículos alternam entre verde e amarelo.
  - Conclusão: O sincronismo entre os semáforos é mantido corretamente; não há conflito de LEDs ativos simultaneamente.
- Caso de teste 3: Travessia Acionada (interação botão e semáforo de veículos) 
  - Pré-condição: Sistema em modo normal, ambos semáforos ativos.
  - Etapas de teste: Pressionar o botão de travessia. Observar LEDs e logs nos dois microcontroladores.
  - Pós-condição observada: O pedestre demora 1 segundo em vermelho e depois verde acende por 4 segundos; o semáforo de veículos entra em amarelo por 1 segundo e vermelho durante esse período de 4 segundos. Após a travessia, o ciclo normal é retomado. Múltiplos pressionamentos não afetam o comportamento.
  - Conclusão: A comunicação entre o botão de pedestre e o semáforo de veículos funciona corretamente e garante segurança da travessia.
- A comprovação dos testes feitos foi realizada por meio de um vídeo gravado com o comportamento geral do sistema, tal vídeo está anexado no arquivo .md do repositório. Ele demonstra o comportamento do botão de sincronização, a sincronização dos semáforos e o comportamento do botão de travessia, bem como as conexões feitas. 
- Não se fez a comprovação por logs, pois os logs não iniciam ao mesmo tempo, de modo a tornar difícil a comprovação do sincronismo geral dos dois semáforos apenas por logs, uma vez que a referência temporal 0.000 não é a mesma.


### Testes de sistema
- Garantido a integração dos dois microcontroladores, fez-se testes gerais para validar o comportamento global do sistema nos diferentes modos de operação e sob condições reais.
- O modo noturno não necessita de sincronização de tempo de piscagem dos LEDS, então é controlado por variáveis globais independentes em cada código, mas deve-se apertar o botão de sincronização para começar os códigos. Portanto, basta trocar o estado da variável global de ambos.
Realizou-se um planejamento e execução de diferentes testes de sistema, para garantir o funcionamento global do sistema em situações reais de operação, conforme registrado abaixo: 
- Caso de teste 1: Modo normal de operação (diurno)
  - Pré-condição: modo_noturno = false (pedestres) e modo_atual = MODO_NORMAL (carros). Sistema em execução contínua.
  - Etapas de teste: Observar o ciclo completo dos semáforos (pedestre e veículos).
  - Pós-condição observada: Ciclo correto: veículos (verde → amarelo → vermelho) e pedestres (vermelho → verde → vermelho), com sincronismo e tempos adequados.
  - Conclusão: Sistema cumpre requisitos funcionais do modo normal de operação.
- Caso de teste 2: Modo noturno (piscar de advertência)
  - Pré-condição: Ativar modo_noturno = true (pedestres) e modo_atual = MODO_NOTURNO (carros).
  - Etapas de teste: Observar LEDs e logs durante 10 segundos.
  - Pós-condição observada: LEDs piscam alternadamente a cada 2 segundos (1 segundo ligado, 1 segundo desligado): carro amarelo e pedestre vermelho. Nenhum outro LED interfere.
  - Conclusão: O sistema entra corretamente no modo noturno, garantindo visibilidade e economia de energia.
- Caso de teste 3: Robustez do sistema (múltiplos acionamentos)
  - Pré-condição: Sistema em operação normal. modo_noturno = false.
  - Etapas de teste: Pressionar o botão de travessia repetidas vezes e rapidamente. Observar logs e LEDs.
  - Pós-condição observada: Apenas um pedido de travessia é reconhecido; sistema ignora múltiplos acionamentos dentro do tempo de debounce.
  - Conclusão: O Sistema é robusto contra acionamentos repetidos; mantém estabilidade e segurança operacional.
- O funcionamento geral do sistema pode ser comprovado pelo vídeo anexado anteriormente. Como o modo noturno dos dois semáforos ocorrem de forma independente, controlados apenas por variáveis globais em cada código, não havia necessidade de fazer um vídeo comprovando sincronismo.