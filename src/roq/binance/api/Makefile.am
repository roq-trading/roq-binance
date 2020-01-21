noinst_LTLIBRARIES = libroq-binance-api.la
libroq_binance_api_la_LDFLAGS = \
	-static \
	$(LIBCONFIG_LIBS) \
	$(ROQ_LIBS)
libroq_binance_api_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBCONFIG_CFLAGS) \
	$(ROQ_CFLAGS)
libroq_binance_api_la_SOURCES = \
	enums.cpp \
	utils.cpp
