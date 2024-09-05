/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Post.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/09 15:05:16 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/09/05 09:50:50 by trstn4        ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Request.hpp"

void Request::PostResponse(std::string request)
{
	std::string boundary;
    std::string line;
    std::istringstream requestStream(request);

	while (std::getline(requestStream, line) && line != "\r")
	{
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