#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr_plugins_auto
 * @file        plugins/bin/asf.c
 * @brief       This file contains implementation of decoder for ASF v1
 *              file format.
 * @version     -
 * @remark      this source file is part of asfary vIEW project (BEYE).
 *              The asfary vIEW (BEYE) is copyright (C) 1995 Nickols_K.
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
static bool  __FASTCALL__ asf_check_fmt()
{
    const unsigned char asfhdrguid[16]= {0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};
/*    const unsigned char asf2hdrguid[16]={0xD1,0x29,0xE2,0xD6,0xDA,0x35,0xD1,0x11,0x90,0x34,0x00,0xA0,0xC9,0x03,0x49,0xBE}; */
    unsigned char buff[16];
    bmReadBufferEx(buff,16,0,binary_stream::Seek_Set);
    if(memcmp(buff,asfhdrguid,16)==0) return true;
    return false;
}

static __filesize_t __FASTCALL__ Show_ASF_Header()
{
    ErrMessageBox("Not implemented yet!","ASF format");
    return BMGetCurrFilePos();
}

static void __FASTCALL__ asf_init_fmt(CodeGuider& code_guider) { UNUSED(code_guider); }
static void __FASTCALL__ asf_destroy_fmt() {}
static int  __FASTCALL__ asf_platform() { return DISASM_DEFAULT; }

extern const REGISTRY_BIN asfTable =
{
  "Advanced stream file format v1",
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  asf_check_fmt,
  asf_init_fmt,
  asf_destroy_fmt,
  Show_ASF_Header,
  NULL,
  asf_platform,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
} // namespace	usr
