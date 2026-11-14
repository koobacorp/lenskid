CXX ?= c++
LDL ?= -ldl
FLAGS = -std=c++17 -O2 -ffp-contract=off
test:
	$(CXX) $(FLAGS) -Isrc tests/test_looks.cpp -o build_test_looks && ./build_test_looks
	$(CXX) $(FLAGS) -fPIC -shared -fvisibility=hidden -Iinclude -Isrc src/lenskid.cpp -o build_LENSKID.ofx
	$(CXX) $(FLAGS) -Iinclude -Isrc tests/host_sim.cpp -o build_host_sim $(LDL) && ./build_host_sim ./build_LENSKID.ofx
luts:
	mkdir -p build/LUTs/LENSKID && $(CXX) $(FLAGS) -Isrc tools/make_luts.cpp -o build_make_luts && ./build_make_luts build/LUTs/LENSKID
stills:
	$(CXX) $(FLAGS) -Isrc tools/render_still.cpp -o build_render_still && python3 tools/make_stills.py ./build_render_still dist/stills
