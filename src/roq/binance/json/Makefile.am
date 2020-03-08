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
	depth_update.cpp \
	depth_update_item.cpp \
	order_response.cpp \
	order_response_fill.cpp \
	side.cpp \
	status.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trade.cpp \
	type.cpp
AUTOGEN_H = \
	agg_trade.h \
	book_ticker.h \
	depth_update.h \
	depth_update_item.h \
	order_response.h \
	order_response_fill.h \
	side.h \
	status.h \
	ticker.h \
	time_in_force.h \
	trade.h \
	type.h
BUILT_SOURCES = $(AUTOGEN_CPP) $(AUTOGEN_H) .gitignore
CLEANFILES = $(BUILT_SOURCES)
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
