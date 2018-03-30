#include "client_http.hpp"
#include "server_http.hpp"
#include "inventory.h"

// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

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
  server.resource["^/submit$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    // Retrieve POST string:
    auto input = request->content.string();

    // Parse inputs
    vector<string> inputs = split(input, '&');
    map<robie_inv::ItemType, int> items;

    for(vector<string>::iterator it = inputs.begin(); it != inputs.end(); ++it) {
      vector<string> single = split(*it, '=');

      if(single[0] == "item"){
        // Check if item is in map
        if(items.find(robie_inv::ItemType(single[1])) != items.end()){
          items[single[1]]++;
        }
        // Otherwise, add it
        else{
          items.insert(pair<robie_inv::ItemType, int>(robie_inv::ItemType(single[1]), 1));
        }
      }
      else if(single[0] == "location"){};
        // TODO: implement order locations
    }

    // Create order
    robie_inv::Order order(items);
    order.write_serial();

    // Send order to inventory server
    robie_comm::Client client("localhost", 5000);
    client.connect();
    string inv_resp = client.send(order);
    client.disconnect();

    // Let user know order status
    stringstream output;
    output << "<html><head></head><body style='text-align: center'>";
    output << "<p>" << inv_resp << "</p>";
    output << "<a href='/order'>Place a new order</a>";
    output << "</body></html>";
    string content = output.str();
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;

    // Alternatively, use one of the convenience functions, for instance:
    // response->write(stream);
  };

  // Place order
  server.resource["^/order$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    // Send order to inventory server
    robie_comm::Client client("localhost", 5000);
    client.connect();
    client.disconnect();

    // Do things the Biff way for now
    stringstream output;
    output << "<!DOCTYPE html>\n";
    output << "<html>\n";
    output << "  <head>\n";
    output << "    <title>How can I help you?</title>\n";
    output << "    <script>\n";
    output << "      function addItem() {\n";
    output << "        var count_rows = document.getElementById('left_table').rows.length;\n";
    output << "        var row = document.getElementById('left_table').insertRow(index = count_rows-1);\n";
    output << "        row.innerHTML = \"<tr><td> <select name='item'> <option value='apple'>Apple</option> <option value='cracker'>Cracker</option> <option value='granola'>Granola Bar</option> </select> </td></tr>\";\n";
    output << "      }\n";
    output << "    </script>\n";
    output << "  </head>\n";
    output << "  <body style='text-align:center;'>\n";
    output << "    <p>Please select a snack and delivery location.</p>\n";
    output << "    <form action='/submit' method='post'>\n";
    output << "      <table style='text-align:center; margin-left:auto; margin-right:auto;'>\n";
    output << "        <tr>\n";
    output << "          <td>\n";
    output << "            <table id='left_table'>\n";
    output << "              <tr><td>Items</td></tr>\n";
    output << "              <tr><td><button type='button' onclick='addItem()'>+</button></td></tr>\n";
    output << "            </table>\n";
    output << "          </td>\n";
    output << "          <td style='vertical-align:top;'>\n";
    output << "            <table id='right_table'>\n";
    output << "              <tr><td>Location</td></tr>\n";
    output << "              <tr><td><select name='location'>\n";
    output << "                <option value='couch'>Couch</option>\n";
    output << "                <option value='kitchen'>Kitchen</option>\n";
    output << "                <option value='bedroom'>Bedroom</option>\n";
    output << "              </select></td></tr>\n";
    output << "            </table>\n";
    output << "          </td>\n";
    output << "        </tr>\n";
    output << "      </table>\n";
    output << "      <button>Submit</button>\n";
    output << "    </form>\n";
    output << "  </body>\n";
    output << "</html>\n";

    string content = output.str();
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  // Default GET-resource. If no other matches, this anonymous function will be called.
  // Will respond with content in the html/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("../html");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

#ifdef HAVE_OPENSSL
//    Uncomment the following lines to enable ETag
//    {
//      ifstream ifs(path.string(), ifstream::in | ios::binary);
//      if(ifs) {
//        auto hash = SimpleWeb::Crypto::to_hex_string(SimpleWeb::Crypto::md5(ifs));
//        header.emplace("ETag", "\"" + hash + "\"");
//        auto it = request->header.find("If-None-Match");
//        if(it != request->header.end()) {
//          if(!it->second.empty() && it->second.compare(1, hash.size(), hash) == 0) {
//            response->write(SimpleWeb::StatusCode::redirection_not_modified, header);
//            return;
//          }
//        }
//      }
//      else
//        throw invalid_argument("could not read file");
//    }
#endif

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  // Wait for server to start so that the clients can connect
  this_thread::sleep_for(chrono::seconds(1));

  server_thread.join();
}
