/**
 * @file service_template.c
 * @brief ScarlettOS Service Template
 * 
 * This is a template for creating system services for ScarlettOS.
 * Copy this file and modify it for your specific service.
 */

#include <scarlettos.h>
#include <stdio.h>
#include <stdbool.h>

// Service metadata
#define SERVICE_NAME "example_service"
#define SERVICE_VERSION "1.0.0"
#define SERVICE_AUTHOR "Your Name"

// Service configuration
#define MAX_CLIENTS 32
#define REQUEST_QUEUE_SIZE 128

// Message types
#define MSG_CONNECT 1
#define MSG_DISCONNECT 2
#define MSG_REQUEST 3
#define MSG_RESPONSE 4

// Service state
typedef struct {
    port_t service_port;
    uint32_t num_clients;
    pid_t clients[MAX_CLIENTS];
    bool running;
} service_state_t;

static service_state_t g_service;

/**
 * Initialize the service
 */
static int service_init(void) {
    printf("%s: Initializing service\n", SERVICE_NAME);
    
    // Create IPC port for service
    if (ipc_create_port(&g_service.service_port) != ERR_SUCCESS) {
        fprintf(stderr, "Failed to create service port\n");
        return -1;
    }
    
    printf("%s: Service port: %u\n", SERVICE_NAME, g_service.service_port);
    
    g_service.num_clients = 0;
    g_service.running = true;
    
    // TODO: Initialize your service-specific resources
    // - Allocate memory
    // - Open files
    // - Connect to other services
    
    return 0;
}

/**
 * Cleanup service resources
 */
static void service_cleanup(void) {
    printf("%s: Cleaning up service\n", SERVICE_NAME);
    
    // TODO: Cleanup service-specific resources
    
    // Destroy IPC port
    ipc_destroy_port(g_service.service_port);
    
    g_service.running = false;
}

/**
 * Handle client connection
 */
static void handle_connect(ipc_msg_t* msg) {
    if (g_service.num_clients >= MAX_CLIENTS) {
        fprintf(stderr, "%s: Too many clients\n", SERVICE_NAME);
        // Send error response
        return;
    }
    
    g_service.clients[g_service.num_clients++] = msg->sender;
    printf("%s: Client %u connected (%u total)\n", 
           SERVICE_NAME, msg->sender, g_service.num_clients);
    
    // Send success response
    ipc_reply(msg->id, "OK", 3);
}

/**
 * Handle client disconnection
 */
static void handle_disconnect(ipc_msg_t* msg) {
    // Find and remove client
    for (uint32_t i = 0; i < g_service.num_clients; i++) {
        if (g_service.clients[i] == msg->sender) {
            // Shift remaining clients
            for (uint32_t j = i; j < g_service.num_clients - 1; j++) {
                g_service.clients[j] = g_service.clients[j + 1];
            }
            g_service.num_clients--;
            printf("%s: Client %u disconnected (%u remaining)\n",
                   SERVICE_NAME, msg->sender, g_service.num_clients);
            break;
        }
    }
}

/**
 * Handle client request
 */
static void handle_request(ipc_msg_t* msg) {
    printf("%s: Request from client %u, size=%u\n",
           SERVICE_NAME, msg->sender, msg->size);
    
    // TODO: Process the request
    // - Parse request data
    // - Perform operation
    // - Generate response
    
    // Example: Echo the request back
    ipc_reply(msg->id, msg->data, msg->size);
}

/**
 * Main service loop
 */
static void service_loop(void) {
    ipc_msg_t msg;
    
    printf("%s: Entering service loop\n", SERVICE_NAME);
    
    while (g_service.running) {
        // Wait for incoming message
        int ret = ipc_recv(g_service.service_port, &msg, 1000); // 1 second timeout
        
        if (ret != ERR_SUCCESS) {
            if (ret == ERR_TIMEOUT) {
                // Timeout - check if we should continue
                continue;
            } else {
                fprintf(stderr, "%s: IPC receive error: %d\n", SERVICE_NAME, ret);
                break;
            }
        }
        
        // Dispatch message based on type
        uint32_t msg_type = *(uint32_t*)msg.data;
        
        switch (msg_type) {
            case MSG_CONNECT:
                handle_connect(&msg);
                break;
                
            case MSG_DISCONNECT:
                handle_disconnect(&msg);
                break;
                
            case MSG_REQUEST:
                handle_request(&msg);
                break;
                
            default:
                fprintf(stderr, "%s: Unknown message type: %u\n", 
                        SERVICE_NAME, msg_type);
                break;
        }
    }
    
    printf("%s: Exiting service loop\n", SERVICE_NAME);
}

/**
 * Service entry point
 */
int main(int argc, char** argv) {
    printf("%s v%s by %s\n", SERVICE_NAME, SERVICE_VERSION, SERVICE_AUTHOR);
    
    // Initialize service
    if (service_init() != 0) {
        fprintf(stderr, "Failed to initialize service\n");
        return 1;
    }
    
    // Run service loop
    service_loop();
    
    // Cleanup
    service_cleanup();
    
    return 0;
}
