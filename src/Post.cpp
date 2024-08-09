/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Post.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/09 15:05:16 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/09 18:05:30 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Request.hpp"

void Request::parseFullRequest(std::string request)
{
	contentLength(request);
	std::cout << "Content-Length: " << _content_length << std::endl;
	int left = _content_length - request.size();
	std::cout << "Left: " << left << std::endl;
	while (left > 0)
	{
		int bytes_read = read(_client_fd, _buffer, 1024);
		if (bytes_read == -1)
		{
			std::cerr << "Error: read failed" << std::endl;
			close(_client_fd);
			exit(EXIT_FAILURE);
		}
		else if (bytes_read == 0)
		{
			std::cerr << "Error: client disconnected" << std::endl;
			_client_fd = -1;
			//exit(EXIT_FAILURE);
		}
		left -= bytes_read;
		if (!(left > 0))
			_buffer[bytes_read] = '\0';
		request.append(_buffer);
		std::cout << left << std::endl;
	}
	std::cout << "Request: " << request << std::endl;

	
}

void Request::PostResponse(std::string request)
{
	std::cout << request << std::endl;
	parseFullRequest(request);
	
	std::string boundary;
    std::string line;
    std::istringstream requestStream(request);

	while (std::getline(requestStream, line) && line != "\r")
	{
		// std::cout << line << std::endl;
        if (line.find("Content-Type: multipart/form-data; boundary=") != std::string::npos)
		{
            boundary = line.substr(line.find("=") + 1);
            boundary = "--" + boundary;
        }
    }
    if (boundary.empty())
        std::cout << "Boundary not found in the request headers" << std::endl;
		
    std::string fileData;
    std::string filename;
    bool inFile = false;

    // Read the multipart data and extract the file content
    while (std::getline(requestStream, line))
	{
        if (line.find(boundary) != std::string::npos)
		{
            if (inFile)
                break;
            inFile = true;
            // Skip headers for the file part
            for (int i = 0; i < 2; ++i)
			{
                std::getline(requestStream, line);
                if (line.find("filename=") != std::string::npos)
				{
                    filename = line.substr(line.find("filename=") + 10);
                    filename = filename.substr(0, filename.size() - 2); // Remove trailing quote and CR
                }
            }
            std::getline(requestStream, line); // Skip the empty line
        }
		else if (inFile)
            fileData += line + "\n";
    }

    // Remove the last line's newline character
	std::cout << "filename: " << filename << std::endl;
	std::cout << "filedata: " << fileData << std::endl;
	
    if (!fileData.empty())
        fileData.pop_back();

    // Create the uploads directory if it does not exist
    std::string uploadDir = "uploads";
    createDir(uploadDir);

    // Save the file content to the uploads directory
    std::string filePath = uploadDir + "/" + filename;
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile)
	{
        std::cout << "Error opening output file for writing" << std::endl;
		return;
    }
    outFile << fileData;
    outFile.close();

    std::string htlmContent = "<html><body><h1>File uploaded successfully!</h1></body></html>";
	_response += _http_version + " " + HTTP_200 + CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htlmContent.size()) + "\r\n\r\n";
	_response += htlmContent;
	
	ssize_t bytes_written = write(_client_fd, _response.c_str(), strlen(_response.c_str()));
	if (bytes_written == -1)
		std::cout << "Error: write failed" << std::endl;
}

void Request::createDir(std::string name)
{
	struct stat st = {0};
	if (stat(name.c_str(), &st) == -1)
		mkdir(name.c_str(), 0700);
}

void Request::contentLength(std::string request)
{
    size_t content_length_pos = request.find("Content-Length:");
    if (content_length_pos != std::string::npos)
	{
        size_t start_pos = content_length_pos + 15;
        size_t end_pos = request.find("\r\n", start_pos);
        if (end_pos != std::string::npos)
		{
            std::string content_length_str = request.substr(start_pos, end_pos - start_pos);
            _content_length = std::stoi(content_length_str);
        }
    }
}