SUBDIRS = data .

CSC = mcs

CSFLAGS =					\
	-debug					\
	-target:library

ASSEMBLIES =					\
	$(FYRE_LIBS)				\
	-r:Mono.Posix				\
	-r:System.Drawing			\
	-r:$(top_builddir)/src/Fyre.exe

TARGET = HistogramImager.dll
TARGET_CSFILES = 				\
	HistogramImager.cs
TARGET_RESOURCES =				\
	-resource:data/HistogramImager.png

$(TARGET): $(TARGET_CSFILES) $(top_builddir)/src/Fyre.exe
	$(CSC) -out:$@ $(CSFLAGS) $(ASSEMBLIES) $(TARGET_CSFILES) $(TARGET_RESOURCES)

pluginsdir = $(pkglibdir)/Plugins
plugins_DATA =					\
	$(TARGET)

EXTRA_DIST =					\
	$(TARGET_CSFILES)

CLEANFILES =					\
	$(TARGET)				\
	$(TARGET).mdb
