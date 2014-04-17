#include "my_indicator.h"
#include "my_confirm.h"

#include <signal.h>
#include <iostream>

class my_app:
	public org::myapp::server_adaptor,
	public org::bluez::my_proxy,
	public DBus::ObjectAdaptor,
	public DBus::ObjectProxy

{
public:
	my_app(DBus::Connection &connection, const char *send_path, const char *send_name, const char *recv_path)
		:DBus::ObjectAdaptor(connection, recv_path)
		,DBus::ObjectProxy(connection, send_path, send_name) {

		std::size_t mtu = GetMtu();
		std::cout << "MTU: " << mtu << std::endl;
		std::size_t header = sizeof(std::uint16_t) + sizeof(std::uint8_t);
		unit_ = mtu - header;
	}
	void confirm() override {
		std::cout << "confirm received. rest: " << rest_ << std::endl;
		if (rest_ == 0) return;
		indicate_imp();
	}
	void indicate(std::vector<std::uint8_t> const& v) {
		it_ = v.cbegin();
		end_ = v.cend();
		rest_ = std::distance(it_, end_);
		indicate_imp();
	}
private:
	void indicate_imp() {
		if (rest_ > unit_) {
			MyIndicate(std::vector<std::uint8_t>(it_, it_ + unit_));
			it_ += unit_;
			rest_ -= unit_;
		}
		else {
			MyIndicate(std::vector<std::uint8_t>(it_, end_));
			it_ = end_;
			rest_ = 0;
		}
	}
	typename std::vector<std::uint8_t>::const_iterator it_;
	typename std::vector<std::uint8_t>::const_iterator end_;
	std::size_t unit_;
	std::size_t rest_;
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

	my_app ma(conn, "/org/bluez/my", "org.bluez", "/org/myapp/server");
	conn.request_name("org.myapp");

	std::vector<std::uint8_t> v;
	// prepare
	for (int i = 0; i < 256; ++i) {
		v.push_back(i);
	}
	ma.indicate(v);
	dispatcher.enter();
}
