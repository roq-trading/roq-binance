TESTS = roq-binance-benchmark
bin_PROGRAMS = roq-binance-benchmark
roq_binance_benchmark_LDADD = \
	$(top_builddir)/src/roq/binance/json/libroq-binance-json.la \
	$(DATE_LIBS) \
	$(ROQ_API_LIBS) \
	$(ROQ_CORE_LIBS) \
	$(FMT_LIBS) \
	$(BENCHMARK_LIBS)  \
	$(RT_LIBS) \
	-lpthread
roq_binance_benchmark_CFLAGS = -pthread
roq_binance_benchmark_CPPFLAGS = \
	-I$(top_builddir)/src \
	$(ROQ_API_CFLAGS) \
	$(ROQ_CORE_CFLAGS) \
	$(BENCHMARK_CFLAGS) \
	$(FMT_CFLAGS) \
	$(DATE_CFLAGS)
nodist_roq_binance_benchmark_SOURCES = \
	main.cpp
