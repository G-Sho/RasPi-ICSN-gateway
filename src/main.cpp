#include <iostream>
#include <csignal>
#include <memory>
#include "main_controller.h"

std::unique_ptr<MainController> g_controller;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received." << std::endl;

    if (g_controller) {
        g_controller->shutdown();
    }

    exit(signum);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string uart_device = "/dev/serial0";
    int baudrate = 115200;

    if (argc >= 2) {
        uart_device = argv[1];
    }

    if (argc >= 3) {
        baudrate = std::stoi(argv[2]);
    }

    std::cout << "=== Raspberry Pi CEFORE Gateway ===" << std::endl;
    std::cout << "UART Device: " << uart_device << std::endl;
    std::cout << "Baudrate: " << baudrate << std::endl;
    std::cout << "===================================" << std::endl;

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create and initialize controller
    g_controller = std::make_unique<MainController>();

    if (!g_controller->initialize(uart_device, baudrate)) {
        std::cerr << "Initialization failed" << std::endl;
        return 1;
    }

    // Run main loop
    g_controller->run();

    return 0;
}
