// g++ -std=gnu++11 gatt_write.cpp -I. -I/home/kondo/work/bluez/attrib -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/home/kondo/work/bluez -o gatt_write -L/home/kondo/work/bluez/attrib/.libs -lgatt -L/home/kondo/work/bluez/lib/.libs -lbluetooth-internal -lglib-2.0


#include <glib.h>
#include <boost/format.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#include <lib/uuid.h>
#include <lib/hci.h>
#include <lib/hci_lib.h>
#include <gattrib.h>
#include <btio/btio.h>
#include <att.h>
#include <gatt.h>

#ifdef __cplusplus
}
#endif

#include <iostream>
#include <fstream>
#include <vector>

static GIOChannel *iochannel = NULL;

static GMainLoop *event_loop;

struct user_data_t {
	int handle;
	uint16_t mtu;
	GAttrib* attrib;
};


static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint8_t value[plen];
	ssize_t vlen;
	int i;
	GString *s;

	if (status != 0) {
		std::cout << "Characteristic value/descriptor read failed: \n" << att_ecode2str(status);
		goto done;
	}

	vlen = dec_read_resp(pdu, plen, value, sizeof(value));
	if (vlen < 0) {
		std::cout << "Protocol error" << std::endl;
		goto done;
	}

	std::cout << "Characteristic value/descriptor: ";

	for (std::size_t index = 0; index < vlen; ++index) {
		if (index % 16 == 0) {
			std::cout << std::endl;
			std::cout << (boost::format("%08x:") % index);
		}
		if (index % 8 == 0) {
			std::cout << ' ';
		}
		std::cout << (boost::format("%02x ") % static_cast<unsigned int>(value[index]));
	}
	std::cout << std::endl;
done:
	g_main_loop_quit(event_loop);
}

static void exchange_mtu_cb(
	guint8 status,
	guint8 const* pdu,
	guint16 plen,
	gpointer user_data)
{
	uint16_t mtu;

	if (status != 0) {
		std::cout << "Exchange MTU Request failed: " << att_ecode2str(status) << std::endl;
		g_main_loop_quit(event_loop);
		return;
	}

	if (!dec_mtu_resp(pdu, plen, &mtu)) {
		std::cout << "Protocol error" << std::endl;
		g_main_loop_quit(event_loop);
		return;
	}
	user_data_t* ud = static_cast<user_data_t*>(user_data);

	mtu = std::min(mtu, ud->mtu);
	/* Set new value for MTU in client */
	if (g_attrib_set_mtu(ud->attrib, mtu)) {
		std::cout << "MTU was exchanged successfully:" << mtu << std::endl;
		gatt_read_char(
			ud->attrib,
			ud->handle,
			char_read_cb,
			nullptr);
	}
	else {
		std::cout << "Error exchanging MTU" << std::endl;
		g_main_loop_quit(event_loop);
	}
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
	if (err) {
		std::cout <<  err->message << std::endl;
		return;
	}
	user_data_t* ud = static_cast<user_data_t*>(user_data);
	GAttrib* attrib = g_attrib_new(io);
	if (ud->mtu < ATT_DEFAULT_LE_MTU) {
		std::cout << "Invalid value. Minimum MTU size is " << ATT_DEFAULT_LE_MTU;
		g_main_loop_quit(event_loop);
		return;
	}
	ud->attrib = attrib;
	gatt_exchange_mtu(attrib, ud->mtu, exchange_mtu_cb, ud);
}

//          0    1        2      3   4
// gatt_write hci0 destaddr handle mtu
int main(int argc, char *argv[])
{
	if (argc != 4 && argc != 5) {
		std::cout << "Usage: " << argv[0] << " adapter destaddr handle [mtu]" << std::endl;
		std::cout << "e.g.): " << argv[0] << " hci0 01:23:45:67:89:AB 0x000b 100" << std::endl;
		exit(EXIT_FAILURE);
	}

	GError *gerr = NULL;

	char const* src = argv[1];
	bdaddr_t sba;
	if (!strncmp(src, "hci", 3))
		hci_devba(atoi(src + 3), &sba);
	else
		str2ba(src, &sba);

	char const* dst = argv[2];
	bdaddr_t dba;
	str2ba(dst, &dba);

	int handle = std::strtol(argv[3], nullptr, 0);

	uint8_t dest_type = BDADDR_LE_PUBLIC; // OR BDADDR_RANDOM
	BtIOSecLevel sec = BT_IO_SEC_LOW;  // OR BT_IO_SEC_HIGH, BT_IO_SEC_LOW

	user_data_t ud { handle , ATT_DEFAULT_LE_MTU };
	if (argc == 5) {
		ud.mtu = std::strtol(argv[4], nullptr, 0);
	}
	GIOChannel* chan = bt_io_connect(
		connect_cb, &ud, NULL, &gerr,
		BT_IO_OPT_SOURCE_BDADDR, &sba,
		BT_IO_OPT_SOURCE_TYPE, BDADDR_LE_PUBLIC,
		BT_IO_OPT_DEST_BDADDR, &dba,
		BT_IO_OPT_DEST_TYPE, dest_type,
		BT_IO_OPT_CID, ATT_CID,
		BT_IO_OPT_SEC_LEVEL, sec,
		BT_IO_OPT_INVALID);

	if (chan == NULL) {
		std::cout <<  gerr->message << std::endl;
		exit(EXIT_FAILURE);
	}

	event_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(event_loop);
	g_main_loop_unref(event_loop);
	exit(EXIT_SUCCESS);
}
