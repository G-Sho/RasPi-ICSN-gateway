#pragma once

#include <memory>
#include <string>
#include "uart_receiver.h"
#include "packet_parser.h"
#include "cefore_interface.h"
#include "name_mapper.h"
#include "gateway_fib.h"

class MainController {
public:
    MainController();
    ~MainController();

    bool initialize(const std::string& uart_device, int baudrate);
    void run();
    void shutdown();

private:
    void onRxPacket(const RxPacket& packet);
    void onInterest(const std::string& uri, uint32_t chunk_num);

    std::unique_ptr<UARTReceiver> uart_;
    std::unique_ptr<PacketParser> parser_;
    std::unique_ptr<CeforeInterface> cefore_;
    std::unique_ptr<NameMapper> name_mapper_;
    std::unique_ptr<GatewayFIB> fib_;
};
