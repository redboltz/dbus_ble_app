#include "my_notifier.h"
#include <signal.h>
#include <iostream>

class my_app:
	public org::bluez::my_proxy,
	public DBus::ObjectProxy
{
public:
	my_app(DBus::Connection &connection, const char *path, const char *name)
		:DBus::ObjectProxy(connection, path, name) {}
};

void notify(my_app& ma, std::vector<std::uint8_t> const& v) {
	std::size_t mtu = ma.GetMtu();
	std::cout << "MTU: " << mtu << std::endl;
	std::size_t header = sizeof(std::uint16_t) + sizeof(std::uint8_t);
	std::size_t unit = mtu - header;
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

int main() {
	DBus::BusDispatcher dispatcher;
	DBus::default_dispatcher = &dispatcher;
	DBus::Connection conn = DBus::Connection::SystemBus();

	my_app ma(conn, "/org/bluez/my", "org.bluez");

	std::vector<std::uint8_t> v; // { 'k', 'o', 'n', 'd', 'o' };
	// prepare
	for (int i = 0; i < 256; ++i) {
		v.push_back(i);
	}
	notify(ma, v);
}
