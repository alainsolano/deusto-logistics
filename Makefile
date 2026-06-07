# ============================================================
#  Deusto Logistics - Makefile
#  Cliente: C (sockets)   |  Servidor: C++ (sockets + sqlite3)
# ============================================================

CC      = gcc
CXX     = g++
CFLAGS  = -Wall -Wextra -g
CXXFLAGS= -Wall -Wextra -g -std=c++17
LIBS_SRV = -lsqlite3 -pthread

# --- Directorios ---------------------------------------------
CLIENTE_DIR  = cliente/src
SERVIDOR_DIR = servidor/src
COMP_DIR     = compartido

# --- Fuentes -------------------------------------------------
CLIENTE_SRCS = $(CLIENTE_DIR)/main.c    \
               $(CLIENTE_DIR)/menu.c    \
               $(CLIENTE_DIR)/usuario.c \
               $(CLIENTE_DIR)/config.c  \
               $(CLIENTE_DIR)/logger.c  \
               $(CLIENTE_DIR)/red.c

SERVIDOR_SRCS = $(SERVIDOR_DIR)/main.cpp \
                $(SERVIDOR_DIR)/db_manager.cpp

# --- Binarios ------------------------------------------------
CLIENTE_BIN  = cliente/cliente
SERVIDOR_BIN = servidor/servidor

# --- Objetos -------------------------------------------------
SERVIDOR_OBJS = $(SERVIDOR_DIR)/main.o \
                $(SERVIDOR_DIR)/db_manager.o

# ============================================================
.PHONY: all cliente servidor clean

all: cliente servidor

# --- Cliente (C) --------------------------------------------
cliente: $(CLIENTE_BIN)

$(CLIENTE_BIN): $(CLIENTE_SRCS)
	$(CC) $(CFLAGS) -I$(CLIENTE_DIR) -I$(COMP_DIR) $^ -o $@

# --- Servidor (C++) -----------------------------------------
servidor: $(SERVIDOR_BIN)

$(SERVIDOR_DIR)/main.o: $(SERVIDOR_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) -I$(COMP_DIR) -c $< -o $@

$(SERVIDOR_DIR)/db_manager.o: $(SERVIDOR_DIR)/db_manager.cpp
	$(CXX) $(CXXFLAGS) -I$(COMP_DIR) -c $< -o $@

$(SERVIDOR_BIN): $(SERVIDOR_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LIBS_SRV)

# --- Limpieza -----------------------------------------------
clean:
	rm -f $(CLIENTE_BIN) $(SERVIDOR_BIN) $(SERVIDOR_DIR)/*.o
