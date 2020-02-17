SUBDIRS = json

bin_PROGRAMS = roq-binance
roq_binance_LDFLAGS = \
	$(PTHREAD_CFLAGS) \
	$(top_builddir)/src/roq/binance/json/libroq-binance-json.la \
	$(ROQ_SERVER_LIBS) \
	$(ROQ_CORE_LIBS) \
	$(ROQ_API_LIBS) \
	$(BROTLI_LIBS) \
	$(GFLAGS_LIBS) \
	$(CROSSGUID_LIBS) \
	$(HTTP_PARSER_LIBS) \
	$(FMT_LIBS) \
	$(LIBUCL_LIBS) \
	$(LIBEVENT_LIBS) \
	-levent_openssl \
	$(OPENSSL_LIBS) \
	$(CRYPTO_LIBS) \
	$(PTHREAD_LIBS) \
	$(RT_LIBS)
roq_binance_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(PTHREAD_CFLAGS) \
	$(GFLAGS_CFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(LIBEVENT_CFLAGS) \
	$(LIBUCL_CFLAGS) \
	$(FMT_CFLAGS) \
	$(HTTP_PARSER_CFLAGS) \
	$(BROTLI_CFLAGS) \
	$(ROQ_API_CFLAGS) \
	$(ROQ_CORE_CFLAGS) \
	$(ROQ_SERVER_CFLAGS)
roq_binance_SOURCES = \
	application.cpp \
	config.cpp \
	gateway.cpp \
	options.cpp \
	order_mapping.cpp \
	product.cpp \
	random.cpp \
	rest.cpp \
	websocket.cpp \
	main.cpp
