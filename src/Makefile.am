AM_CPPFLAGS	=  -I$(top_srcdir)/include # @PGFCODEC_CFLAGS@

lib_LTLIBRARIES = libpgf.la
libpgf_la_LDFLAGS = -version-info @INTERFACE_VERSION@

libpgf_la_SOURCES = \
	Decoder.cpp \
	Encoder.cpp \
	PGFimage.cpp \
	PGFstream.cpp \
	Subband.cpp \
	WaveletTransform.cpp \
	\
	fse/entropy_common.c \
	fse/fse_compress.c \
	fse/fse_decompress.c \
	fse/hist.c \
	\
	lz4/lz4.c \
	lz4/lz4hc.c

