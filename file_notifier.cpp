#include "my_notifier.h"
#include <signal.h>
#include <iostream>
#include <fstream>

#include <msgpack.hpp>

class my_app:
	public org::bluez::my_proxy,
	public DBus::ObjectProxy
{
public:
	my_app(DBus::Connection &connection, const char *path, const char *name)
		:DBus::ObjectProxy(connection, path, name) {}
};

struct fname_content {
	std::string fname;
	std::vector<uint8_t> content;
	MSGPACK_DEFINE(fname, content);
};

void notify(my_app& ma, std::vector<std::uint8_t> const& v) {
	std::size_t unit = ma.GetMaxPayloadSize();
	std::cout << "unit: " << unit << std::endl;
	auto b = v.cbegin();
	auto e = v.cend();
	auto rest = std::distance(b, e);
	while (rest > unit) {
		ma.MyNotify(std::vector<std::uint8_t>(b, b + unit));
		b += unit;
		rest -= unit;
	}
	if (rest) {
		ma.MyNotify(std::vector<std::uint8_t>(b, e));
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " filename" << std::endl;
		return -1;
	}
	DBus::BusDispatcher dispatcher;
	DBus::default_dispatcher = &dispatcher;
	DBus::Connection conn = DBus::Connection::SystemBus();

	my_app ma(conn, "/org/bluez/my", "org.bluez");

	std::ifstream ifs(argv[1], std::ifstream::in | std::ifstream::binary);
	std::vector<std::uint8_t> content = std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(ifs),
		std::istreambuf_iterator<char>());

	msgpack::sbuffer sbuf;
	msgpack::pack(sbuf, fname_content { argv[1], content} );

	std::vector<uint8_t> v(sbuf.data(), sbuf.data() + sbuf.size());
	notify(ma, v);
}
