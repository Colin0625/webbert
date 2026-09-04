# Webbert

Webbert is my custom made HTTP server that serves my personal website,
[colinsepulveda.com](https://colinsepulveda.com). It is very simple and does not support TLS encryption,
so I put it behind nginx on a vps to keep myself and visitors safe. I asked Codex
to make a summary for me:

## Functionality

- Accepts IPv4 TCP connections and handles them with a pool of worker threads.
- Serves HTML, CSS, JavaScript, images, SVG files, and PDFs.
- Maps multiple URLs to the same HTML file for client-side routing.
- Returns `404 Not Found` for unknown routes or missing files.
- Supports HTTP/1.0 and HTTP/1.1 connection persistence rules.
- Closes idle keep-alive connections after 10 seconds.
- Handles partial socket writes and disconnected clients safely.

Again, the server is intended to run behind a reverse proxy such as nginx, which can
provide public TLS termination and additional request filtering.

## Build and run

The project currently uses `g++-14` and GNU Make:

```sh
make
./bin/main.out
```

The default port is `5001`. Pass another port as the first argument:

```sh
./bin/main.out 8080
```

You can also build and run it with:

```sh
make run PORT=8080
```

Routes and their corresponding files are defined near the top of
`src/server.cpp`.
