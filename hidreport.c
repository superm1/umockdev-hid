#include <gusb.h>
#include <glib.h>
#include <umockdev.h>

#define HUB_CMD_READ_DATA 0xC0
#define HUB_EXT_READ_STATUS 0x09
#define HID_MAX_RETRIES 15
#define HIDI2C_TRANSACTION_TIMEOUT 2000
#define HID_REPORT_SET	0x09
#define HID_REPORT_GET	0x01

static gboolean
hid_set_report (GUsbDevice *usb_device,
			guint8 *outbuffer,
			GError **error)
{
	gboolean ret;
	gsize actual_len = 0;
	for (gint i = 0; i <= HID_MAX_RETRIES; i++) {
		g_autoptr(GError) error_local = NULL;
		ret = g_usb_device_control_transfer (
		    usb_device, G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE,
		    G_USB_DEVICE_REQUEST_TYPE_CLASS,
		    G_USB_DEVICE_RECIPIENT_INTERFACE, HID_REPORT_SET, 0x0200,
		    0x0000, outbuffer, 192, &actual_len,
		    HIDI2C_TRANSACTION_TIMEOUT, NULL, &error_local);
		if (ret)
			break;
		if (i < HID_MAX_RETRIES) {
			g_debug ("%d: control transfer failed: %s, retrying", i,
				 error_local->message);
			sleep (1);
		} else {
			g_prefix_error (&error_local,
					"control transfer failed: ");
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				     "control transfer failed: %s",
				     error_local->message);
			return FALSE;
		}
	}
	if (actual_len != 192) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     "only wrote %" G_GSIZE_FORMAT "bytes", actual_len);
		return FALSE;
	}
	return TRUE;
}

static gboolean
hid_get_report (GUsbDevice *usb_device,
			guint8 *inbuffer,
			GError **error)
{
	gboolean ret;
	gsize actual_len = 0;
	for (gint i = 0; i <= HID_MAX_RETRIES; i++) {
		g_autoptr(GError) error_local = NULL;
		ret = g_usb_device_control_transfer (
		    usb_device, G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
		    G_USB_DEVICE_REQUEST_TYPE_CLASS,
		    G_USB_DEVICE_RECIPIENT_INTERFACE, HID_REPORT_GET, 0x0100,
		    0x0000, inbuffer, 192, &actual_len,
		    HIDI2C_TRANSACTION_TIMEOUT, NULL, &error_local);
		if (ret)
			break;
		if (i < HID_MAX_RETRIES)
			g_debug ("%d: control transfer failed: %s, retrying", i,
				 error_local->message);
		else {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				     "control transfer failed: %s",
				     error_local->message);
			return FALSE;
		}
	}
	if (actual_len != 192) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     "only read %" G_GSIZE_FORMAT "bytes", actual_len);
		return FALSE;
	}
	return TRUE;
}


gboolean
hid_get_hub_version (GUsbDevice *usb_device,
		     guint8 *major,
		     guint8 *minor,
		     GError **error)
{
	guint8 cmd_buffer [256] = {0};
	cmd_buffer[0] = HUB_CMD_READ_DATA;
	cmd_buffer[1] = HUB_EXT_READ_STATUS;
	cmd_buffer[6] = 12;

	if (!hid_set_report (usb_device, (guint8 *) &cmd_buffer,
			     error)) {
		g_prefix_error (error, "failed to set report: ");
		return FALSE;
	}
	if (!hid_get_report (usb_device, cmd_buffer + 64, error)) {
		g_prefix_error (error, "failed to get report: ");
		return FALSE;
	}
	*major = cmd_buffer[74];
	*minor = cmd_buffer[75];

	return TRUE;
}

gboolean
open_device (GUsbDevice *usb_device, GError **error) {
	/* open device and clear status */
	if (!g_usb_device_open (usb_device, error)) {
		g_prefix_error (error, "failed to open: ");
		return FALSE;
	}
	if (!umockdev_in_mock_environment ()) {
		if (!g_usb_device_claim_interface (usb_device, 0, /* HID */
						   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER, error)) {
						   g_prefix_error (error, "failed to claim");
			return FALSE;
		}
	}

	return TRUE;
}

int main (int argc, char *argv[])
{
	g_autoptr(GUsbContext) usb_ctx = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) devices = NULL;
	GUsbDevice *usb_device = NULL;
	guint16 idx;
	guint8 major;
	guint8 minor;

	usb_ctx = g_usb_context_new (&error);
	g_assert_nonnull (usb_ctx);
	devices = g_usb_context_get_devices (usb_ctx);
	g_assert_nonnull (devices);

	if (argc != 2) {
		g_print ("usage: ./hidreport <pid>\n");
		return 1;
	}
	idx = g_ascii_strtoll (argv[1], NULL, 16);
	for (guint i = 0; i < devices->len; i++)  {
		usb_device = g_ptr_array_index (devices, i);
		if (g_usb_device_get_vid (usb_device) != 0x413c)
			continue;
		if (idx == g_usb_device_get_pid (usb_device)) {
			g_print ("Querying %x:%x\n", g_usb_device_get_vid (usb_device), idx);
			if (!open_device (usb_device, &error)) {
				g_print ("%s\n", error->message);
				return 1;
			}
			if (!hid_get_hub_version (usb_device, &major, &minor, &error)) {
				g_print ("Failed: %s\n", error->message);
				return 1;
			}
			g_print ("Major: %x, Minor: %x\n", major, minor);
			if (!g_usb_device_close (usb_device, &error)) {
				g_print("failed to close: %s\n", error->message);
				return 1;
			}
		}
		usb_device = NULL;
	}

	return 0;
}
