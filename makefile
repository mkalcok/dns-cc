CC=gcc -w
PREFIX=/usr/bin
SETTINGS_DIR=/etc/dns-cc

compile: dns-cc.c compress.h nameres.h display.h
	$(CC) dns-cc.c -g -o dns-cc -lz -lpthread -lcares -lcurses

install: dns-cc
	cp dns-cc $(PREFIX)

	if [ ! -d $(SETTINGS_DIR) ] ;\
	then\
		mkdir $(SETTINGS_DIR);\
	fi;\

	cp dns.cfg $(SETTINGS_DIR)/dns.cfg


uninstall:
	if [ -e $(PREFIX)/dns-cc ] ; \
	then \
		rm $(PREFIX)/dns-cc ; \
	fi; \

	if [ -e $(SETTINGS_DIR)/dns.cfg ] ; \
	then \
		rm $(SETTINGS_DIR)/dns.cfg ; \
	fi; \

	if [ -d $(SETTINGS_DIR) ] ; \
	then \
		rmdir $(SETTINGS_DIR) ; \
	fi; \
