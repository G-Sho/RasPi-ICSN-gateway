#include <iostream>
#include <csignal>
#include <fstream>
#include <memory>
#include "main_controller.h"

std::unique_ptr<MainController> g_controller;

void signalHandler(int signum) {
    // std::cout << "\nInterrupt signal (" << signum << ") received." << std::endl;

    if (g_controller) {
        g_controller->shutdown();
    }

    exit(signum);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string uart_device = "/dev/serial0";
    int baudrate = 115200;
    std::string fib_config_path = "";

    if (argc >= 2) {
        uart_device = argv[1];
    }

    if (argc >= 3) {
        baudrate = std::stoi(argv[2]);
    }

    if (argc >= 4) {
        fib_config_path = argv[3];
    } else {
        // 第4引数が省略された場合はデフォルト候補を順に探す
        // build/ ディレクトリから実行する場合: ../config/test_fib.conf
        // リポジトリルートから実行する場合:    config/test_fib.conf
        const char* default_paths[] = {
            "../config/test_fib.conf",
            "config/test_fib.conf",
            nullptr
        };
        for (int i = 0; default_paths[i] != nullptr; i++) {
            std::ifstream f(default_paths[i]);
            if (f.good()) {
                fib_config_path = default_paths[i];
                break;
            }
        }
    }

    // std::cout << "=== Raspberry Pi CEFORE Gateway ===" << std::endl;
    // std::cout << "UART Device: " << uart_device << std::endl;
    // std::cout << "Baudrate: " << baudrate << std::endl;
    // if (!fib_config_path.empty()) {
    //     std::cout << "FIB Config: " << fib_config_path << std::endl;
    // } else {
    //     std::cout << "[WARN] No FIB config file found. Static routes will not be loaded." << std::endl;
    // }
    // std::cout << "===================================" << std::endl;

    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create and initialize controller
    g_controller = std::make_unique<MainController>();

    if (!g_controller->initialize(uart_device, baudrate, fib_config_path)) {
        // std::cerr << "Initialization failed" << std::endl;
        return 1;
    }

    // Run main loop
    g_controller->run();

    return 0;
}
