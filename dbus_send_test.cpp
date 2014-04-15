// g++ -std=c++11 dbus_send_test.cpp -I. -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -ldbus-1

#include <dbus/dbus.h>
#include <iostream>

int main() {
	DBusError e;
	dbus_error_init(&e);
	DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &e);
	if (dbus_error_is_set(&e)) {
		std::cout << "name: " << e.name << std::endl;
		std::cout << "message: " << e.message << std::endl;
		return -1;
	}
	DBusMessage* msg = dbus_message_new_method_call(
		"org.myapp",
		"/org/myapp/server",
		"org.myapp.server",
		"read");
	dbus_error_init(&e);
	DBusMessage* rmsg = dbus_connection_send_with_reply_and_block(
		conn,
		msg,
		1000,
		&e);
	if (dbus_error_is_set(&e)) {
		std::cout << "name: " << e.name << std::endl;
		std::cout << "message: " << e.message << std::endl;
		return -1;
	}
}

