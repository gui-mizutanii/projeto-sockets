# Servidor HTTP em C++ com Winsock

Este projeto implementa um servidor web básico utilizando a API Winsock em C++. Ele é capaz de receber conexões de múltiplos clientes simultaneamente (através de threads) e responder a requisições HTTP do tipo `GET`.

## 💡 Objetivo

Aplicar conceitos fundamentais de redes de computadores como:
- Programação com sockets em ambiente Windows (Winsock)
- Comunicação cliente-servidor
- Tratamento de requisições HTTP
- Uso de múltiplas threads para lidar com conexões simultâneas

## 📁 Estrutura do Projeto

- `Servidor.cpp`: Lógica principal do servidor (inicialização do socket, tratamento de requisições, resposta HTTP).
- `Cliente.cpp`: Arquivo para testes (vazio ou incompleto).
- `requisicao.html`: Arquivo HTML que o servidor busca e retorna como resposta à requisição.

## ⚙️ Como Executar

1. **Requisitos**:
   - Sistema Windows
   - Compilador compatível com Winsock (ex: MinGW ou MSVC)

2. **Compilação** (exemplo com MinGW):
   ```bash
   g++ Servidor.cpp -o servidor.exe -lws2_32
