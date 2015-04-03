#include "httpclient.h"





namespace http_client {



#ifdef HTTPS
bool verifyCA(bool preverified, boost::asio::ssl::verify_context& ctx)
{
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert((X509_STORE_CTX*) ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    //std::cout << "------------------\nVerifying " << subject_name << "\ntrust: " << preverified<<"\n";

    return preverified;
}
#endif

HttpClient::HttpClient(const string &host, const string &port)
{
    try{
        // Get a list of endpoints
        tcp::resolver resolver(_io_service);
#ifdef HTTPS
        _pquery = new tcp::resolver::query(host, "https");
#else
        if (port == "")
            _pquery = new tcp::resolver::query(host, "http");
        else
            _pquery = new tcp::resolver::query(host, port);
#endif
        _endpoint_iter = resolver.resolve(*_pquery);

        // Try each endpoint util we successfull establish a connection
#ifdef HTTPS
        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        ctx.set_default_verify_paths();
        _psocket = new ssl_socket(_io_service, ctx);
        boost::asio::connect(_psocket->lowest_layer(), _endpoint_iter);
        _psocket->set_verify_mode(boost::asio::ssl::verify_peer);
        _psocket->set_verify_callback(verifyCA);
        _psocket->handshake(ssl_socket::client);
#else
        _psocket = new tcp::socket(_io_service);
        boost::asio::connect(*_psocket, _endpoint_iter);
#endif
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n\n";
    }

    _host = host;
}

HttpClient::~HttpClient()
{
    if (_pquery != NULL)
        delete _pquery;
    if (_psocket != NULL)
        delete _psocket;
}

string HttpClient::get(const string &path, data_type type)
{
    try
        {
            // Form the request. We specify the "Connection: close" header so that the s
            // server will close the socket after transmitting the response. This will
            // allow us to treat all data up until the EOF as the content.
            boost::asio::streambuf request;
            std::ostream request_stream(&request);
            request_stream << "GET " << path << " HTTP/1.1\r\n";
            request_stream << "Host: " << _host << "\r\n";
            if (type == data_type::JSON)
                request_stream << "Accept: application/json \r\n";
            else
                request_stream << "Accept: */* \r\n";
            if (_x_api_key != "")
                request_stream << "X-Api-Key: " << _x_api_key << "\r\n";
            if (_access_token != "")
                request_stream << "Authorization: Bearer " << _access_token << "\r\n";
            request_stream << "Connection: close \r\n\r\n";

            // Send the request
            boost::asio::write(*_psocket, request);

            // Read the response status line. The response streambuf will automatically
            // grow to accommodate the entire line. The growth may be limited by passing
            // a maximum size to the streambuf constructor.
            boost::asio::streambuf response;
            boost::asio::read_until(*_psocket, response, "\r\n");
            // Check that response is OK
            std::istream response_stream(&response);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);

            if (!response_stream || http_version.substr(0,5) != "HTTP/")
            {
                return "error: Invalid response";
            }

            if (status_code != 200)
            {
                std::cout << "status_code: " << status_code << "\n";
                return "error: Status Code not 2000";
            }

            // Read the response headers, which are terminated by a black line.
            boost::asio::read_until(*_psocket, response, "\r\n\r\n");
            // Process the response headers
            std::string header;
            while (std::getline(response_stream, header) && header != "\r"){
            //do something with this header
            }

            string ret;			// result body
            ostringstream ssRes;
            ssRes << &response;
            ret.append(ssRes.str());
            // Read until EOF
            boost::system::error_code ec;
            while (boost::asio::read(*_psocket, response, boost::asio::transfer_at_least(1), ec)){
                ostringstream ssRes;
                ssRes << &response;
                ret.append(ssRes.str());
            }
            if ( ec != boost::asio::error::eof){
                throw boost::system::system_error(ec);
            }
            return ret;
        }
        catch (std::exception& e)
        {
            std::cout << "Exception: " << e.what() << "\n\n";
            return std::string("\"error\":")  + std::string(e.what());
        }
}

    json HttpClient::getJSON (const string& path){
        try{
            string data = get(path, data_type::JSON);
            return json::parse(data);
        }catch (std::exception & e){
            std::cout << "Exception: " << e.what() << "\n\n";
            return json::parse(std::string("\"error\":")  + std::string(e.what()));
        }
    }


    json HttpClient::postJSON(const string& path, const string& post_data)
    {
        try
        {
            // Form the request. We specify the "Connection: close" header so that the s
            // server will close the socket after transmitting the response. This will
            // allow us to treat all data up until the EOF as the content.
            boost::asio::streambuf request;
            std::ostream request_stream(&request);
            request_stream << "POST " << path << " HTTP/1.1\r\n";
            request_stream << "Host: " << _host << "\r\n";
            request_stream << "Accept: application/json \r\n";
            if (_x_api_key != "")
                request_stream << "X-Api-Key: " << _x_api_key << "\r\n";
            if (_access_token != "")
                request_stream << "Authorization: Bearer " << _access_token << "\r\n";
            request_stream << "Content-type: application/json \r\n";
            int content_length = post_data.length();
            request_stream << "Content-length: " << content_length << "\r\n";
            request_stream << "Connection: close \r\n\r\n";

            request_stream << post_data;

            // Send the request
            boost::asio::write(*_psocket, request);

            // Read the response status line. The response streambuf will automatically
            // grow to accommodate the entire line. The growth may be limited by passing
            // a maximum size to the streambuf constructor.
            boost::asio::streambuf response;
            boost::asio::read_until(*_psocket, response, "\r\n");
            // Check that response is OK
            std::istream response_stream(&response);
            std::string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;
            std::string status_message;
            std::getline(response_stream, status_message);

            if (!response_stream || http_version.substr(0,5) != "HTTP/")
            {
                return json::parse("{\"error\": \"Invalid response\"}");
            }

            if (status_code != 200)
            {
                std::cout << "status_code: " << status_code << "\n";
                return json::parse("{\"error\": \"Status Code not 2000\"}");
            }

            // Read the response headers, which are terminated by a black line.
            boost::asio::read_until(*_psocket, response, "\r\n\r\n");
            // Process the response headers
            std::string header;
            while (std::getline(response_stream, header) && header != "\r"){
            //do something with this header
            }

               // Read until EOF
               boost::system::error_code ec;
               std::stringstream ret;
               while (boost::asio::read(*_psocket, response, boost::asio::transfer_at_least(1), ec))
                   ret << &response;
               if ( ec != boost::asio::error::eof){
                   throw boost::system::system_error(ec);
               }

               return json::parse(ret.str());
           }
           catch (std::exception& e)
           {
               std::cout << "Exception: " << e.what() << "\n\n";
               return json::parse(std::string("\"error\":")  + std::string(e.what()));
           }
   }

} // namespace http_client
