# ==============================================
# Makefile for QR Code Programs
# Targets:
#   make all     → Compile QRServer & QRClient
#   make clean   → Clean compiled binaries
# ==============================================

CC = gcc
CFLAGS = -Wall
SERVER = QRServer
CLIENT = QRClient

UNAME_S := $(shell uname -s)
ifeq ($(OS),Windows_NT)
	CFLAGS += -lws2_32
else ifeq ($(UNAME_S),Linux)
	CFLAGS += -lpthread
endif

all: $(SERVER) $(CLIENT)

$(SERVER): QRServer.c
	$(CC) $(CFLAGS) -o $(SERVER) QRServer.c

$(CLIENT): QRClient.c
	$(CC) $(CFLAGS) -o $(CLIENT) QRClient.c

clean:
	rm -f $(SERVER) $(CLIENT)

