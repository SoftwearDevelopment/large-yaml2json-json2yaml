JOBS := $(shell < /proc/cpuinfo grep processor | wc -l)
JOBS ?= 2

CPPFLAGS += -I"$(PWD)/vendor/libyaml/include" -I"$(PWD)/vendor/rapidjson/include/"
CXXFLAGS ?= -O3
CXXFLAGS += -std=c++11 -Wall -Wpedantic -Wextra
LDFLAGS  += -std=c++11 -Wall -Wpedantic -Wextra

.PHONY: all
all: json2yaml yaml2json

j2y_objs = json2yaml.o yaml_scalar_parse.o
json2yaml: $(j2y_objs) deps/build/libyaml/libyaml.a
	$(CXX) $(LDFLAGS) $(j2y_objs) deps/build/libyaml/libyaml.a -o json2yaml

y2j_objs = yaml2json.o yaml_scalar_parse.o
yaml2json: $(y2j_objs) deps/build/libyaml/libyaml.a
	$(CXX) $(LDFLAGS) $(y2j_objs) deps/build/libyaml/libyaml.a -o yaml2json

$(y2j_objs) $(j2y_objs): yaml_scalar_parse.hpp

deps/build/libyaml/libyaml.a:
	mkdir -p "$(PWD)/"deps/build/libyaml         && \
	  cd deps/build/libyaml                      && \
		cmake -DCMAKE_INSTALL_PREFIX="$(PWD)/deps" "$(PWD)/vendor/libyaml" && \
		$(MAKE)

.PHONY: clean clean-deps

clean:
	rm -f yaml2json json2yaml $(j2y_objs) $(y2j_objs)

clean-deps:
	rm -Rf deps/
