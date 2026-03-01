# 💊 DOSE CERTA - Organizador de Remédios Inteligente

O **Dose Certa** é um dispensador de medicamentos inteligente baseado em Internet das Coisas (IoT). Desenvolvido com o microcontrolador ESP8266 (NodeMCU), o projeto visa auxiliar pacientes no controle rigoroso de seus tratamentos médicos, garantindo que o medicamento correto seja tomado na hora certa.

O sistema possui 4 compartimentos físicos identificados por LEDs, alertas sonoros e visuais (Display LCD), além de uma interface web responsiva para gerenciamento e integração de notificações em tempo real via Telegram.

---

## 🚀 Principais Funcionalidades

* **Gestão Web Independente:** Painel de controle acessível via navegador (celular ou PC) hospedado no próprio dispositivo.
* **Sistema Multi-Horários:** Cadastro de várias doses para o mesmo medicamento de forma simplificada (formato 24h).
* **Indicação Física (4 Compartimentos):** LEDs indicam visualmente qual caixa o paciente deve abrir.
* **Alertas Multicanal:** * Sonoro (Buzzer integrado).
  * Visual (Display LCD 16x2 e LEDs).
  * Remoto (Notificações no Telegram).
* **Configuração de Wi-Fi Inteligente:** Portal cativo interno para conectar o dispositivo a qualquer rede sem precisar alterar o código.
* **Relógio Sincronizado (NTP):** Horário atualizado automaticamente via internet.

---

## ⚙️ Tecnologias Utilizadas

* **Hardware:** ESP8266 (NodeMCU), Display LCD 16x2 I2C, Buzzer, LEDs, Push-button.
* **Firmware:** C++ (Arduino IDE) com `ESPAsyncWebServer` e sistema de arquivos `LittleFS`.
* **Frontend:** HTML5, CSS3, JavaScript (Single Page Application - Dark Mode).

---

## 📖 Manual do Usuário (Modo de Uso)

Siga os passos abaixo para configurar e utilizar o **Dose Certa** no seu dia a dia.

### Passo 1: Ligando o Dispositivo
1. Conecte o dispositivo à energia (cabo USB / fonte de 5V ou pilhas, conforme a montagem).
2. O Display LCD acenderá exibindo a mensagem "Iniciando...".

### Passo 2: Configurando o Wi-Fi (Primeiro Uso)
Como o dispositivo ainda não conhece a sua rede de internet, ele criará a própria rede.
1. No seu celular ou computador, busque pelas redes Wi-Fi disponíveis e conecte-se à rede chamada **`Dispensador-TCC`** (Senha: `12345678`).
2. Abra o navegador e acesse o endereço IP exibido no painel (geralmente `192.168.4.1` ou clique na opção "Configurar Rede" do seu celular).
3. Na tela de configuração, digite o nome (SSID) e a senha do Wi-Fi da sua casa.
4. Clique em **Salvar e Conectar**. O dispositivo vai reiniciar e se conectar à sua internet. O Display LCD mostrará o novo endereço IP.

### Passo 3: Acessando o Painel de Controle
1. Volte o seu celular para o Wi-Fi da sua casa.
2. Abra o navegador e digite o **IP** que está aparecendo no Display LCD do Dose Certa (ex: `192.168.1.15`).
3. Você verá a tela inicial com o relógio sincronizado e o menu de Medicamentos.

### Passo 4: Cadastrando um Medicamento
1. Na aba **💊 Medicamentos**, digite o Nome do Remédio (ex: *Dipirona*) e a Dose (ex: *1 cp*).
2. Em **Horários**, adicione as horas exatas em que deve tomar o remédio (ex: *08h00*, *16h00*). Você pode clicar em "+ Adicionar outro horário" para múltiplas doses diárias.
3. No campo **Onde está guardado?**, escolha fisicamente em qual compartimento da caixa (1, 2, 3 ou 4) você vai colocar esse remédio.
4. Marque os **Dias da Semana** e os minutos de aviso prévio.
5. Clique em **Salvar Agendamento**. Guarde as pílulas na caixa física correspondente.

### Passo 5: Na Hora do Remédio
Quando o relógio atingir o horário programado:
1. O **Buzzer** começará a apitar.
2. O **LED** do compartimento correto vai piscar.
3. O **Display LCD** mostrará a mensagem: `PEGAR COMPARTIM. [X]` e o nome do remédio.
4. Você (ou o responsável) receberá uma mensagem no **Telegram**.
5. Retire o remédio da caixa iluminada e aperte o **Botão de Parar** no dispositivo para confirmar que a dose foi tomada e silenciar o alarme.

---

## 📱 Configurando as Notificações do Telegram

Para receber alertas no seu celular na hora exata do remédio, o sistema utiliza a API do CallMeBot. Siga os passos atentamente:

1. **Ative o Bot:** Abra o seu aplicativo do Telegram, busque pelo contato `@CallMeBot_API` e envie uma mensagem para ele para iniciar a conversa e autorizar o recebimento de mensagens automáticas.
2. **Solução de Problemas (Login):** Se o bot não enviar mensagens ou der erro de autorização, acesse o site oficial [https://api2.callmebot.com/txt/login.php](https://api2.callmebot.com/txt/login.php) e faça o login com o seu número do Telegram para liberar o acesso da API.
3. **Configuração no Dose Certa:** Acesse o painel web do dispositivo e vá até a aba **⚙️ Configurações**.
4. No campo "Seu Usuário", digite o seu nome de usuário do Telegram (seu `@username` sem o `@`).
5. Clique em **Salvar Config**.
6. Clique no botão **Testar Envio** para confirmar se a mensagem de teste chega perfeitamente no seu aplicativo.
