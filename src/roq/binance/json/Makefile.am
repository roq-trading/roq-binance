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
	agg_trade.cpp \
	book_ticker.cpp \
	depth.cpp \
	depth_item.cpp \
	depth_update.cpp \
	error.cpp \
	event_type.cpp \
	exchange_info.cpp \
	execution_type.cpp \
	field.cpp \
	filter_type.cpp \
	mini_ticker.cpp \
	order_status.cpp \
	order_type.cpp \
	order_update.cpp \
	result.cpp \
	side.cpp \
	stream.cpp \
	symbol.cpp \
	symbol_status.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trade.cpp
AUTOGEN_H = \
	agg_trade.h \
	book_ticker.h \
	depth.h \
	depth_item.h \
	depth_update.h \
	error.h \
	event_type.h \
	exchange_info.h \
	execution_type.h \
	field.h \
	filter_type.h \
	mini_ticker.h \
	order_status.h \
	order_type.h \
	order_update.h \
	result.h \
	side.h \
	stream.h \
	symbol.h \
	symbol_status.h \
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
