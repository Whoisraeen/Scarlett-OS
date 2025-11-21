/**
 * Hello World - ScarlettOS Sample Application
 * 
 * Demonstrates basic ScarlettOS application structure
 */

#include <scarlettos.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Hello, ScarlettOS!\n");
    printf("Process ID: %u\n", sys_getpid());
    printf("Thread ID: %u\n", sys_gettid());
    
    // Demonstrate IPC
    port_t port;
    if (ipc_create_port(&port) == ERR_SUCCESS) {
        printf("Created IPC port: %u\n", port);
        ipc_destroy_port(port);
    }
    
    return 0;
}
