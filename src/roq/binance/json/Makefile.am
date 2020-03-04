noinst_LTLIBRARIES = libroq-binance-json.la
libroq_binance_json_la_LDFLAGS = \
	-static \
	$(LIBCONFIG_LIBS) \
	$(ROQ_LIBS)
libroq_binance_json_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBCONFIG_CFLAGS) \
	$(ROQ_CFLAGS)
BUILT_SOURCES = \
	agg_trade.cpp \
	agg_trade.h \
	book_ticker.cpp \
	book_ticker.h \
	depth_update.cpp \
	depth_update.h \
	depth_update_item.cpp \
	depth_update_item.h \
	order_response.cpp \
	order_response_fill.cpp \
	order_response_fill.h \
	order_response.h \
	side.cpp \
	side.h \
	status.cpp \
	status.h \
	ticker.cpp \
	ticker.h \
	time_in_force.cpp \
	time_in_force.h \
	trade.cpp \
	trade.h \
	type.cpp \
	type.h
CLEANFILES = $(BUILT_SOURCES)
libroq_binance_json_la_SOURCES = \
	agg_trade.cpp \
	book_ticker.cpp \
	depth_update.cpp \
	depth_update_item.cpp \
	order_response.cpp \
	order_response_fill.cpp \
	side.cpp \
	status.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trade.cpp \
	type.cpp \
	parser.cpp \
	utils.cpp

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
