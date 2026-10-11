CC = gcc

CLFAGS = 

LDFLAGS = -lm

TARGET = homework01

SRC_DIR = Homework-01
OUT_DIR = out

TARGET = $(OUT_DIR)/homework01

SRCS = $(SRC_DIR)/main.c
OBJS = $(OUT_DIR)/main.o


all: $(TARGET)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUT_DIR)

run: $(TARGET)
	out/homework01 Homework-01/sample.wav

.PHONY: all clean run

