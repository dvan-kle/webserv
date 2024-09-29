#!/usr/bin/env python3
import sys
import os
import urllib.parse

def main():
    # Output HTTP headers
    
    request_method = os.environ.get('REQUEST_METHOD', 'GET')
    
    # Initialize an empty dictionary to hold parameters
    params = {}

    if request_method == 'POST':
        # Read POST data from stdin
        content_length = os.environ.get('CONTENT_LENGTH')
        if content_length:
            try:
                content_length = int(content_length)
                post_data = sys.stdin.read(content_length)
                # Parse the POST data
                params = urllib.parse.parse_qs(post_data)
            except Exception as e:
                print("<html><body>")
                print(f"<p>Error processing POST data: {e}</p>")
                print("</body></html>")
                return
    elif request_method == 'GET':
        # Parse the QUERY_STRING
        query_string = os.environ.get('QUERY_STRING', '')
        params = urllib.parse.parse_qs(query_string)
    else:
        print("<html><body>")
        print(f"<p>Unsupported request method: {request_method}</p>")
        print("</body></html>")
        return

    # Print the received values
    print("<html><body>")
    print(f"<h1>Received {request_method} Data</h1>")
    if params:
        print("<ul>")
        for key, values in params.items():
            for value in values:
                print(f"<li>{key}: {value}</li>")
        print("</ul>")
    else:
        print("<p>No data received.</p>")
    print("</body></html>")

if __name__ == "__main__":
    main()
