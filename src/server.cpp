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

void process_request(int client_fd) {
  char buffer[4096];

  ssize_t s = recv(client_fd, buffer, 4096, 0);
  std::cout << "Received message from the client: " << std::endl;
  
  std::string_view line = get_line(buffer, s);

  HttpRequest req = parse_request(line);

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
    "Content-Length: 0\r\n";

    send(client_fd, response.data(), response.size(), 0);
    std::cout << "File did not exist, sent 404" << std::endl;
  } else {
    const std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: " +  std::string(get_content_type(path.value())) + "\r\n"
      "Content-Length: " + std::to_string(file->size()) + "\r\n"
      "Connection: close\r\n"
      "\r\n";

    send(client_fd, headers.data(), headers.size(), 0);
    send(client_fd, file->data(), file->size(), 0);
    std::cout << "File existed, sent " << path.value() << std::endl;
  }
}

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

  int fd;
  std::cout << "Initializing..." << std::endl;
  int success = initialize_socket(&fd, &IP, port);
  if (success < 0) {
    std::cout << "Failed to initialize: " << errno << std::endl;
    return 0;
  }

  listen(fd, SOMAXCONN);

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);

  while (true) {
    std::cout << "Listening on " << IP << ":" << port << "..." << std::endl;
    int client_fd = accept(
      fd,
      reinterpret_cast<sockaddr*>(&client_addr),
      &client_len
    );

    std::cout << "Client connected!" << std::endl;
    process_request(client_fd);
    std::cout << std::endl << std::endl;
  }


  close(fd);
  return 0;
}
