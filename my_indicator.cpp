// g++ -std=c++11 my_indicator.cpp -I. -I/usr/include/dbus-c++-1 -ldbus-c++-1 -o indicator

#include "my_indicator.h"
#include "my_confirm.h"

#include <signal.h>
#include <iostream>
#include <thread>

DBus::BusDispatcher dispatcher;

class my_app:
	public org::myapp::server_adaptor,
	public org::bluez::my_proxy,
	public DBus::ObjectAdaptor,
	public DBus::ObjectProxy

{
public:
	my_app(DBus::Connection &send_conn, const char *send_path, const char *send_name, DBus::Connection &recv_conn, const char *recv_path)
		:DBus::ObjectAdaptor(recv_conn, recv_path)
		,DBus::ObjectProxy(send_conn, send_path, send_name) {

		std::size_t mtu = GetMtu();
		std::cout << "MTU: " << mtu << std::endl;
		std::size_t header = sizeof(std::uint16_t) + sizeof(std::uint8_t);
		unit_ = mtu - header;
	}
	void confirm() override {
		std::cout << "confirm received. rest: " << rest_ << std::endl;
		thread_.join();
		if (rest_ == 0) {
			dispatcher.leave();
			return;
		}
		// To avoid dbus return message's deadlock.
		thread_ = std::thread(&my_app::indicate_imp, this);
	}
	void indicate(std::vector<std::uint8_t> const& v) {
		it_ = v.cbegin();
		end_ = v.cend();
		rest_ = std::distance(it_, end_);
		thread_ = std::thread(&my_app::indicate_imp, this);
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
	std::thread thread_;
};

void niam(int sig)
{
	dispatcher.leave();
}

int main() {
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection send_conn = DBus::Connection::SystemBus();
	DBus::Connection recv_conn = DBus::Connection::SystemBus();

	my_app ma(send_conn, "/org/bluez/my", "org.bluez",
			  recv_conn, "/org/myapp/server");
	recv_conn.request_name("org.myapp");

	std::vector<std::uint8_t> v;
	// prepare
	for (int i = 0; i < 256; ++i) {
		v.push_back(i);
	}
	ma.indicate(v);
	dispatcher.enter();
}
