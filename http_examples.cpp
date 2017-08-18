#include "client_http.hpp"
#include "server_http.hpp"
#include "utility.hpp"

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

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;


using namespace SimpleWeb;

std::ifstream::pos_type filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg(); 
}

int main() {
	
	srand(time(nullptr));
	
	// HTTP-server at port 8080 using 1 thread
	// Unless you do more heavy non-threaded processing in the resources,
	// 1 thread is usually faster than several threads
	HttpServer server;
	server.config.port = 8093;
	server.config.thread_pool_size = 10;

	// POST-example for the path /json, responds firstName+" "+lastName from the posted json
	// Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
	// Example posted json:
	// {
	//	 "Uri": "/bao/uploaded/i3/12778391/TB2goXhjohnpuFjSZFEXXX0PFXa_!!12778391.jpg_180x180.jpg",
	//	 "Cookie": "l=AsrKoy-KJoVmJmfwh5NxmpAsmrpsu04V",
	//	 "Referer": "https://item.taobao.com/item.htm",
	//	 "Origin": "",
	//	 "Host": "img.alicdn.com",
	//   "ToFilename":"",
	//   "HasExt":""
	// }
	server.resource["^/offimg$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

		string _Uri;
		string _Cookie;
		string _Referer;
		string _Origin;
		string _Host;
		string _ToFilename;
		string _HasExt;

		try {
			
			ptree pt;
			read_json(request->content, pt);

			 _Uri = pt.get<string>("Uri");
			 _Cookie = pt.get<string>("Cookie");
			 _Referer = pt.get<string>("Referer");
			 _Origin = pt.get<string>("Host"); //Origin
			 _Host = pt.get<string>("Host");
			 _ToFilename = pt.get<string>("ToFilename");
			 _HasExt = pt.get<string>("HasExt");

			//*response << "HTTP/1.1 200 OK\r\n"
			//					<< "Content-Length: " << name.length() << "\r\n\r\n"
			//					<< name;
		}
		catch(const exception &e) {
			cout << "bad req: " << e.what() << endl;
			response->write(SimpleWeb::StatusCode::client_error_bad_request, e.what());
		}

		cout << "Req: http://" << _Host << _Uri; cout.flush();
		
		HttpClient client(_Host);
		client.config.timeout = 10;

		CaseInsensitiveMultimap newHeader;

		if (!_Cookie.empty()) newHeader.insert(std::make_pair<string,string>("Cookie", _Cookie.c_str()));
		if (!_Referer.empty()) newHeader.insert(std::make_pair<string,string>("Referer", _Referer.c_str()));
		if (!_Origin.empty()) newHeader.insert(std::make_pair<string,string>("Origin", _Origin.c_str()));
		if (!_Host.empty()) newHeader.insert(std::make_pair<string,string>("Host", _Host.c_str()));

		auto r1 = client.request("GET", _Uri, "", newHeader);
		//cout << r1->content.rdbuf() << endl;
		cout << "\t" << r1->status_code << endl; cout.flush();

		if (r1->status_code.find("200") != string::npos)
		{
			string contentType = "";
			if (r1->header.find("Content-Type") != r1->header.end())
				contentType = r1->header.find("Content-Type")->second;
			
			string fnameExt = "";
			if (_HasExt == "true")
			{
				fnameExt = ".noext";
				if (!contentType.empty())
				{
					fnameExt = ".";
					fnameExt.append(contentType.substr(contentType.find_last_of('/') + 1, string::npos));
					if (fnameExt.find_last_of(';') != string::npos)
						fnameExt.pop_back();
				}
			}

			string fname;
			{
				std::stringstream ss;
				ss << _ToFilename << fnameExt;
				fname = ss.str();
			}
			
			//cout << fname;return;

			ofstream ofile(fname);
			std::copy(istreambuf_iterator<char>(r1->content), istreambuf_iterator<char>(), ostreambuf_iterator<char>(ofile));
			ofile.close();

			stringstream ssRes;
			ssRes << "{\"originUri\":\"http://" << _Host << _Uri << "\","
					<< "\"contentType\":\"" << contentType << "\","
					<< "\"fname\":\"" << fname << "\"}";
			ssRes.seekp(0, ios::end);

			*response << "HTTP/1.1 200 OK\r\nContent-Type:text/json; charset=UTF-8\r\nContent-Length: " << ssRes.tellp() << "\r\n\r\n" << ssRes.rdbuf();

		}
		else
		{
			*response << "HTTP/1.1 500 Internal Server Error\r\n\r\n";
		}
		
		//client.io_service->run();
		
	};


	server.resource["^/b64img$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {

		string _ToFilename;
		string _Base64;

		try {
			ptree pt;
			read_json(request->content, pt);

			 _ToFilename = pt.get<string>("ToFilename");
			 _Base64 = pt.get<string>("Base64");
		}
		catch(const exception &e) {
			cout << "bad req: " << e.what() << endl;
			response->write(SimpleWeb::StatusCode::client_error_bad_request, e.what());
		}

		cout << "Write base64 to file: " << _ToFilename << std::endl; cout.flush();

		std::ofstream ofs(_ToFilename.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

		typedef boost::archive::iterators::transform_width<boost::archive::iterators::binary_from_base64<string::const_iterator>, 8, 6> Base64DecodeIterator;
		copy(Base64DecodeIterator(_Base64.begin() + strlen("data:image/png;base64,")) , Base64DecodeIterator(_Base64.end()), ostream_iterator<char>(ofs));

		ofs.close();

		stringstream ssRes;
        ssRes << "{"
                << "\"fsize\":" << filesize(_ToFilename.c_str()) << ","
                << "\"fname\":\"" << _ToFilename << "\""
				<< "}";
		ssRes.seekp(0, ios::end);

		*response << "HTTP/1.1 200 OK\r\nContent-Type:text/json; charset=UTF-8\r\nContent-Length: " << ssRes.tellp() << "\r\n\r\n" << ssRes.rdbuf();
	};
	
	
	
	
	server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
		// Handle errors here
	};

	thread server_thread([&server]() {
	// Start server
	server.start();
	});

	server_thread.join();
}
