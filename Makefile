JOBS := $(shell < /proc/cpuinfo grep processor | wc -l)
JOBS ?= 2

CPPFLAGS += -I"$(PWD)/deps/include/" -I"$(PWD)/vendor/rapidjson/include/"
CXXFLAGS ?= -O2
CXXFLAGS += -std=c++11 -Wall -Wpedantic -Wextra

.PHONY: all
all: json2yaml yaml2json

json2yaml: json2yaml.o deps/lib/libyaml-cpp.a
	$(CXX) $(LDFLAGS) json2yaml.o deps/lib/libyaml-cpp.a -o json2yaml

yaml2json: yaml2json.o deps/lib/libyaml-cpp.a
	$(CXX) $(LDFLAGS) yaml2json.o deps/lib/libyaml-cpp.a -o yaml2json

json2yaml.o: deps/include/yaml-cpp/
yaml2json.o: deps/include/yaml-cpp/

deps/include/yaml-cpp/ deps/lib/libyaml-cpp.a:
	mkdir -p "$(PWD)/"deps/build/yaml-cpp &&     \
	  cd deps/build/yaml-cpp &&                  \
		cmake -DCMAKE_INSTALL_PREFIX="$(PWD)/deps" -DYAML_CPP_BUILD_TOOLS=false \
		      "$(PWD)/vendor/yaml-cpp" &&          \
		make install -j "$(JOBS)"

.PHONY: clean clean-deps

clean:
	rm -f yaml2json json2yaml json2yaml.o yaml2json.o:

clean-deps:
	rm -R deps/
