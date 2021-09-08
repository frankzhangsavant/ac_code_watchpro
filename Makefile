TOPDIR := $(shell cd ../..;pwd)
include $(TOPDIR)/build/base.mk

LIBS := -lrt -ldl -lcommon
OBJS := watch_process.o
TARGET := watch_process

include $(BUILD_APP)
