#include "client_http.hpp"
#include "server_http.hpp"
#include "Client.h"
#include "Command.h"
#include "Socket.h"
#include "Order.h"

// Added for the default_resource example
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

// String parser, from https://stackoverflow.com/questions/236129/the-most-elegant-way-to-iterate-the-words-of-a-string
template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

// Read in an HTML file and store it in a string
std::string readHtml ( const std::string& filename )
{
    try {
        auto web_root_path = std::filesystem::canonical ( "../html" );
        auto path = std::filesystem::canonical ( web_root_path / filename );

        // Check if path is within web_root_path
        if ( std::distance ( web_root_path.begin(), web_root_path.end() ) >
                 std::distance ( path.begin(), path.end() ) ||
             !std::equal ( web_root_path.begin(),
                           web_root_path.end(),
                           path.begin() ) ) {
            throw std::invalid_argument ( "path must be within root path" );
        }
        if ( std::filesystem::is_directory ( path ) ) {
            path /= "index.html";
        }

        // Read the file
        std::cout << "Loading " << path << std::endl;
        std::ifstream fstream ( path );
        std::stringstream sstream;
        sstream << fstream.rdbuf();
        return sstream.str();
    } catch ( const std::exception &e ) {
        return "Could not open file " + filename + ": " + e.what();
    }
}

// Add an "OK" HTTP header to HTML content
std::string generateOkResponse ( const std::string& content )
{
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\nContent-Length: "
             << content.length() << "\r\n\r\n"
             << content;
    return response.str();
}

void substituteInventory ( std::map<std::string, int> inventory,
                           std::string& content )
{
    std::string strToReplace { "DYNAMIC_INVENTORY" };
    std::stringstream replacement;
    size_t startPos = content.find ( strToReplace );
    if ( startPos != std::string::npos ) {
        for ( const auto& item : inventory ) {
            replacement << "<tr><td>" << item.first << " " << item.second
                        << "</td></tr>";
        }
    }
    content.replace ( startPos, strToReplace.size(), replacement.str() );
}

void substituteOptions ( std::vector<std::string> options,
                         std::string& content )
{
    std::string strToReplace { "DYNAMIC_OPTIONS" };
    std::stringstream replacement;
    replacement << "<tr><td><select name='item'>";
    size_t startPos = content.find ( strToReplace );
    if ( startPos != std::string::npos ) {
        for ( const auto& option : options ) {
            replacement << "<option value='" << option
                        << "'>" << option << "</option>";
        }
    }
    replacement << "</select></td></tr>";
    content.replace ( startPos, strToReplace.size(), replacement.str() );
}

void substituteLocations ( std::vector<std::string> options,
                           std::string& content )
{
    std::string strToReplace { "DYNAMIC_LOCATIONS" };
    std::stringstream replacement;
    size_t startPos = content.find ( strToReplace );
    if ( startPos != std::string::npos ) {
        for ( const auto& option : options ) {
            replacement << "<option value='" << option
                        << "'>" << option << "</option>";
        }
    }
    content.replace ( startPos, strToReplace.size(), replacement.str() );
}

std::string generateResultPage ( const std::string& result )
{
    std::stringstream output;
    output << "<!DOCTYPE html><html><head></head>"
           << "<body style='text-align:center;'><p>"
           << result
           << "</p><a href='/'>Place a new order</a></body></html>";
    return output.str();
}

int main() {
    // HTTP-server at port 8080 using 1 thread
    // Unless you do more heavy non-threaded processing in the resources,
    // 1 thread is usually faster than several threads
    HttpServer server;
    server.config.port = 8080;

    // Add resources using path-regex and method-string, and an anonymous
    // function

    // Default GET resource. If no other matches, this anonymous function will
    // be called.
    server.default_resource["GET"] = 
        []( std::shared_ptr<HttpServer::Response> response, 
            std::shared_ptr<HttpServer::Request> /* request */ ) {

            // Get current inventory
            /*
            Command command("summary");
            Client client ( SocketType::IP, "127.0.0.1:5000" );

            std::string curr_inv = client.send ( command );

            std::vector<std::string> items = split ( curr_inv, '\n' );
            */

            std::map<std::string, int> tempInv { { "apple", 10 },
                                                 { "banana", 10 },
                                                 { "currant", 50 } };
            std::vector<std::string> tempOpt;
            for ( const auto& item : tempInv ) {
                tempOpt.push_back ( item.first );
            }
            std::vector<std::string> tempLoc { "couch", "kitchen", "bench" };

            // Load the html and substitute the dynamic parts
            std::string content = readHtml ( "index.html" );
            substituteInventory ( tempInv, content );
            substituteOptions ( tempOpt, content );
            substituteLocations ( tempLoc, content );

            *response << generateOkResponse ( content );

            // Alternatively, use one of the convenience functions, for
            // instance:
            // response->write(stream);
        };

  // Process user's order using POST
  server.resource["^/result$"]["POST"] =
    []( std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {

        // Retrieve POST string:
        auto input = request->content.string();

        /*
        // Parse inputs
        std::vector<std::string> inputs = split(input, '&');
        std::map<std::string, int> items;
        std::string location;

        for ( auto it = inputs.begin(); it != inputs.end(); ++it) {
            std::vector<std::string> single = split(*it, '=');

          if(single[0] == "item"){
            // Check if item is in map
            if(items.find(std::string(single[1])) != items.end()){
              items[single[1]]++;
            }
            // Otherwise, add it
            else{
              items.insert(std::pair<std::string, int>(std::string(single[1]), 1));
            }
          }
          else if(single[0] == "location"){};
            location = single[1];
        }

        // Create order
        Order order ( location, items );
        order.write_serial();

        // Send order to inventory server
        Client client ( SocketType::IP, "127.0.0.1:5000" );
        std::string inv_resp = client.send(order);
        */

        // Let user know order status
        std::string content = generateResultPage ( "Order placed" );
        *response << generateOkResponse ( content );
      };

  server.on_error =
    []( std::shared_ptr<HttpServer::Request> /*request*/,
        const SimpleWeb::error_code & /*ec*/) {
        // Handle errors here
        // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
      };

  // Start up the server
  std::thread server_thread ( [&server]() { server.start(); });

  // Wait for server to start so that the clients can connect
  std::this_thread::sleep_for ( std::chrono::seconds ( 1 ) );

  server_thread.join();
}
