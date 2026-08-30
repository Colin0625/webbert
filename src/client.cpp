#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

int main() {
  const std::string IP = "127.0.0.1";
  const int PORT = 5000;
  
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in server{};
  server.sin_family = AF_INET;
  server.sin_port = htons(PORT);
  inet_pton(AF_INET, IP.data(), &server.sin_addr);

  std::cout << "Connecting to server on " << IP << ":" << PORT << "..." << std::endl;
  connect(fd, reinterpret_cast<sockaddr*>(&server), sizeof(server));
  std::cout << "Connected to server!" << std::endl;
  std::cout << "Enter a message: ";

  std::string msg;
  std::getline(std::cin, msg);

  char buffer[4096];

  send(fd, msg.data(), msg.size(), 0);
  std::cout << "Sent " << msg.size() << " bytes to the server" << std::endl;
  ssize_t s = recv(fd, buffer, 4096, 0);

  for (int i{}; i < s; i++) std::cout << buffer[i];
  std::cout << std::endl;

  return 0;
}
