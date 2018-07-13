hidreport: hidreport.c
	gcc -o hidreport hidreport.c `pkg-config --libs gusb glib-2.0 gio-2.0 umockdev-1.0 --cflags`

clean:
	rm -f hidreport
