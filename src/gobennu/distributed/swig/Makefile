clean:
	-rm endpoint_wrap.cxx
	-rm endpoint.go

endpoint.go: endpoint.i
	swig -c++ -go -intgosize 64 -cgo -package swig endpoint.i
