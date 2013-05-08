#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr_plugins_auto
 * @file        plugins/bin/avi.c
 * @brief       This file contains implementation of decoder for AVI multimedia
 *              streams.
 * @version     -
 * @remark      this source file is part of Binary EYE project (BEYE).
 *              The Binary EYE (BEYE) is copyright (C) 1995 Nickols_K.
 *              All rights reserved. This software is redistributable under the
 *              licence given in the file "Licence.en" ("Licence.ru" in russian
 *              translation) distributed in the BEYE archive.
 * @note        Requires POSIX compatible development system
 *
 * @author      Nickols_K
 * @since       1995
 * @note        Development, fixes and improvements
**/
#include <stddef.h>
#include <string.h>

#include "bconsole.h"
#include "beyehelp.h"
#include "colorset.h"
#include "beyeutil.h"
#include "reg_form.h"
#include "bmfile.h"
#include "libbeye/kbd_code.h"
#include "plugins/disasm.h"
#include "plugins/bin/mmio.h"

namespace	usr {
typedef struct
{
    DWORD		dwMicroSecPerFrame;	// frame display rate (or 0L)
    DWORD		dwMaxBytesPerSec;	// max. transfer rate
    DWORD		dwPaddingGranularity;	// pad to multiples of this
						// size; normally 2K.
    DWORD		dwFlags;		// the ever-present flags
    DWORD		dwTotalFrames;		// # frames in file
    DWORD		dwInitialFrames;
    DWORD		dwStreams;
    DWORD		dwSuggestedBufferSize;

    DWORD		dwWidth;
    DWORD		dwHeight;

    DWORD		dwReserved[4];
} MainAVIHeader;

/*
** Stream header
*/

typedef struct {
    FOURCC		fccType;
    FOURCC		fccHandler;
    DWORD		dwFlags;	/* Contains AVITF_* flags */
    WORD		wPriority;
    WORD		wLanguage;
    DWORD		dwInitialFrames;
    DWORD		dwScale;
    DWORD		dwRate;	/* dwRate / dwScale == samples/second */
    DWORD		dwStart;
    DWORD		dwLength; /* In units above... */
    DWORD		dwSuggestedBufferSize;
    DWORD		dwQuality;
    DWORD		dwSampleSize;
    RECT		rcFrame;
} AVIStreamHeader;

static const uint32_t formtypeAVI             =mmioFOURCC('A', 'V', 'I', ' ');
static const uint32_t listtypeAVIHEADER       =mmioFOURCC('h', 'd', 'r', 'l');
static const uint32_t ckidAVIMAINHDR          =mmioFOURCC('a', 'v', 'i', 'h');
static const uint32_t listtypeSTREAMHEADER    =mmioFOURCC('s', 't', 'r', 'l');
static const uint32_t ckidSTREAMHEADER        =mmioFOURCC('s', 't', 'r', 'h');
static const uint32_t ckidSTREAMFORMAT        =mmioFOURCC('s', 't', 'r', 'f');
static const uint32_t ckidSTREAMHANDLERDATA   =mmioFOURCC('s', 't', 'r', 'd');
static const uint32_t ckidSTREAMNAME	      =mmioFOURCC('s', 't', 'r', 'n');

static const uint32_t listtypeAVIMOVIE        =mmioFOURCC('m', 'o', 'v', 'i');
static const uint32_t listtypeAVIRECORD       =mmioFOURCC('r', 'e', 'c', ' ');

static const uint32_t ckidAVINEWINDEX         =mmioFOURCC('i', 'd', 'x', '1');

/*
** Stream types for the <fccType> field of the stream header.
*/
static const uint32_t streamtypeVIDEO         =mmioFOURCC('v', 'i', 'd', 's');
static const uint32_t streamtypeAUDIO         =mmioFOURCC('a', 'u', 'd', 's');
static const uint32_t streamtypeMIDI	      =mmioFOURCC('m', 'i', 'd', 's');
static const uint32_t streamtypeTEXT          =mmioFOURCC('t', 'x', 't', 's');

#if 0
/* Basic chunk types */
static const uint32_t cktypeDIBbits           =aviTWOCC('d', 'b');
static const uint32_t cktypeDIBcompressed     =aviTWOCC('d', 'c');
static const uint32_t cktypePALchange         =aviTWOCC('p', 'c');
static const uint32_t cktypeWAVEbytes         =aviTWOCC('w', 'b');
#endif
/* Chunk id to use for extra chunks for padding. */
static const uint32_t ckidAVIPADDING          =mmioFOURCC('J', 'U', 'N', 'K');

static bool  __FASTCALL__ avi_check_fmt()
{
    unsigned long id;
    id=bmReadDWordEx(8,binary_stream::Seek_Set);
    if(	bmReadDWordEx(0,binary_stream::Seek_Set)==mmioFOURCC('R','I','F','F') &&
	(id==mmioFOURCC('A','V','I',' ') || id==mmioFOURCC('O','N','2',' ')))
	return true;
    return false;
}

static void __FASTCALL__ avi_init_fmt(CodeGuider& code_guider) { UNUSED(code_guider); }
static void __FASTCALL__ avi_destroy_fmt() {}
static int  __FASTCALL__ avi_platform() { return DISASM_DEFAULT; }

static __filesize_t __FASTCALL__ avi_find_chunk(__filesize_t off,unsigned long id)
{
    unsigned long ids,size,type;
    bmSeek(off,binary_stream::Seek_Set);
    while(!bmEOF())
    {
/*	fpos=bmGetCurrFilePos();*/
	ids=bmReadDWord();
	if(ids==id) return bmGetCurrFilePos();
	size=bmReadDWord();
	size=(size+1)&(~1);
/*fprintf(stderr,"%08X:%4s %08X\n",fpos,(char *)&ids,size);*/
	if(ids==mmioFOURCC('L','I','S','T'))
	{
	    type=bmReadDWord();
	    if(type==id) return bmGetCurrFilePos();
	    continue;
	}
	bmSeek(size,binary_stream::Seek_Cur);
    }
    return -1;
}

static __filesize_t __FASTCALL__ Show_AVI_Header()
{
 unsigned keycode;
 TWindow * hwnd;
 MainAVIHeader avih;
 __filesize_t fpos,fpos2;
 fpos = BMGetCurrFilePos();
 fpos2 = avi_find_chunk(12,mmioFOURCC('a','v','i','h'));
 if((__fileoff_t)fpos2==-1) { ErrMessageBox("Main AVI Header not found",""); return fpos; }
 bmSeek(fpos2,binary_stream::Seek_Set);
 bmReadDWord(); /* skip section size */
 bmReadBuffer(&avih,sizeof(MainAVIHeader));
 fpos2 = avi_find_chunk(12,mmioFOURCC('m','o','v','i'));
 if((__fileoff_t)fpos2!=-1) fpos2-=4;
 hwnd = CrtDlgWndnls(" AVI File Header ",43,9);
 hwnd->goto_xy(1,1);
 hwnd->printf(
	  "MicroSecond per Frame= %lu\n"
	  "Max bytes per second = %lu\n"
	  "Padding granularity  = %lu\n"
	  "Flags                = %lu\n"
	  "Total frames         = %lu\n"
	  "Initial frames       = %lu\n"
	  "Number of streams    = %lu\n"
	  "Suggested buffer size= %lu\n"
	  "Width x Height       = %lux%lu\n"
	  ,avih.dwMicroSecPerFrame
	  ,avih.dwMaxBytesPerSec
	  ,avih.dwPaddingGranularity
	  ,avih.dwFlags
	  ,avih.dwTotalFrames
	  ,avih.dwInitialFrames
	  ,avih.dwStreams
	  ,avih.dwSuggestedBufferSize
	  ,avih.dwWidth
	  ,avih.dwHeight);
 while(1)
 {
   keycode = GetEvent(drawEmptyPrompt,NULL,hwnd);
   if(keycode == KE_F(5) || keycode == KE_ENTER) { fpos = fpos2; break; }
   else
     if(keycode == KE_ESCAPE || keycode == KE_F(10)) break;
 }
 delete hwnd;
 return fpos;
}

static __filesize_t __FASTCALL__ Show_A_Header()
{
 unsigned keycode;
 TWindow * hwnd;
 AVIStreamHeader strh;
 WAVEFORMATEX wavf;
 __filesize_t newcpos,fpos,fpos2;
 fpos = BMGetCurrFilePos();
 memset(&wavf,0,sizeof(wavf));
 fpos2=12;
 do
 {
    fpos2 = avi_find_chunk(fpos2,mmioFOURCC('s','t','r','h'));
    if((__fileoff_t)fpos2==-1) { ErrMessageBox("Audio Stream Header not found",""); return fpos; }
    bmSeek(fpos2,binary_stream::Seek_Set);
    newcpos=bmReadDWord();
    bmReadBuffer(&strh,sizeof(AVIStreamHeader));
    fpos2+=newcpos+4;
 }while(strh.fccType!=streamtypeAUDIO);
 if(bmReadDWord()==mmioFOURCC('s','t','r','f'))
 {
    bmReadDWord(); /* skip header size */
    bmReadBuffer(&wavf,sizeof(WAVEFORMATEX));
 }
 hwnd = CrtDlgWndnls(" Stream File Header ",43,18);
 hwnd->goto_xy(1,1);
 hwnd->printf(
	  "Stream type          = %c%c%c%c\n"
	  "FOURCC handler       = %c%c%c%c(%08Xh)\n"
	  "Flags                = %lu\n"
	  "Priority             = %u\n"
	  "Language             = %u\n"
	  "Initial frames       = %lu\n"
	  "Rate/Scale           = %lu/%lu=%5.3f\n"
	  "Start                = %lu\n"
	  "Length               = %lu\n"
	  "Suggested buffer size= %lu\n"
	  "Quality              = %lu\n"
	  "SampleSize           = %lu\n"
	  "====== WAVE HEADER ====================\n"
	  "FormatTag            = 0x%04X (%s)\n"
	  "ChannelsxSampleSecs  = %ux%u\n"
	  "AvgBytesSecs         = %lu\n"
	  "BlockAlign           = %u\n"
	  "BitsPerSample        = %u\n"
	  INT_2_CHAR_ARG(strh.fccType)
	  INT_2_CHAR_ARG(strh.fccHandler),strh.fccHandler
	 ,strh.dwFlags
	 ,strh.wPriority
	 ,strh.wLanguage
	 ,strh.dwInitialFrames
	 ,strh.dwRate
	 ,strh.dwScale
	 ,(float)strh.dwRate/(float)strh.dwScale
	 ,strh.dwStart
	 ,strh.dwLength
	 ,strh.dwSuggestedBufferSize
	 ,strh.dwQuality
	 ,strh.dwSampleSize
	 ,wavf.wFormatTag,wtag_find_name(wavf.wFormatTag)
	 ,wavf.nChannels,wavf.nSamplesPerSec
	 ,wavf.nAvgBytesPerSec
	 ,wavf.nBlockAlign
	 ,wavf.wBitsPerSample);
 while(1)
 {
   keycode = GetEvent(drawEmptyPrompt,NULL,hwnd);
   if(keycode == KE_ESCAPE || keycode == KE_F(10)) break;
 }
 delete hwnd;
 return fpos;
}

static __filesize_t __FASTCALL__ Show_V_Header()
{
 unsigned keycode;
 TWindow * hwnd;
 AVIStreamHeader strh;
 BITMAPINFOHEADER bmph;
 __filesize_t newcpos,fpos,fpos2;
 fpos = BMGetCurrFilePos();
 memset(&bmph,0,sizeof(BITMAPINFOHEADER));
 fpos2=12;
 do
 {
    fpos2 = avi_find_chunk(fpos2,mmioFOURCC('s','t','r','h'));
    if((__fileoff_t)fpos2==-1) { ErrMessageBox("Video Stream Header not found",""); return fpos; }
    bmSeek(fpos2,binary_stream::Seek_Set);
    newcpos=bmReadDWord(); /* skip section size */
    bmReadBuffer(&strh,sizeof(AVIStreamHeader));
    fpos2+=newcpos+4;
 }while(strh.fccType!=streamtypeVIDEO);
 if(bmReadDWord()==mmioFOURCC('s','t','r','f'))
 {
    bmReadDWord(); /* skip header size */
    bmReadBuffer(&bmph,sizeof(BITMAPINFOHEADER));
 }
 hwnd = CrtDlgWndnls(" Stream File Header ",43,20);
 hwnd->goto_xy(1,1);
 hwnd->printf(
	  "Stream type          = %c%c%c%c\n"
	  "FOURCC handler       = %c%c%c%c(%08Xh)\n"
	  "Flags                = %lu\n"
	  "Priority             = %u\n"
	  "Language             = %u\n"
	  "Initial frames       = %lu\n"
	  "Rate/Scale           = %lu/%lu=%5.3f\n"
	  "Start                = %lu\n"
	  "Length               = %lu\n"
	  "Suggested buffer size= %lu\n"
	  "Quality              = %lu\n"
	  "SampleSize           = %lu\n"
	  "FrameRect            = %u.%ux%u.%u\n"
	  "===== BITMAP INFO HEADER ===============\n"
	  "WxH                  = %lux%lu\n"
	  "PlanesxBitCount      = %ux%u\n"
	  "Compression          = %c%c%c%c\n"
	  "ImageSize            = %lu\n"
	  "XxYPelsPerMeter      = %lux%lu\n"
	  "ColorUsedxImportant  = %lux%lu\n"
	  INT_2_CHAR_ARG(strh.fccType)
	  INT_2_CHAR_ARG(strh.fccHandler),strh.fccHandler
	 ,strh.dwFlags
	 ,strh.wPriority
	 ,strh.wLanguage
	 ,strh.dwInitialFrames
	 ,strh.dwRate
	 ,strh.dwScale
	 ,(float)strh.dwRate/(float)strh.dwScale
	 ,strh.dwStart
	 ,strh.dwLength
	 ,strh.dwSuggestedBufferSize
	 ,strh.dwQuality
	 ,strh.dwSampleSize
	 ,strh.rcFrame.left,strh.rcFrame.top,strh.rcFrame.right,strh.rcFrame.bottom
	 ,bmph.biWidth,bmph.biHeight
	 ,bmph.biPlanes,bmph.biBitCount
	 INT_2_CHAR_ARG(bmph.biCompression)
	 ,bmph.biSizeImage
	 ,bmph.biXPelsPerMeter,bmph.biYPelsPerMeter
	 ,bmph.biClrUsed,bmph.biClrImportant);
 while(1)
 {
   keycode = GetEvent(drawEmptyPrompt,NULL,hwnd);
   if(keycode == KE_ESCAPE || keycode == KE_F(10)) break;
 }
 delete hwnd;
 return fpos;
}

extern const REGISTRY_BIN aviTable =
{
  "Audio Video Interleaved format",
  { NULL, "Audio", "Video", NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, Show_A_Header, Show_V_Header, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  avi_check_fmt,
  avi_init_fmt,
  avi_destroy_fmt,
  Show_AVI_Header,
  NULL,
  avi_platform,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
} // namespace	usr
