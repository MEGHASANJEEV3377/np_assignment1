#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <math.h>
#include <netdb.h> // For address resolution using getaddrinfo()

// Uncomment for debugging output
// #define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG_LOG(x) printf x
#else
#define DEBUG_LOG(x)
#endif

// Function to process the operation and compute the result
void process_operation(const char *operation, const char *arg1, const char *arg2, char *output_result) {
    double operand1, operand2, result_value;

    // Check if the operation involves floating-point numbers
    if (operation[0] == 'f') {  
        operand1 = atof(arg1);  // Convert the first operand to floating-point
        operand2 = atof(arg2);  // Convert the second operand to floating-point
    } else {  
        operand1 = atoi(arg1);  // Convert the first operand to integer
        operand2 = atoi(arg2);  // Convert the second operand to integer
    }

    // Compute based on the operation type
    if (strcmp(operation, "add") == 0) {
        result_value = operand1 + operand2;
        sprintf(output_result, "%d\n", (int)result_value);
    } else if (strcmp(operation, "sub") == 0) {
        result_value = operand1 - operand2;
        sprintf(output_result, "%d\n", (int)result_value);
    } else if (strcmp(operation, "mul") == 0) {
        result_value = operand1 * operand2;
        sprintf(output_result, "%d\n", (int)result_value);
    } else if (strcmp(operation, "div") == 0) {
        if (operand2 == 0) {
            sprintf(output_result, "ERROR\n"); // Division by zero for integers
        } else {
            result_value = operand1 / operand2;
            sprintf(output_result, "%d\n", (int)result_value);
        }
    } else if (strcmp(operation, "fadd") == 0) {
        result_value = operand1 + operand2;
        sprintf(output_result, "%8.8g\n", result_value);
    } else if (strcmp(operation, "fsub") == 0) {
        result_value = operand1 - operand2;
        sprintf(output_result, "%8.8g\n", result_value);
    } else if (strcmp(operation, "fmul") == 0) {
        result_value = operand1 * operand2;
        sprintf(output_result, "%8.8g\n", result_value);
    } else if (strcmp(operation, "fdiv") == 0) {
        if (fabs(operand2) < 0.0001) {  // Division by near-zero for floating-point
            sprintf(output_result, "ERROR\n");
        } else {
            result_value = operand1 / operand2;
            sprintf(output_result, "%8.8g\n", result_value);
        }
    } else {
        sprintf(output_result, "ERROR\n");  // Unknown operation
    }
}

// Function to check and validate the protocol received from the server
int check_protocol(const char *protocol_response) {
    size_t protocol_length = strlen(protocol_response);

    // Ensure the protocol response ends with two newlines
    if (protocol_length < 2 || strcmp(&protocol_response[protocol_length - 2], "\n\n") != 0) {
        printf("ERROR: Invalid protocol format.\n");
        return 0;
    }

    char *temp_response = strdup(protocol_response);
    char *line = strtok(temp_response, "\n");
    int is_protocol_supported = 0;

    // Check each line for supported protocol versions
    while (line != NULL) {
        if (strncmp(line, "TEXT TCP", 8) == 0) {
            is_protocol_supported = 1;  // Mark as valid if a supported protocol is found
        }
        line = strtok(NULL, "\n");
    }

    free(temp_response);

    if (!is_protocol_supported) {
        printf("ERROR: No supported protocol found.\n");
        return 0;
    }
    return 1;  // Protocol is valid
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <host:port>\n", argv[0]);
        return 1;
    }

    // Split the input into hostname and port
    const char *delimiter = ":"; // Updated to `const char*` to avoid warnings
    char *hostname = strtok(argv[1], delimiter);
    char *port_string = strtok(NULL, delimiter);

    if (!hostname || !port_string) {
        fprintf(stderr, "ERROR: Invalid host:port format.\n");
        return 1;
    }

    int server_port = atoi(port_string);
    if (server_port <= 0) {
        fprintf(stderr, "ERROR: Invalid port value.\n");
        return 1;
    }

    printf("Attempting to connect to %s:%d\n", hostname, server_port);

    struct addrinfo address_hint = {0}, *resolved_addresses, *current_address;
    address_hint.ai_family = AF_UNSPEC;  // Support both IPv4 and IPv6
    address_hint.ai_socktype = SOCK_STREAM;  // Use TCP

    int addr_info_status = getaddrinfo(hostname, port_string, &address_hint, &resolved_addresses);
    if (addr_info_status != 0) {
        fprintf(stderr, "ERROR: %s\n", gai_strerror(addr_info_status));
        return 1;
    }

    int client_socket = -1;
    for (current_address = resolved_addresses; current_address != NULL; current_address = current_address->ai_next) {
        client_socket = socket(current_address->ai_family, current_address->ai_socktype, current_address->ai_protocol);
        if (client_socket < 0) continue;

        if (connect(client_socket, current_address->ai_addr, current_address->ai_addrlen) == 0) {
            break;  // Connection successful
        }
        close(client_socket);
    }

    freeaddrinfo(resolved_addresses);

    if (current_address == NULL) {
        fprintf(stderr, "ERROR: Connection to the server failed.\n");
        return 1;
    }

    char server_message[1024];
    ssize_t bytes_received = read(client_socket, server_message, sizeof(server_message) - 1);
    if (bytes_received <= 0) {
        fprintf(stderr, "ERROR: Failed to receive protocol information.\n");
        close(client_socket);
        return 1;
    }
    server_message[bytes_received] = '\0';

    if (!check_protocol(server_message)) {
        close(client_socket);
        return 1;
    }

    const char *confirmation_message = "OK\n";
    write(client_socket, confirmation_message, strlen(confirmation_message));

    bytes_received = read(client_socket, server_message, sizeof(server_message) - 1);
    server_message[bytes_received] = '\0';

    printf("RECEIVED TASK: %s", server_message);

    char operation[16], operand1[32], operand2[32];
    sscanf(server_message, "%s %s %s", operation, operand1, operand2);

    char result_buffer[64];
    process_operation(operation, operand1, operand2, result_buffer);

    write(client_socket, result_buffer, strlen(result_buffer));

#ifdef DEBUG_MODE
    printf("DEBUG: Sent result: %s", result_buffer);
#endif

    bytes_received = read(client_socket, server_message, sizeof(server_message) - 1);
    server_message[bytes_received] = '\0';

    result_buffer[strcspn(result_buffer, "\n")] = '\0';
    printf("SERVER RESPONSE: %s (Result: %s)\n", server_message, result_buffer);

    close(client_socket);
    return 0;
}
