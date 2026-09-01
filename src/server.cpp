#include <server.hpp>

#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string>
#include <string_view>
#include <ranges>
#include <charconv>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <optional>
#include <queue>
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <thread>

namespace {

std::string print_ip(const in_addr& address) {
  char buffer[INET_ADDRSTRLEN]{};
  if (inet_ntop(AF_INET, &address, buffer, sizeof(buffer)) == nullptr) {
    return "<invalid IPv4 address>";
  }
  return buffer;
}

HttpRequest parse_request(std::string_view request) {
  HttpRequest res;
  int count{};
  for (const auto word : std::views::split(request, ' ')) {
    std::string_view v(word.begin(), word.end());
    if (count == 0) {
      if (v == "GET") res.type = RequestType::GET;
      else if (v == "POST") res.type = RequestType::POST;
    }
    else if (count == 1) res.path = v;
    else if (count == 2) res.version = v;
    count++;
  }
  return res;
}

bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
  return lhs.size() == rhs.size() &&
    std::ranges::equal(lhs, rhs, [](unsigned char a, unsigned char b) {
      return std::tolower(a) == std::tolower(b);
    });
}

std::string_view trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
    value.remove_prefix(1);
  }
  while (!value.empty() &&
         (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
    value.remove_suffix(1);
  }
  return value;
}

bool has_connection_token(std::string_view value, std::string_view wanted) {
  for (const auto token : std::views::split(value, ',')) {
    if (equals_ignore_case(trim(std::string_view(token.begin(), token.end())), wanted)) {
      return true;
    }
  }
  return false;
}

bool should_keep_alive(std::string_view request, std::string_view version) {
  bool wants_close = false;
  bool wants_keep_alive = false;
  size_t line_start = request.find('\n');

  while (line_start != std::string_view::npos && ++line_start < request.size()) {
    const size_t line_end = request.find('\n', line_start);
    const std::string_view line = request.substr(line_start, line_end - line_start);
    if (trim(line).empty()) break;

    const size_t colon = line.find(':');
    if (colon != std::string_view::npos &&
        equals_ignore_case(trim(line.substr(0, colon)), "Connection")) {
      const std::string_view value = trim(line.substr(colon + 1));
      wants_close |= has_connection_token(value, "close");
      wants_keep_alive |= has_connection_token(value, "keep-alive");
    }

    line_start = line_end;
  }

  if (wants_close) return false;
  if (wants_keep_alive) return true;
  return equals_ignore_case(trim(version), "HTTP/1.1");
}

std::optional<std::string> load_file(std::string& path) {
  if (!std::filesystem::is_regular_file(path)) {
    return std::nullopt;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  return std::string(
    std::istreambuf_iterator<char>(file),
    std::istreambuf_iterator<char>()
  );
}

std::string_view get_content_type(const std::filesystem::path& path) {
  const auto extension = path.extension().string();

  if (extension == ".html") return "text/html; charset=utf-8";
  if (extension == ".css")  return "text/css; charset=utf-8";
  if (extension == ".js")   return "text/javascript; charset=utf-8";
  if (extension == ".json") return "application/json";
  if (extension == ".png")  return "image/png";
  if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
  if (extension == ".svg")  return "image/svg+xml";
  if (extension == ".ico")  return "image/x-icon";

  return "application/octet-stream";
}

std::optional<std::string> translate_path(std::string_view& path) {
  if (path == "/") return "testfiles/index.html";
  std::string res = "testfiles";
  res += path;
  return res;
}

std::string_view get_line(const char* buffer, size_t len) {
  std::string_view res(buffer, len);

  size_t offset{};
  while (res[offset] != '\n') {
    offset++;
  }
  return res.substr(0, offset);
}

int initialize_socket(int* fd, const std::string* IP, const int PORT) {
  *fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*fd < 0) {
    return -1;
  }

  int reuse = 1;
  if (setsockopt(
        *fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
      ) < 0) {
    close(*fd);
    return -1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  inet_pton(AF_INET, IP->data(), &addr.sin_addr);

  return bind(*fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
}

bool process_request(int client_fd) {
  char buffer[4096];

  ssize_t s = recv(client_fd, buffer, 4096, 0);
  if (s <= 0) {
    close(client_fd);
    return false;
  }
  std::cout << "Received message from the client: " << std::endl;

  const std::string_view request(buffer, static_cast<size_t>(s));
  std::string_view line = get_line(buffer, static_cast<size_t>(s));

  HttpRequest req = parse_request(line);
  const bool keep_alive = should_keep_alive(request, req.version);

  if (req.type == RequestType::GET) std::cout << "GET request" << std::endl;
  std::cout << req.path << std::endl;
  

  std::optional<std::string> path = translate_path(req.path);
  if (!path) {
    throw std::runtime_error("Path does not exist");
  }

  std::optional<std::string> file = load_file(path.value());

  if (!file) {
    const std::string response =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 0\r\n"
    "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + "\r\n"
    "\r\n";

    send(client_fd, response.data(), response.size(), 0);
    std::cout << "File did not exist, sent 404" << std::endl;
  } else {
    const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: " +  std::string(get_content_type(path.value())) + "\r\n"
      "Content-Length: " + std::to_string(file->size()) + "\r\n"
      "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + "\r\n"
      "\r\n";

    send(client_fd, headers.data(), headers.size(), 0);
    send(client_fd, file->data(), file->size(), 0);
    std::cout << "File existed, sent " << path.value() << std::endl;
  }
  std::cout << std::endl;

  if (!keep_alive) {
    close(client_fd);
    return false;
  }
  return true;
}



}


Server::Server(const std::string& ip, int port) : ip_addr_(ip), port_(port) {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::runtime_error("Failed to create socket: " + std::string(std::strerror(errno)));
  }
  int reuse = 1;
  if (setsockopt(
        server_fd_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
      ) < 0) {
    close(server_fd_);
    throw std::runtime_error("Failed to set socket options: " + std::string(std::strerror(errno)));
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  inet_pton(AF_INET, ip.data(), &addr.sin_addr);
  
  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    throw std::runtime_error("Failed to bind socket: " + std::string(std::strerror(errno)));
  }
}

bool Server::Start() {
  listen(server_fd_, SOMAXCONN);

  int cores = std::max(std::thread::hardware_concurrency(), 1u) * (1 + (50/50));

  for (int i{}; i < cores; i++) {
    threads.emplace_back(&Server::Work, this);
  }

  while (true) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    std::cout << "Listening for connections..." << std::endl;
    int fd = accept(
      server_fd_,
      reinterpret_cast<sockaddr*>(&client_addr),
      &client_len
    );

    {
      std::lock_guard lock(mt);
      request_queue_.emplace(fd, client_addr, client_len);
    }

    std::cout << "Accepted a connection from "
              << print_ip(client_addr.sin_addr)
              << std::endl;
  }

  return true;
}

void Server::Work() {
  while (true) {
    ConnectionInfo conn;
    {
      std::lock_guard lock(mt);
      if (!request_queue_.size()) continue;
      conn = request_queue_.front();
      request_queue_.pop();
    }
    std::cout << "handed connection to worker" << std::endl;
    bool open = true;
    while (open) {
      open = process_request(conn.fd);
    }
    std::cout << "Connection closed" << std::endl;
  }
}
