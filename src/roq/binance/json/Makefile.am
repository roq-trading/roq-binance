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
	account.cpp \
	agg_trade.cpp \
	balance.cpp \
	balance_update.cpp \
	balances_item.cpp \
	book_ticker.cpp \
	cancel_order.cpp \
	depth.cpp \
	depth_item.cpp \
	depth_update.cpp \
	error.cpp \
	event_type.cpp \
	exchange_info.cpp \
	execution_report.cpp \
	execution_type.cpp \
	field.cpp \
	fill.cpp \
	filter_type.cpp \
	listen_key.cpp \
	mini_ticker.cpp \
	new_order.cpp \
	order_status.cpp \
	order_type.cpp \
	outbound_account_info.cpp \
	outbound_account_position.cpp \
	result.cpp \
	side.cpp \
	stream.cpp \
	symbol.cpp \
	symbol_status.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trade.cpp
AUTOGEN_H = \
	account.h \
	agg_trade.h \
	balance.h \
	balance_update.h \
	balances_item.h \
	book_ticker.h \
	cancel_order.h \
	depth.h \
	depth_item.h \
	depth_update.h \
	error.h \
	event_type.h \
	exchange_info.h \
	execution_report.h \
	execution_type.h \
	field.h \
	fill.h \
	filter_type.h \
	listen_key.h \
	mini_ticker.h \
	new_order.h \
	order_status.h \
	order_type.h \
	outbound_account_info.h \
	outbound_account_position.h \
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
	market_stream_parser.cpp \
	user_stream_parser.cpp \
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
