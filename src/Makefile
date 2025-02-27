CXX = g++
CXX_FLAGS = -O3 -g
CMP_FLAGS = $(CXX_FLAGS) -c
EXE_FLAGS = $(CXX_FLAGS)

MONITOR := monitor
XFTP := xftp
EXECS := $(XFTP)
EXECS += $(MONITOR)

MONITOR_OBJS := monitor.o config_file.o string_split.o sqlite.o http.o ip.o
XFTP_OBJS := xftp_rk3588.o
XFTP_OBJS += config_file.o string_split.o sqlite.o http.o ip.o thread.o sps_parser.o
XFTP_OBJS += cbuf.o frame_cir_buff.o annotation_info.o
XFTP_OBJS += mpp_rknn.o mpp_decoder.o mpp_encoder.o postprocess.o

LIB_PATH := ../libs
LIBS := $(addprefix -L,$(LIB_PATH))
LIBS += -lcurl -lcjson -lsqlite3 -pthread -lm -lstdc++
LIBS += -lxftp_live -lrtsp_utils
LIBS += -lxttp_rtc
LIBS += -lhwid
LIBS += -lrknnrt -lrockchip_mpp -lrga

INC_PATH := ../include
INC_PATH += ../include/rk3588 ../include/rk3588/rga
INCS := $(addprefix -I,$(INC_PATH))

INSTALL_DIR = /usr/local/xt/bin


.PHONY: all clean

all: $(EXECS)

$(MONITOR): $(MONITOR_OBJS)
	$(CXX) $(EXE_FLAGS) -o $@ $^ $(LIBS)

$(XFTP): $(XFTP_OBJS)
	$(CXX) $(EXE_FLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CMP_FLAGS) $(INCS) -o $@ $<

%.o: %.cc
	$(CXX) $(CMP_FLAGS) $(INCS) -o $@ $<

clean:
	rm -f $(EXECS) *.o

install:
	cp -f $(EXECS) $(INSTALL_DIR)
