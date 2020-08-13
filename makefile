raytrace: raytrace.o 
	g++ raytrace.o -o raytrace 
raytrace.o: raytrace.cpp 
	g++ raytrace.cpp -c 


