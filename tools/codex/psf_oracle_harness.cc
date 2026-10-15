/* Test-only HLE PSF harness. Links a separately downloaded Audacious PSF core;
 * neither the emulator nor music data is linked into the native game port. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include "psx.h"
#include "cpuintrf.h"
#include "peops/spu.h"

int psf_refresh=60;
bool stop_flag=false;
/* PS2 paths are unreachable for this PSF1-only harness. */
uint32_t psf2_get_loadaddr(){return 0;}
void psf2_set_loadaddr(uint32_t){}
uint32_t psf2_load_file(const char*,uint8_t*,uint32_t){std::abort();}
uint32_t psf2_load_elf(uint8_t*,uint32_t){std::abort();}
static FILE *output;
static size_t remaining;
static void capture(const void *bytes,int size){
    if(size<=0)return;
    size_t n=(size_t)size;if(n>remaining)n=remaining;
    if(std::fwrite(bytes,1,n,output)!=n)std::abort();
    remaining-=n;
}
static uint32_t word(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|(uint32_t)p[3]<<24;}
static std::vector<uint8_t> upload(const char *path){
    FILE *f=std::fopen(path,"rb");if(!f)std::abort();
    std::fseek(f,0,SEEK_END);long n=std::ftell(f);std::rewind(f);
    if(n<2048||n>2099200)std::abort();
    std::vector<uint8_t> b((size_t)n);
    if(std::fread(b.data(),1,b.size(),f)!=b.size())std::abort();
    std::fclose(f);
    if(std::memcmp(b.data(),"PS-X EXE",8))std::abort();
    uint32_t address=word(b.data()+0x18)&0x1fffffff;
    if(address+b.size()-2048>2097152)std::abort();
    std::memcpy((uint8_t*)psx_ram+address,b.data()+2048,b.size()-2048);
    return b;
}
int main(int argc,char **argv){
    if(argc!=5)return 2;
    unsigned seconds=(unsigned)std::strtoul(argv[4],nullptr,10);
    if(!seconds||seconds>600)return 2;
    std::memset(psx_ram,0,2097152);
    auto driver=upload(argv[1]);upload(argv[2]);
    mips_init();mips_reset(nullptr);
    union cpuinfo info;
    info.i=word(driver.data()+0x10);mips_set_info(CPUINFO_INT_PC,&info);
    info.i=word(driver.data()+0x14);mips_set_info(CPUINFO_INT_REGISTER+MIPS_R28,&info);
    info.i=word(driver.data()+0x30);if(!info.i)info.i=0x801fff00;
    mips_set_info(CPUINFO_INT_REGISTER+MIPS_R29,&info);
    mips_set_info(CPUINFO_INT_REGISTER+MIPS_R30,&info);
    psx_hw_init();SPUinit();SPUopen();setendless(1);setlength(-1,0);
    output=std::fopen(argv[3],"wb");if(!output)return 1;
    std::string trace_path=std::string(argv[3])+".voices.csv";
    FILE *trace=std::fopen(trace_path.c_str(),"w");if(!trace)return 1;
    std::fprintf(trace,"frame,voice,vol_l,vol_r,pitch,start,adsr1,adsr2,envelope\n");
    remaining=(size_t)seconds*44100*4;
    mips_execute(5000);
    for(unsigned frame=0;remaining&&frame<(seconds+5)*60;++frame){
        for(int i=0;i<735;++i){psx_hw_slice();SPUasync(384,capture);}
        for(unsigned voice=0;voice<24;++voice){
            uint32_t base=0x1F801C00+voice*16;
            std::fprintf(trace,"%u,%u",frame,voice);
            for(unsigned r=0;r<7;++r)std::fprintf(trace,",%u",SPUreadRegister(base+r*2));
            std::fprintf(trace,"\n");
        }
        psx_hw_frame();
    }
    SPUclose();SPUshutdown();std::fclose(output);std::fclose(trace);
    return remaining?1:0;
}
