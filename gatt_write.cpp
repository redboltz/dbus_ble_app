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

static GIOChannel *iochannel = NULL;

int const handle = 0x000b;

static GMainLoop *event_loop;

#if TK_TMP

disconnect() {
	g_io_channel_shutdown(iochannel, FALSE, NULL);
	g_io_channel_unref(iochannel);
	iochannel = NULL;

	set_state(STATE_DISCONNECTED);
}

static void exchange_mtu_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint16_t mtu;

	if (status != 0) {
		std::cout << "Exchange MTU Request failed: " << att_ecode2str(status) << std::endl;
		return;
	}

	if (!dec_mtu_resp(pdu, plen, &mtu)) {
		std::cout << "Protocol error" << std::endl;
		return;
	}

	mtu = MIN(mtu, opt_mtu);
	/* Set new value for MTU in client */
	if (g_attrib_set_mtu(attrib, mtu))
		rl_printf("MTU was exchanged successfully: %d\n", mtu);
	else
		std::cout << "Error exchanging MTU" << std::endl; 
}

static void cmd_mtu(int argcp, char **argvp)
{
	if (conn_state != STATE_CONNECTED) {
		failed("Disconnected\n");
		return;
	}

	if (opt_psm) {
		failed("Operation is only available for LE transport.\n");
		return;
	}

	if (argcp < 2) {
		rl_printf("Usage: mtu <value>\n");
		return;
	}

	if (opt_mtu) {
		failed("MTU exchange can only occur once per connection.\n");
		return;
	}

	errno = 0;
	opt_mtu = strtoll(argvp[1], NULL, 0);
	if (errno != 0 || opt_mtu < ATT_DEFAULT_LE_MTU) {
		std::cout << "Invalid value. Minimum MTU size is " << ATT_DEFAULT_LE_MTU;
		return;
	}

	gatt_exchange_mtu(attrib, opt_mtu, exchange_mtu_cb, NULL);
}

#endif

static void gatt_write_char_cb(
	guint8 status,
	guint8 const* pdu,
	guint16 plen,
	gpointer user_data) {
	if (status != 0) {
		std::cout << "Characteristic Write Request failed: " << att_ecode2str(static_cast<uint8_t>(status)) << std::endl;
		goto done;
	}

	if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
		std::cout << "Protocol error" << std::endl;
		goto done;
	}

	std::cout << "Characteristic value was written successfully" << std::endl;
done:
	g_main_loop_quit(event_loop);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
	if (err) {
		std::cout <<  err->message << std::endl;
		return;
	}
	std::vector<std::uint8_t>* content = static_cast<std::vector<std::uint8_t>*>(user_data);
	GAttrib* attrib = g_attrib_new(io);
	gatt_write_char(
		attrib,
		handle,
		content->data(),
		content->size(),
		gatt_write_char_cb,
		NULL);
}

//          0    1        2        3
// gatt_write hci0 destaddr filename
int main(int argc, char *argv[])
{
	if (argc != 4) {
		std::cout << "Usage: " << argv[0] << " adapter destaddr filename" << std::endl;
		std::cout << "e.g.): " << argv[0] << " hci0 01:23:45:67:89:AB test.txt" << std::endl;
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

	char const* fn = argv[3];
	std::ifstream ifs(fn, std::ifstream::in | std::ifstream::binary);
	std::vector<std::uint8_t> content = std::vector<std::uint8_t>(
		std::istreambuf_iterator<char>(ifs),
		std::istreambuf_iterator<char>());

	uint8_t dest_type = BDADDR_LE_PUBLIC; // OR BDADDR_RANDOM
	BtIOSecLevel sec = BT_IO_SEC_LOW;  // OR BT_IO_SEC_HIGH, BT_IO_SEC_LOW

	GIOChannel* chan = bt_io_connect(
		connect_cb, &content, NULL, &gerr,
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
