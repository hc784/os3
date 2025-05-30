# Compiler 변수 설정: g++ 사용
CXX=g++

# 컴파일러 플래그 설정: C++17 표준 사용
CXXFLAGS=-std=c++17 -g -Wno-deprecated-declarations

# 라이브러리 플래그
LDFLAGS = -lgtest -lgtest_main -pthread -lcrypto

# Source files
SOURCES=ftl_impl.cpp test_util.cpp test.cpp
OBJECTS=$(SOURCES:.cpp=.o)

# Executable name
EXECUTABLE=test

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)