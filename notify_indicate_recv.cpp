// g++ -std=gnu++11 gatt_write.cpp -I. -I/home/kondo/work/bluez/attrib -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/home/kondo/work/bluez -o gatt_write -L/home/kondo/work/bluez/attrib/.libs -lgatt -L/home/kondo/work/bluez/lib/.libs -lbluetooth-internal -lglib-2.0


#include <glib.h>


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

#include <boost/format.hpp>

static GIOChannel *iochannel = NULL;

static GMainLoop *event_loop;

struct user_data_t {
	int handle;
	uint16_t value;
	uint16_t mtu;
	GAttrib* attrib;
	GIOChannel* chan;
};

static void disconnect(GAttrib* attrib, GIOChannel* chan)
{
	g_attrib_unref(attrib);

	g_io_channel_shutdown(chan, FALSE, NULL);
	g_io_channel_unref(chan);
}

static void notify_cb(
	const uint8_t *pdu,
	uint16_t len,
	gpointer user_data) {
	if (pdu[0] != ATT_OP_HANDLE_NOTIFY) {
		std::cout << "Invalid opecode for notify. pdu[0]:" << std::hex << static_cast<unsigned int>(pdu[0]) << std::endl;
		return;
	}
	uint16_t handle = (pdu[2] << 8) | pdu[1];
	std::cout << (boost::format("Notify from %04x length:%d") % handle % len) << std::endl;
	for (std::size_t index = 3, address = 0; index < len; ++index, ++address) {
		if (address % 16 == 0) {
			std::cout << std::endl;
			std::cout << (boost::format("%08x:") % address);
		}
		if (address % 8 == 0) {
			std::cout << ' ';
		}
		std::cout << (boost::format("%02x ") % static_cast<unsigned int>(pdu[index]));
	}
	std::cout << std::endl;
}

static void indicate_cb(
	const uint8_t *pdu,
	uint16_t len,
	gpointer user_data) {
	if (pdu[0] != ATT_OP_HANDLE_IND) {
		std::cout << "Invalid opecode for indicate. pdu[0]:" << std::hex << static_cast<unsigned int>(pdu[0]) << std::endl;
		return;
	}
	uint16_t handle = (pdu[2] << 8) | pdu[1];
	std::cout << (boost::format("Indicate from %04x length:%d") % handle % len) << std::endl;
	for (std::size_t index = 3, address = 0; index < len; ++index, ++address) {
		if (address % 16 == 0) {
			std::cout << std::endl;
			std::cout << (boost::format("%08x:") % address);
		}
		if (address % 8 == 0) {
			std::cout << ' ';
		}
		std::cout << (boost::format("%02x ") % static_cast<unsigned int>(pdu[index]));
	}
	std::cout << std::endl;

	size_t plen;
	user_data_t* ud = static_cast<user_data_t*>(user_data);
	uint8_t* opdu = g_attrib_get_buffer(ud->attrib, &plen);
	uint16_t olen = enc_confirmation(opdu, plen);
	if (olen > 0)
		g_attrib_send(ud->attrib, 0, opdu, olen, NULL, NULL, NULL);
}



static void gatt_write_char_cb(
	guint8 status,
	guint8 const* pdu,
	guint16 plen,
	gpointer user_data) {
	if (status != 0) {
		std::cout << "Characteristic Write Request failed: " << att_ecode2str(status) << std::endl;
		goto done;
	}

	if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
		std::cout << "Protocol error" << std::endl;
		goto done;
	}

	std::cout << "Characteristic value was written successfully" << std::endl;
	return;
done:
	user_data_t* ud = static_cast<user_data_t*>(user_data);
	disconnect(ud->attrib, ud->chan);
	g_main_loop_quit(event_loop);
}

static void exchange_mtu_cb(
	guint8 status,
	guint8 const* pdu,
	guint16 plen,
	gpointer user_data)
{
	uint16_t mtu;
	user_data_t* ud = static_cast<user_data_t*>(user_data);

	if (status != 0) {
		std::cout << "Exchange MTU Request failed: " << att_ecode2str(status) << std::endl;
		disconnect(ud->attrib, ud->chan);
		g_main_loop_quit(event_loop);
		return;
	}

	if (!dec_mtu_resp(pdu, plen, &mtu)) {
		std::cout << "Protocol error" << std::endl;
		disconnect(ud->attrib, ud->chan);
		g_main_loop_quit(event_loop);
		return;
	}

	mtu = std::min(mtu, ud->mtu);
	/* Set new value for MTU in client */
	if (g_attrib_set_mtu(ud->attrib, mtu)) {
		std::cout << "MTU was exchanged successfully:" << mtu << std::endl;
		gatt_write_char(
			ud->attrib,
			ud->handle,
			reinterpret_cast<uint8_t const*>(&ud->value),
			sizeof(ud->value),
			gatt_write_char_cb,
			nullptr);
	}
	else {
		std::cout << "Error exchanging MTU" << std::endl;
		disconnect(ud->attrib, ud->chan);
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
	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
						notify_cb, ud, NULL);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
						indicate_cb, ud, NULL);

	if (ud->mtu < ATT_DEFAULT_LE_MTU) {
		std::cout << "Invalid value. Minimum MTU size is " << ATT_DEFAULT_LE_MTU;
		disconnect(ud->attrib, ud->chan);
		g_main_loop_quit(event_loop);
		return;
	}
	ud->attrib = attrib;
	gatt_exchange_mtu(attrib, ud->mtu, exchange_mtu_cb, ud);
}

//          0    1        2      3     4   5
// gatt_write hci0 destaddr handle value mtu
int main(int argc, char *argv[])
{
	if (argc != 5 && argc != 6) {
		std::cout << "Usage: " << argv[0] << " adapter destaddr handle value [mtu]" << std::endl;
		std::cout << "e.g.): " << argv[0] << " hci0 01:23:45:67:89:AB 0x000c 0x0300 100" << std::endl;
		std::cout << "value: " << argv[0] << " 0x0100: start notify, 0x0200 start indicate, 0x0300 both" << std::endl;
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
	uint16_t value = std::strtol(argv[4], nullptr, 0);

	uint8_t dest_type = BDADDR_LE_PUBLIC; // OR BDADDR_RANDOM
	BtIOSecLevel sec = BT_IO_SEC_LOW;  // OR BT_IO_SEC_HIGH, BT_IO_SEC_LOW

	user_data_t ud { handle, value, ATT_DEFAULT_LE_MTU };
	if (argc == 6) {
		ud.mtu = std::strtol(argv[5], nullptr, 0);
	}
	ud.chan = bt_io_connect(
		connect_cb, &ud, NULL, &gerr,
		BT_IO_OPT_SOURCE_BDADDR, &sba,
		BT_IO_OPT_SOURCE_TYPE, BDADDR_LE_PUBLIC,
		BT_IO_OPT_DEST_BDADDR, &dba,
		BT_IO_OPT_DEST_TYPE, dest_type,
		BT_IO_OPT_CID, ATT_CID,
		BT_IO_OPT_SEC_LEVEL, sec,
		BT_IO_OPT_INVALID);

	if (ud.chan == NULL) {
		std::cout <<  gerr->message << std::endl;
		exit(EXIT_FAILURE);
	}

	event_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(event_loop);
	g_main_loop_unref(event_loop);
	exit(EXIT_SUCCESS);
}
