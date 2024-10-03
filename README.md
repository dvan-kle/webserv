# Webserv

## Overview

This project is part of the 42 curriculum and aims to build a simple web server that adheres to the HTTP/1.1 specification. The goal is to gain a deeper understanding of how web servers operate, how HTTP works, and to create a server capable of handling multiple client requests, serving static files, and executing CGI scripts.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Usage](#usage)
- [Directory Structure](#directory-structure)
- [Built With](#built-with)
- [Project Requirements](#project-requirements)
- [Contributors](#contributors)

## Introduction

In this project, we implemented a simplified web server, similar to `nginx` or `Apache`, that can serve HTTP requests over a network. The server can handle multiple types of requests, such as:

- `GET`: Retrieve static content.
- `POST`: Accept form data or upload files.
- `DELETE`: Delete a resource on the server.

It also implements some fundamental concepts like handling concurrent connections, parsing and responding to HTTP requests, and supporting CGI for dynamic content.

## Features

- Serve static files (HTML, CSS, JS, images, etc.)
- Handle HTTP methods: `GET`, `POST`, and `DELETE`.
- HTTP request parsing and response generation.
- CGI (Common Gateway Interface) support for dynamic content.
- Multiple client support using non-blocking I/O (epoll, select, or poll).
- Custom error pages.
- Configurable server behavior via a configuration file.
- Graceful handling of HTTP status codes (e.g., `200 OK`, `404 Not Found`, `500 Internal Server Error`).

## Getting Started

### Prerequisites

To compile and run this project, you need the following:

- A Unix-based operating system (Linux, macOS).
- A working C++11 or later compiler.
- A basic understanding of networking and HTTP.

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/your_username/webserv.git
   cd webserv
2. Build the project:
   ```bash
   make
3. Run the server:
   ```bash
   ./webserv <path_to_configuration_file>

## Configuration

The server is configured using a configuration file that specifies various server settings such as port, server name, root directory, and more. Here's an example configuration:
  ```json
  server {
      listen 8080;
      server_name localhost;
  
      location / {
          root /var/www/html;
          index index.html;
      }
  
      location /cgi-bin/ {
          root /var/www/cgi-bin;
          cgi_pass /usr/bin/python3;
      }
  
      error_page 404 /404.html;
  }
```

### Configuration Options

`listen`: Specifies the port to listen on.
`server_name`: Defines the domain name or IP address the server will respond to.
`root`: The root directory for serving files.
`index`: The default file to serve if no file is specified in the request.
`cgi_pass`: Path to the CGI executable (e.g., Python, PHP, or any custom script).
`error_page`: Custom error page paths for different HTTP error codes.





