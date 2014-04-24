#include "my_server.h"

#include <signal.h>
#include <iostream>
#include <iterator>
#include <fstream>

#include <boost/format.hpp>

#include <msgpack.hpp>

class my_app:
	public org::myapp::server_adaptor,
	public DBus::ObjectAdaptor
{
	struct fname_content {
		std::string fname;
		std::vector<uint8_t> content;
		MSGPACK_DEFINE(fname, content);
	};
public:
	my_app(DBus::Connection &connection, const char *path, char const* fname)
		:DBus::ObjectAdaptor(connection, path), fname_(fname) {}
private:
	void write(const std::vector<uint8_t>& bytes) override {
		std::cout << "write called" << std::endl;

		msgpack::unpacked result;
		msgpack::unpack(result, reinterpret_cast<char const*>(bytes.data()), bytes.size());
		fname_content fc;
		result.get().convert(fc);
		std::ofstream ofs(fc.fname, std::ofstream::out | std::ifstream::binary);
		ofs.write(reinterpret_cast<char const*>(fc.content.data()), fc.content.size());
	}

	std::vector<uint8_t> read() override {
		std::cout << "read called" << std::endl;

		std::ifstream ifs(fname_, std::ifstream::in | std::ifstream::binary);
		std::vector<std::uint8_t> content = std::vector<std::uint8_t>(
			std::istreambuf_iterator<char>(ifs),
			std::istreambuf_iterator<char>());

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, fname_content { fname_, content} );

		std::vector<uint8_t> ret(sbuf.data(), sbuf.data() + sbuf.size());
		return ret;
	}
private:
	char const* fname_;
};

DBus::BusDispatcher dispatcher;

void niam(int sig)
{
	dispatcher.leave();
}


int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " filename" << std::endl;
		return -1;
	}
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;
	DBus::Connection conn = DBus::Connection::SystemBus();
	conn.request_name("org.myapp");
	my_app ma(conn, "/org/myapp/server", argv[1]);
	dispatcher.enter();
}
