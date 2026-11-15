#include <sdk_compat.h>
#include <paf.h>
#include <psp2/audioout.h>
#include <psp2/avplayer.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/sysmodule.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sceavplayer_webmaf.h>
#include "video_player.h"
extern "C" int sceAvPlayerStreamCount(SceAvPlayerHandle handle);
using namespace paf;
namespace { Module *s_rgbaModule = NULL; }
VideoPlayer *VideoPlayer::s_active = NULL;
bool AvPlayerHandleValid(SceAvPlayerHandle handle) { const uint32_t bits=(uint32_t)handle; return bits!=0 && (bits&0xFF000000u)!=0x80000000u; }
VideoPlayer::VideoPlayer():m_player(0),m_startedUs(0),m_duration(0),m_haveFrame(false),m_paused(false),m_failed(false),m_speed(100),m_ready(false),m_started(false),m_eos(false),m_audioThread(-1),m_audioRun(false),m_fileFd(-1),m_fileSize(0){}
VideoPlayer::~VideoPlayer(){Close();}
void VideoPlayer::Log(const char *fmt,...){sceIoMkdir("ux0:data/FileManager",0777);SceUID fd=sceIoOpen("ux0:data/FileManager/video_diagnostic.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_APPEND,0777);if(fd<0)return;char line[384];va_list ap;va_start(ap,fmt);int n=vsnprintf(line,sizeof(line),fmt,ap);va_end(ap);if(n>0)sceIoWrite(fd,line,n<(int)sizeof(line)?n:(int)sizeof(line)-1);sceIoWrite(fd,"\r\n",2);sceIoClose(fd);}
void *VideoPlayer::Alloc(void*,uint32_t a,uint32_t s){if(a<16)a=16;return memalign(a,s);} void VideoPlayer::Free(void*,void*p){free(p);}
void *VideoPlayer::GpuAlloc(void*,uint32_t a,uint32_t s){if(a<SCE_KERNEL_256KiB)a=SCE_KERNEL_256KiB;s=(s+a-1)&~(a-1);SceKernelAllocMemBlockOpt b;memset(&b,0,sizeof(b));b.size=sizeof(b);b.attr=SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;b.alignment=a;GraphMem::Option o;memset(&o,0,sizeof(o));o.memblock_option=&b;o.gpu_map_attrib=SCE_GXM_MEMORY_ATTRIB_READ|SCE_GXM_MEMORY_ATTRIB_WRITE;return GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory,s,"FM::VideoDecode",&o);}
void VideoPlayer::GpuFree(void*,void*p){if(p)GraphMem::FreeOSMemory(GraphMem::DeviceType_VideoMemory,p);}
int VideoPlayer::FileOpen(void*a,const char*f){VideoPlayer*s=(VideoPlayer*)a;if(!s)return-1;s->m_fileFd=sceIoOpen(f,SCE_O_RDONLY,0);if(s->m_fileFd<0)return-1;SceOff z=sceIoLseek(s->m_fileFd,0,SCE_SEEK_END);if(z<0){FileClose(s);return-1;}s->m_fileSize=z;sceIoLseek(s->m_fileFd,0,SCE_SEEK_SET);return 0;}
int VideoPlayer::FileClose(void*a){VideoPlayer*s=(VideoPlayer*)a;if(!s)return-1;if(s->m_fileFd>=0)sceIoClose(s->m_fileFd);s->m_fileFd=-1;s->m_fileSize=0;return 0;}
int VideoPlayer::FileRead(void*a,uint8_t*b,uint64_t p,uint32_t n){VideoPlayer*s=(VideoPlayer*)a;if(!s||s->m_fileFd<0)return-1;if(sceIoLseek(s->m_fileFd,p,SCE_SEEK_SET)<0)return-1;return sceIoRead(s->m_fileFd,b,n);}
uint64_t VideoPlayer::FileSize(void*a){VideoPlayer*s=(VideoPlayer*)a;return s?s->m_fileSize:0;}
void VideoPlayer::PlayerEvent(void*a,int32_t e,int32_t,void*){VideoPlayer*s=(VideoPlayer*)a;if(!s)return;if(e==SCE_AVPLAYER_STATE_READY)s->m_ready=true;else if(e==SCE_AVPLAYER_STATE_PLAY)s->m_started=true;else if(e==SCE_AVPLAYER_STATE_EOS)s->m_eos=true;}
int VideoPlayer::AudioThread(SceSize,void*){VideoPlayer*s=s_active;int port=-1,rate0=0,ch0=0,grain0=0;while(s&&s->m_audioRun){SceAvPlayerFrameInfo f;memset(&f,0,sizeof(f));if(s->m_started&&AvPlayerHandleValid(s->m_player)&&sceAvPlayerGetAudioData(s->m_player,&f)&&f.pData){int rate=f.details.audio.sampleRate,ch=f.details.audio.channelCount,grain=ch>0?f.details.audio.size/(ch*2):0;if(rate!=rate0||ch!=ch0||grain!=grain0){if(port>=0)sceAudioOutReleasePort(port);port=-1;rate0=rate;ch0=ch;grain0=grain;if((ch==1||ch==2)&&rate>=8000&&rate<=48000&&grain>=64&&grain<=2048)port=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,grain,rate,ch==1?SCE_AUDIO_OUT_MODE_MONO:SCE_AUDIO_OUT_MODE_STEREO);s->Log("Audio rate=%d channels=%d grain=%d port=0x%08X",rate,ch,grain,port);}if(port>=0)sceAudioOutOutput(port,f.pData);}else sceKernelDelayThread(4000);}if(port>=0)sceAudioOutReleasePort(port);return sceKernelExitDeleteThread(0);}
void VideoPlayer::StartAudio(){if(m_audioRun)return;m_audioRun=true;m_audioThread=sceKernelCreateThread("FM::VideoAudio",AudioThread,0x10000100,0x4000,0,0,NULL);if(m_audioThread>=0)sceKernelStartThread(m_audioThread,0,NULL);else m_audioRun=false;}
void VideoPlayer::StopAudio(){m_audioRun=false;if(m_audioThread>=0){sceKernelWaitThreadEnd(m_audioThread,NULL,NULL);m_audioThread=-1;}}
void VideoPlayer::RefreshStreams(){m_duration=0;int c=sceAvPlayerStreamCount(m_player);for(int i=0;i<c&&i<32;i++){SceAvPlayerStreamInfo x;memset(&x,0,sizeof(x));if(sceAvPlayerGetStreamInfo(m_player,i,&x)>=0&&x.duration>m_duration)m_duration=x.duration;}Log("Ready streams=%d duration=%llu",c,(unsigned long long)m_duration);}
bool VideoPlayer::OpenDecoder(){SceAvPlayerInitData i;memset(&i,0,sizeof(i));i.memoryReplacement.objectPointer=this;i.memoryReplacement.allocate=Alloc;i.memoryReplacement.deallocate=Free;i.memoryReplacement.allocateTexture=GpuAlloc;i.memoryReplacement.deallocateTexture=GpuFree;i.fileReplacement.objectPointer=this;i.fileReplacement.open=FileOpen;i.fileReplacement.close=FileClose;i.fileReplacement.readOffset=FileRead;i.fileReplacement.size=FileSize;i.eventReplacement.objectPointer=this;i.eventReplacement.eventCallback=PlayerEvent;i.basePriority = 0xA0;i.numOutputVideoFrameBuffers = 5;i.autoStart=SCE_FALSE;s_active=this;m_player=sceAvPlayerInit(&i);if(!AvPlayerHandleValid(m_player)){Log("Init error 0x%08X",m_player);m_player=0;return false;}int r=sceAvPlayerAddSource(m_player,m_path.c_str());Log("AddSource=0x%08X",r);if(r<0){sceAvPlayerClose(m_player);m_player=0;FileClose(this);return false;}m_startedUs=sceKernelGetSystemTimeWide();return true;}
bool VideoPlayer::Open(const char*p){Close();if(!p||!*p)return false;m_path=p;m_failed=false;m_ready=m_started=m_eos=false;sceIoMkdir("ux0:data/FileManager",0777);sceIoRemove("ux0:data/FileManager/video_diagnostic.txt");if(sceSysmoduleIsLoaded(SCE_SYSMODULE_AVPLAYER)<0)Log("Module load=0x%08X",sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER));if(!s_rgbaModule)s_rgbaModule=new Module("app0:module/libSceAvPlayerPSVitaRGBA8888.suprx");Log("Opening RGBA %s",p);if(!OpenDecoder()){m_failed=true;return false;}return true;}
void VideoPlayer::CloseDecoder(){StopAudio();m_frame.clear();m_surfaces.clear();m_haveFrame=false;if(AvPlayerHandleValid(m_player)){sceDisplayWaitVblankStart();sceAvPlayerStop(m_player);sceAvPlayerClose(m_player);}m_player=0;FileClose(this);m_ready=m_started=m_eos=false;}
void VideoPlayer::Close(){CloseDecoder();m_path.clear();m_duration=0;m_paused=false;m_failed=false;if(s_active==this)s_active=NULL;}
void VideoPlayer::Update(){if(!AvPlayerHandleValid(m_player))return;if(m_ready&&!m_started){RefreshStreams();int r=sceAvPlayerStart(m_player);Log("Start=0x%08X",r);if(r<0){m_failed=true;return;}m_started=true;StartAudio();}if(!m_haveFrame&&sceKernelGetSystemTimeWide()-m_startedUs>=10000000ULL){Log("No RGBA frame ready=%d started=%d",m_ready,m_started);m_failed=true;return;}if(!m_started)return;SceAvPlayerFrameInfoEx f;memset(&f,0,sizeof(f));if(!sceAvPlayerGetVideoDataEx(m_player,&f)||!f.pData)return;uint32_t w=f.details.video.width,h=f.details.video.height,p=f.details.video.pitch;if(!w||!h||w>1920||h>1088||p<w){Log("Invalid frame %ux%u pitch=%u",w,h,p);return;}intrusive_ptr<graph::Surface>s;for(uint32_t n=0;n<m_surfaces.size();n++){SurfaceEntry&e=m_surfaces.at(n);if(e.data==f.pData&&e.width==w&&e.height==h&&e.pitch==p){s=e.surface;break;}}if(!s.get()){SurfaceEntry e;e.data=f.pData;e.width=w;e.height=h;e.pitch=p;e.surface=new graph::Surface(w,h,ImageMode_RGBA8888,ImageOrder_Linear,1,f.pData,f.details.video.pitch*4,1,0);s=e.surface;m_surfaces.push_back(e);}m_frame=s;if(!m_haveFrame)Log("First RGBA frame %ux%u pitch=%u",w,h,p);m_haveFrame=m_frame.get()!=NULL;}
void VideoPlayer::PauseResume(){if(!AvPlayerHandleValid(m_player))return;int r=m_paused?sceAvPlayerResume(m_player):sceAvPlayerPause(m_player);if(r>=0)m_paused=!m_paused;}
int VideoPlayer::SetSpeed(int32_t percent){if(!AvPlayerHandleValid(m_player))return-1;if(percent!=100&&percent!=150&&percent!=200)return-1;int r=sceAvPlayerSetTrickSpeed(m_player,percent);if(r<0&&percent==150){Log("Speed 150 rejected 0x%08X; restoring 100",r);sceAvPlayerSetTrickSpeed(m_player,100);m_speed=100;return r;}if(r>=0)m_speed=percent;return r;}
void VideoPlayer::Seek(int32_t d){int64_t t=(int64_t)CurrentTime()+d;if(t<0)t=0;if(m_duration&&(uint64_t)t>m_duration)t=m_duration>1000?m_duration-1000:0;SeekTo(t);}
void VideoPlayer::SeekTo(uint64_t t){if(AvPlayerHandleValid(m_player))sceAvPlayerJumpToTime(m_player,t);} uint64_t VideoPlayer::CurrentTime()const{return AvPlayerHandleValid(m_player)?sceAvPlayerCurrentTime(m_player):0;}
