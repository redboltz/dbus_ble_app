#include "my_server.h"
#include <signal.h>
#include <iostream>
#include <iterator>

class my_app:
	public org::myapp::server_adaptor,
	public DBus::ObjectAdaptor
{
public:
	my_app(DBus::Connection &connection, const char *path)
		:DBus::ObjectAdaptor(connection, path) {}
private:
	void write(const std::vector<uint8_t>& bytes) override {
		std::cout << "write called" << std::endl;
		std::cout << std::hex;
		std::copy(bytes.begin(), bytes.end(), std::ostream_iterator<int>(std::cout, " "));
		std::cout << std::endl;
	}
	std::vector<uint8_t> read() override {
		std::cout << "read called" << std::endl;
		std::vector<uint8_t> v;
		for (int i = 0; i < 256; ++i) v.push_back(i);
		v.push_back(0xde);
		v.push_back(0xad);
		v.push_back(0xbe);
		v.push_back(0xef);
		return v;
	}
};

DBus::BusDispatcher dispatcher;

void niam(int sig)
{
	dispatcher.leave();
}


int main() {
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;
	DBus::Connection conn = DBus::Connection::SystemBus();
	conn.request_name("org.myapp");
	my_app ma(conn, "/org/myapp/server");
	dispatcher.enter();
}
