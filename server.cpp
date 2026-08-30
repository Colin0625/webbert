#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

int main() {
  const std::string IP = "127.0.0.1";
  const int PORT = 5000;

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  inet_pton(AF_INET, IP.data(), &addr.sin_addr);
  
  int success = bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

  std::cout << "Listening on " << IP << ":" << PORT << "..." << std::endl;
  listen(fd, SOMAXCONN);

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);

  int client_fd = accept(
    fd,
    reinterpret_cast<sockaddr*>(&client_addr),
    &client_len
  );

  std::cout << "Client connected!" << std::endl;

  char buffer[4096];

  ssize_t s = recv(client_fd, buffer, 4096, 0);
  std::cout << "Received messgae from the client: " << std::endl;
  for (int i{}; i < s; i++) std::cout << buffer[i];
  std::cout << std::endl;


  send(client_fd, buffer, s, 0);
  std::cout << "Sent response message to the client" << std::endl;

  return 0;
}