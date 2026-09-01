#include <queue>
#include <unordered_map>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string>
#include <thread>

enum class RequestType : uint8_t {
  GET = 0,
  POST = 1,
  DELETE = 2
};

struct HttpRequest {
  std::string_view path;
  std::string_view version;
  RequestType type;
};

struct RequestDescription {
  HttpRequest request;
  int connection_id;
};

struct ConnectionInfo {
  int fd;
  sockaddr_in addr;
  socklen_t addr_len;
};

class Server {
private:
  std::queue<ConnectionInfo> request_queue_;
  std::unordered_map<int, ConnectionInfo> connections_;
  std::string ip_addr_;
  int port_;
  int server_fd_;
  std::vector<std::jthread> threads;
  std::mutex mt;
  
public:
  Server(const std::string& ip, int port);
  bool Start();
  void Work();
};