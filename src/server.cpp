#include "client_http.hpp"
#include "server_http.hpp"
#include "Client.h"
#include "Command.h"
#include "InventoryMsg.h"
#include "Locations.h"
#include "Socket.h"
#include "Order.h"

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

// Convert the POST result from the input form into order
std::tuple<std::map<std::string, int>, int>
convertToOrder ( const std::string& postString ) {
    std::map<std::string, int> order;
    int location;

    std::stringstream stream ( postString );
    std::string item;
    while ( std::getline ( stream, item, '&' ) ) {
        size_t splitPos = item.find ( '=' );
        std::string name =  item.substr ( 0, splitPos );
        std::string qty = item.substr ( splitPos+1 );
        if ( name == "location" ) {
            location = std::stoi ( qty );
        } else if ( !name.empty() && !qty.empty() ) {
            try {
                order.insert ( { std::move ( name ), std::stoi ( qty ) } );
            } catch ( const std::invalid_argument & ) {
                std::cout << "Invalid quantity entered" << std::endl;
            }
        }
    }
    return std::make_tuple ( order, location );
}

// Read in an HTML file and store it in a string
std::string readHtml ( const std::string& filename )
{
    try {
        auto web_root_path = boost::filesystem::canonical ( "../html" );
        auto path = boost::filesystem::canonical ( web_root_path / filename );

        // Check if path is within web_root_path
        if ( std::distance ( web_root_path.begin(), web_root_path.end() ) >
                 std::distance ( path.begin(), path.end() ) ||
             !std::equal ( web_root_path.begin(),
                           web_root_path.end(),
                           path.begin() ) ) {
            throw std::invalid_argument ( "path must be within root path" );
        }
        if ( boost::filesystem::is_directory ( path ) ) {
            path /= "index.html";
        }

        // Read the file
        std::ifstream fstream ( path.string() );
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

// Insert inventory contents into HTML
void substituteInventory ( std::map<std::string, int> inventory,
                           std::string& content )
{
    std::string strToReplace { "DYNAMIC_INVENTORY" };
    std::stringstream replacement;
    size_t startPos = content.find ( strToReplace );
    if ( startPos != std::string::npos ) {
        for ( const auto& item : inventory ) {
            replacement << "<tr><td>" << item.first << "</td>"
                        << "<td> " << item.second << "</td>"
                        << "<td><input type='number' name='" << item.first
                        << "'></td></tr>";
        }
    }
    content.replace ( startPos, strToReplace.size(), replacement.str() );
}

// Insert delivery locations into HTML
void substituteLocations ( std::map<int, std::string> options,
                           std::string& content )
{
    std::string strToReplace { "DYNAMIC_LOCATIONS" };
    std::stringstream replacement;
    size_t startPos = content.find ( strToReplace );
    if ( startPos != std::string::npos ) {
        for ( const auto& option : options ) {
            replacement << "<option value='" << option.first
                        << "'>" << option.second << "</option>";
        }
    }
    content.replace ( startPos, strToReplace.size(), replacement.str() );
}

// Generate simple dynamic results page
std::string generateResultPage ( const std::string& result )
{
    std::stringstream output;
    output << "<!DOCTYPE html><html><head>"
           << "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
           << "</head>"
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

            // Get current inventory and locations
            Client client ( SocketType::IP, "127.0.0.1:5000" );
            Command command;

            command.set_command ( "inventory" );
            InventoryMsg inv_msg { client.send ( command ) };

            command.set_command ( "locations" );
            Locations loc_msg { client.send ( command ) };

            // Load the html and substitute the dynamic parts
            std::string content = readHtml ( "index.html" );
            substituteInventory ( inv_msg.get_items(), content );
            substituteLocations ( loc_msg.get_locations(), content );

            *response << generateOkResponse ( content );

            // Alternatively, use one of the convenience functions, for
            // instance:
            // response->write(stream);
        };

  // Process user's order using POST
  server.resource["^/result$"]["POST"] =
    []( std::shared_ptr<HttpServer::Response> response,
        std::shared_ptr<HttpServer::Request> request) {

        // Retrieve and convert POST string
        std::string input = request->content.string();
        std::map<std::string, int> items;
        int location;
        std::tie ( items, location ) = convertToOrder ( input );

        // Create order
        Order order;
        order.set_location ( location );
        order.set_items ( items );

        // Send order to inventory server
        Client client ( SocketType::IP, "127.0.0.1:5000" );
        std::string inv_resp = client.send(order);

        // Let user know order status
        std::stringstream debug;
        debug << inv_resp;

        if ( inv_resp == "Order placed" ) {
            debug << ". Sending ";
            bool first { true };
            for ( auto item : items ) {
                if ( !first ) {
                    debug << ", ";
                }
                debug << item.second << " " << item.first << "(s)";
                first = false;
            }
            debug << " to " << location;
        }
        std::string content = generateResultPage ( debug.str() );
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
