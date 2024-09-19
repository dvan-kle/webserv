#!/usr/bin/env python3
import os
import sys
import urllib.parse

def handle_get():
    query_string = os.environ.get('QUERY_STRING', '')
    if query_string:
        query_params = urllib.parse.parse_qs(query_string)
    else:
        query_params = {}

    print("Content-Type: text/html\n")
    print("<html><head><title>GET Request</title></head><body>")
    print("<h1>GET Request</h1>")
    
    if query_params:
        print("<h2>Query Parameters:</h2>")
        for key, values in query_params.items():
            print(f"<p>{key}: {', '.join(values)}</p>")
    else:
        print("<p>No query parameters provided.</p>")
    
    print("</body></html>")

def handle_post():
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    post_data = sys.stdin.read(content_length)
    form_data = urllib.parse.parse_qs(post_data)

    print("Content-Type: text/html\n")
    print("<html><head><title>POST Request</title></head><body>")
    print("<h1>POST Request</h1>")

    if form_data:
        print("<h2>Form Data:</h2>")
        for key, values in form_data.items():
            print(f"<p>{key}: {', '.join(values)}</p>")
    else:
        print("<p>No form data provided.</p>")
    
    print("</body></html>")

def main():
    method = os.environ.get('REQUEST_METHOD', 'GET')
    
    if method == "POST":
        handle_post()
    elif method == "GET":
        handle_get()
    else:
        print("Content-Type: text/html\n")
        print("<html><body><h1>405 Method Not Allowed</h1></body></html>")

if __name__ == "__main__":
    main()
