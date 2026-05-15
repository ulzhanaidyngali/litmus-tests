# ── Linux / macOS ─────────────────────────────────────────
CXX      = g++
CXXFLAGS = -O2 -std=c++17 -pthread -march=native -Wall

TARGET = litmus

all: $(TARGET)

$(TARGET): main.cpp harness.h tests.h
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

# ── Windows (uncomment if using nmake) ────────────────────
# CXX = cl
# CXXFLAGS = /O2 /std:c++17 /EHsc
# TARGET = litmus.exe
# all:
# 	$(CXX) $(CXXFLAGS) main.cpp /Fe:$(TARGET)
