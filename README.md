# TempLightFlow

O **TempLightFlow** é um projeto desenvolvido para ESP32 utilizando ESP-IDF. O objetivo é ler dados do sensor de temperatura e umidade DHT11 e controlar LEDs RGB com base nos dados recebidos. Além disso, o projeto inclui testes unitários utilizando a biblioteca Criterion.h e automatização de build e entrega utilizando GitHub Actions.

## Funcionalidades

- Leitura de temperatura e umidade do sensor DHT11.
- Controle de LEDs RGB com base na leitura de temperatura.
- Testes automatizados utilizando a biblioteca **Criterion.h**.
- Automação de build, testes e entrega utilizando GitHub Actions.

## Requisitos

- ESP32 (Franzininho Wifi Lab).
- Sensor DHT11.
- LEDs RGB.
- Ambiente de desenvolvimento com ESP-IDF configurado.

## Workflow do GitHub Actions

O workflow do GitHub Actions, denominado `TempLightFlow`, é composto por três jobs:

1. **Build**:
    - Compila o projeto utilizando o ESP-IDF.
    - Armazena o artefato gerado no processo de compilação.
   
2. **Test**:
    - Compila e executa os testes unitários.
    - Publica os resultados dos testes no formato XML.
   
3. **Delivery**:
    - Baixa os artefatos gerados.
    - Cria uma nova release com o artefato compilado.

