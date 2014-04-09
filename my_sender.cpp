#include "my_sender.h"
#include <signal.h>

class my_app:
	public org::bluez::my_proxy,
	public DBus::ObjectProxy
{
public:
	my_app(DBus::Connection &connection, const char *path, const char *name)
		:DBus::ObjectProxy(connection, path, name) {}
};


int main() {
	DBus::BusDispatcher dispatcher;
	DBus::default_dispatcher = &dispatcher;
	DBus::Connection conn = DBus::Connection::SystemBus();

	my_app ma(conn, "/org/bluez/my", "org.bluez");

	std::vector<std::uint8_t> v;
	for (int i = 0; i < 256; ++i) {
		v.push_back(i);
	}
	ma.MyNotify(v);
}
