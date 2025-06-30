#include <winsock2.h> // Principal cabeçalho para Winsock
#include <ws2tcpip.h> // Para funcoes como inet_pton
#include <stdio.h>    // Para printf, perror
#include <stdlib.h>   // Para EXIT_FAILURE, EXIT_SUCCESS
#include <string.h>   // Para strlen, strncmp, strcat, snprintf
#include <time.h>     // Para time() para data HTTP
#include <sys/stat.h> // Para stat() para verificar existencia de arquivo (compatibilidade)

// Necessário para linkar com a biblioteca Winsock no compilador (ex: MinGW)
// #pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 2048
#define REQUIRED_FILE "requisicao.html"

// Estrutura para passar argumentos para a thread do cliente
typedef struct {
    SOCKET client_sock;
    char client_ip[INET_ADDRSTRLEN];
} ClientThreadArgs;

// Função para verificar se um arquivo existe
// No Windows, stat.h é uma alternativa compatível, mas _stat ou GetFileAttributes são mais nativas.
// Usamos stat para manter a familiaridade com o exemplo POSIX, mas é bom saber as alternativas.
int file_exists(const char *filename) {
    struct stat st;
    return stat(filename, &st) == 0; // Retorna 1 se o arquivo existe, 0 caso contrário
}

// Função para obter a data e hora formatada para cabeçalho HTTP
void get_http_date(char *date_str, size_t size) {
    time_t timer;
    struct tm *gmt;
    timer = time(NULL);
    gmt = gmtime(&timer);
    // Formato RFC 1123 para cabeçalhos HTTP
    strftime(date_str, size, "%a, %d %b %Y %H:%M:%S GMT", gmt);
}

// Função que será executada em uma nova thread para cada cliente
DWORD WINAPI handle_client(LPVOID lpParam) {
    ClientThreadArgs *args = (ClientThreadArgs *)lpParam;
    SOCKET client_sock = args->client_sock;
    char *client_ip = args->client_ip; // IP do cliente para logs

    char buffer[BUFFER_SIZE];
    char http_date[64];
    int bytes_received;
    const char *status_line;
    const char *content_type = "text/html; charset=utf-8";
    char *response_body;
    char response_header[BUFFER_SIZE];
    int response_body_len;

    // Recebe a requisição HTTP do cliente
    bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == SOCKET_ERROR) {
        fprintf(stderr, "Erro ao receber dados do cliente %s: %d\n", client_ip, WSAGetLastError());
        closesocket(client_sock); // Fecha o socket
        free(args); // Libera a memória alocada para os argumentos da thread
        return 1;
    }
    if (bytes_received == 0) {
        printf("Cliente %s desconectou (conexão fechada).\n", client_ip);
        closesocket(client_sock);
        free(args);
        return 0;
    }

    buffer[bytes_received] = '\0'; // Null-terminate a string recebida
    printf("Requisição recebida de %s:\n%s\n", client_ip, buffer);

    // Verifica se a requisição começa com "GET "
    if (strncmp(buffer, "GET ", 4) == 0) {
        int exists = file_exists(REQUIRED_FILE);
        get_http_date(http_date, sizeof(http_date));

        if (exists) {
            status_line = "HTTP/1.1 200 OK";
            response_body = "<html><body><h1>200 OK:</h1><p>Arquivo '" REQUIRED_FILE "' encontrado!</p></body></html>";
            printf("Arquivo '%s' encontrado. Enviando 200 OK para %s.\n", REQUIRED_FILE, client_ip);
        } else {
            status_line = "HTTP/1.1 404 Not Found";
            response_body = "<html><body><h1>404 Not Found:</h1><p>Arquivo '" REQUIRED_FILE "' NAO encontrado.</p></body></html>";
            printf("Arquivo '%s' NAO encontrado. Enviando 404 Not Found para %s.\n", REQUIRED_FILE, client_ip);
        }

        response_body_len = strlen(response_body);

        // Constrói os cabeçalhos da resposta HTTP
        // Usamos \r\n para quebra de linha em HTTP
        snprintf(response_header, sizeof(response_header),
                 "%s\r\n"
                 "Date: %s\r\n"
                 "Server: MeuServidorWinsock/1.0\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n", // Linha em branco para separar cabeçalhos do corpo
                 status_line, http_date, content_type, response_body_len);

        // Envia cabeçalhos e corpo para o cliente
        send(client_sock, response_header, strlen(response_header), 0);
        send(client_sock, response_body, response_body_len, 0);

    } else {
        // Responde com 405 Method Not Allowed para outros métodos HTTP
        get_http_date(http_date, sizeof(http_date));
        response_body = "<html><body><h1>405 Method Not Allowed:</h1><p>Metodo nao permitido. Apenas GET e suportado.</p></body></html>";
        response_body_len = strlen(response_body);
        
        snprintf(response_header, sizeof(response_header),
                 "HTTP/1.1 405 Method Not Allowed\r\n"
                 "Date: %s\r\n"
                 "Server: MeuServidorWinsock/1.0\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %d\r\n"
                 "Allow: GET\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 http_date, response_body_len);

        send(client_sock, response_header, strlen(response_header), 0);
        send(client_sock, response_body, response_body_len, 0);
    }

    closesocket(client_sock); // Fecha o socket do cliente
    printf("Cliente %s desconectado.\n", client_ip);
    free(args); // Libera a memória alocada para os argumentos da thread
    return 0; // Retorna da thread
}

int main() {
    WSADATA wsaData; // Estrutura para armazenar informações da Winsock
    SOCKET listen_sock; // Socket para escutar por conexões
    SOCKET client_sock; // Socket para a conexão com o cliente
    struct sockaddr_in server_addr, client_addr; // Estruturas de endereço
    int client_addr_len = sizeof(client_addr);
    int opt = 1;

    // 1. Inicializa a Winsock
    // WSAStartup(versao_requerida, ponteiro_para_WSADATA)
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Erro ao inicializar Winsock: %d\n", WSAGetLastError());
        return 1;
    }
    printf("Winsock inicializada com sucesso.\n");

    // 2. Criação do socket: AF_INET para IPv4, SOCK_STREAM para TCP
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "Erro ao criar socket: %d\n", WSAGetLastError());
        WSACleanup(); // Limpa a Winsock antes de sair
        return 1;
    }

    // 3. Define opções do socket: Reuso de endereço para evitar "Address already in use"
    // SO_REUSEADDR permite que o socket seja vinculado a um endereço que está em TIME_WAIT
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        fprintf(stderr, "Erro em setsockopt SO_REUSEADDR: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escuta em todas as interfaces disponíveis
    server_addr.sin_port = htons(PORT);       // Converte porta para ordem de bytes de rede

    // 4. Vincula o socket a um endereço e porta
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Erro ao vincular socket (bind): %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    // 5. Entra em modo de escuta (backlog de 10 conexões pendentes)
    if (listen(listen_sock, 10) == SOCKET_ERROR) {
        fprintf(stderr, "Erro ao escutar (listen): %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    printf("Servidor C HTTP (Winsock) iniciado na porta %d\n", PORT);
    printf("Verificando a existência do arquivo: %s\n", REQUIRED_FILE);

    while (1) {
        // 6. Aceita uma nova conexão de cliente
        client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            fprintf(stderr, "Erro ao aceitar conexão (accept): %d\n", WSAGetLastError());
            continue; // Continua tentando aceitar
        }

        char client_ip[INET_ADDRSTRLEN];
        // Converte o endereço IP do cliente de formato binário para string
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Conexão recebida de %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Aloca memória para os argumentos da thread
        ClientThreadArgs *args = (ClientThreadArgs *)malloc(sizeof(ClientThreadArgs));
        if (args == NULL) {
            fprintf(stderr, "Erro ao alocar memória para argumentos da thread.\n");
            closesocket(client_sock);
            continue;
        }
        args->client_sock = client_sock;
        strcpy(args->client_ip, client_ip); // Copia o IP do cliente

        // 7. Cria uma nova thread para lidar com o cliente (substitui fork())
        // CreateThread(atributos_seguranca, tamanho_pilha, funcao_thread, argumentos, flags, id_thread)
        HANDLE thread_handle = CreateThread(NULL, 0, handle_client, args, 0, NULL);
        if (thread_handle == NULL) {
            fprintf(stderr, "Erro ao criar thread para o cliente: %d\n", GetLastError());
            closesocket(client_sock);
            free(args);
        } else {
            // Fecha o handle da thread imediatamente, pois não precisamos esperar por ela.
            // A thread continuará executando independentemente.
            CloseHandle(thread_handle);
        }
    }

    // Código de limpeza (nunca será alcançado neste loop infinito)
    closesocket(listen_sock);
    WSACleanup(); // Limpa a Winsock
    return 0;
}