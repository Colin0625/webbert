#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string>
#include <string_view>
#include <ranges>
#include <charconv>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>

#include "server.hpp"



int main(int argc, char* argv[]) {
  const std::string IP = "0.0.0.0";
  int port = 5001;

  if (argc > 1) {
    const std::string_view port_arg = argv[1];
    const auto [end, error] = std::from_chars(
      port_arg.data(),
      port_arg.data() + port_arg.size(),
      port
    );

    if (error != std::errc{} || end != port_arg.data() + port_arg.size()
        || port < 1 || port > 65535) {
      std::cerr << "Invalid port: " << port_arg << std::endl;
      return 1;
    }
  }

  Server server(IP, port);

  server.Start();


  return 0;
}
