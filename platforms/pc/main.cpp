#include <SDL3/SDL.h>
#include <filesystem>
#include <cstdio>
#include <stdexcept>
extern "C" int aurora_main(int,char**);
int main(int argc,char** argv){
    try {std::filesystem::current_path(SDL_GetBasePath());return aurora_main(argc,argv);}
    catch(const std::exception& e){std::fprintf(stderr,"ProjectLM-NX PC: %s\n",e.what());return 1;}
}
