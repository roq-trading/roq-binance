noinst_LTLIBRARIES = libroq-binance-json.la
libroq_binance_json_la_LDFLAGS = \
	-static \
	$(LIBCONFIG_LIBS) \
	$(ROQ_LIBS)
libroq_binance_json_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBCONFIG_CFLAGS) \
	$(ROQ_CFLAGS)
AUTOGEN_CPP = \
	depth_update.cpp \
	depth_update_item.cpp \
	event_type.cpp \
	exchange_info.cpp \
	execution_type.cpp \
	order_status.cpp \
	order_type.cpp \
	order_update.cpp \
	side.cpp \
	symbol.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trade.cpp
AUTOGEN_H = \
	depth_update.h \
	depth_update_item.h \
	event_type.h \
	exchange_info.h \
	execution_type.h \
	order_status.h \
	order_type.h \
	order_update.h \
	side.h \
	symbol.h \
	ticker.h \
	time_in_force.h \
	trade.h
CLEANFILES = $(AUTOGEN_CPP) $(AUTOGEN_H)
BUILT_SOURCES = $(CLEANFILES) .gitignore
libroq_binance_json_la_SOURCES = \
	$(AUTOGEN_CPP) \
	parser.cpp \
	utils.cpp

.gitignore: Makefile.am
	echo $(AUTOGEN_CPP) $(AUTOGEN_H) | tr " " "\n" | sort > $@

.json.h:
	@abs_top_builddir@/autogen/autogen.py \
		--namespace=roq/binance/json \
		--output-type json \
		--file-type h \
		$< > $@

.json.cpp:
	@abs_top_builddir@/autogen/autogen.py \
		--namespace=roq/binance/json \
		--output-type json \
		--file-type cpp \
		$< > $@
