
all: xml2proto
	cat document.xml | ./xml2proto --proto=schema.proto --type=Address --output=text

xml2proto: xml2proto.cc
	clang++ -Wall -lprotobuf -lprotoc -lxml2 -I/usr/include/libxml2 -lpthread -o xml2proto ./xml2proto.cc

clean:
	\rm -rf xml2proto

