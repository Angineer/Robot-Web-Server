#include "client_http.hpp"
#include "server_http.hpp"
#include "Client.h"
#include "Command.h"
#include "Socket.h"
#include "Order.h"

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
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

int main() {
  // HTTP-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  server.config.port = 8080;

  // Add resources using path-regex and method-string, and an anonymous function

  // POST order
  server.resource["^/submit$"]["POST"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
    // Retrieve POST string:
    auto input = request->content.string();

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

    // Let user know order status
    std::stringstream output;
    output << "<html><head></head><body style='text-align: center'>";
    output << "<p>" << inv_resp << "</p>";
    output << "<a href='/order'>Place a new order</a>";
    output << "</body></html>";
    std::string content = output.str();
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;

    // Alternatively, use one of the convenience functions, for instance:
    // response->write(stream);
  };

  // Place order
  server.resource["^/order$"]["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> /* request */ ) {
    // Get current inventory
    Command command("summary");
    Client client( SocketType::IP, "127.0.0.1:5000" );

    std::stringstream output;
    std::string curr_inv = client.send(command);

    std::vector<std::string> items = split(curr_inv, '\n');

    // Do things the Biff way for now
    output << "<!DOCTYPE html>\n";
    output << "<html>\n";
    output << "  <head>\n";
    output << "    <title>How can I help you?</title>\n";
    output << "    <script>\n";
    output << "      function addItem() {\n";
    output << "        var count_rows = document.getElementById('left_table').rows.length;\n";
    output << "        var row = document.getElementById('left_table').insertRow(index = count_rows-1);\n";
    output << "        row.innerHTML = \"<tr><td><select name='item'>";
    for(auto it = items.begin(); it != items.end(); ++it){
        std::vector<std::string> single = split(*it, ' ');
      output << "          <option value='" << single[1] << "'>" << single[1] << "</option>";
    }
    output << "</select></td></tr>\";";
    output << "      }\n";
    output << "    </script>\n";
    output << "  </head>\n";
    output << "  <body style='text-align:center;'>\n";
    output << "    <table style='margin-left:auto; margin-right:auto'>\n";
    output << "      <tr style='vertical-align:top;'><td style='vertical-align:top; padding:0px 30px;'>\n";
    output << "        <p>Current Inventory</p>\n";
    output << "        <table style='margin-left:auto; margin-right:auto'>\n";
    for(auto it = items.begin(); it != items.end(); ++it){
      output << "          <tr><td>" << *it << "</td></tr>\n";
    }
    output << "        </table>\n";
    output << "      </td><td style='vertical-align:top; padding:0px 30px;'>\n";
    output << "        <p>Please select a snack and delivery location</p>\n";
    output << "        <form action='/submit' method='post'>\n";
    output << "          <table style='display:inline; margin-left:auto; margin-right:auto;'>\n";
    output << "            <tr>\n";
    output << "              <td>\n";
    output << "                <table id='left_table'>\n";
    output << "                  <tr><td>Items</td></tr>\n";
    output << "                  <tr><td><button type='button' onclick='addItem()'>+</button></td></tr>\n";
    output << "                </table>\n";
    output << "              </td>\n";
    output << "              <td style='vertical-align:top;'>\n";
    output << "                <table id='right_table'>\n";
    output << "                  <tr><td>Location</td></tr>\n";
    output << "                  <tr><td><select name='location'>\n";
    output << "                    <option value='couch'>Couch</option>\n";
    output << "                    <option value='kitchen'>Kitchen</option>\n";
    output << "                    <option value='bedroom'>Bedroom</option>\n";
    output << "                  </select></td></tr>\n";
    output << "                </table>\n";
    output << "              </td>\n";
    output << "            </tr>\n";
    output << "          </table>\n";
    output << "          <br><button>Submit</button>\n";
    output << "        </form>\n";
    output << "      </td></tr>\n";
    output << "    </table>\n";
    output << "  </body>\n";
    output << "</html>\n";

    std::string content = output.str();
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  // Default GET-resource. If no other matches, this anonymous function will be called.
  // Will respond with content in the html/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("../html");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if ( std::distance(web_root_path.begin(), web_root_path.end())
              > std::distance(path.begin(), path.end()) ||
           !std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw std::invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

      auto ifs = std::make_shared<std::ifstream>();
      ifs->open(path.string(), std::ifstream::in | std::ios::binary | std::ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, std::ios::beg);

        header.emplace("Content-Length", std::to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const std::shared_ptr<HttpServer::Response> &response, const std::shared_ptr<std::ifstream> &ifs) {
            // Read and send 128 KB at a time
            static std::vector<char> buffer(131072); // Safe when server is running on one thread
            std::streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<std::streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<std::streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    std::cerr << "Connection interrupted" << std::endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw std::invalid_argument("could not read file");
    }
    catch(const std::exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](std::shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  std::thread server_thread([&server]() {
    // Start server
    server.start();
  });

  // Wait for server to start so that the clients can connect
  std::this_thread::sleep_for(std::chrono::seconds(1));

  server_thread.join();
}
