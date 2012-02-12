/*
  Adventure Game Studio source code Copyright 1999-2011 Chris Jones.
  All rights reserved.

  The AGS Editor Source Code is provided under the Artistic License 2.0
  http://www.opensource.org/licenses/artistic-license-2.0.php

  You MAY NOT compile your own builds of the engine without making it EXPLICITLY
  CLEAR that the code has been altered from the Standard Version.

*/
#pragma unmanaged
#define CROOM_NOFUNCTIONS
#ifndef USE_ALFONT
#define USE_ALFONT
#endif
#define USE_CLIB
#define WGT2ALLEGRO_NOFUNCTIONS
#include "wgt2allg.h"
#include "acroom.h"
#include "acruntim.h"

#ifdef USE_ALFONT
#include "alfont.h"
#endif

#ifdef THIS_IS_THE_ENGINE
#define fopen clibfopen
#endif

typedef unsigned char* wgtfont;

extern "C"
{
  extern FILE *clibfopen(char *, char *);
  extern long cliboffset(char *);
  extern long clibfilesize(char *);
  extern long last_opened_size;
}

class WFNFontRenderer : public IAGSFontRenderer {
public:
  virtual bool LoadFromDisk(int fontNumber, int fontSize);
  virtual void FreeMemory(int fontNumber);
  virtual bool SupportsExtendedCharacters(int fontNumber) { return extendedCharacters[fontNumber]; }
  virtual int GetTextWidth(const char *text, int fontNumber);
  virtual int GetTextHeight(const char *text, int fontNumber);
  virtual void RenderText(const char *text, int fontNumber, BITMAP *destination, int x, int y, int colour) ;
  virtual void AdjustYCoordinateForFont(int *ycoord, int fontNumber);
  virtual void EnsureTextValidForFont(char *text, int fontNumber);

private:
  int printchar(int xxx, int yyy, int fontNumber, int charr);
  bool extendedCharacters[MAX_FONTS];
};

class TTFFontRenderer : public IAGSFontRenderer {
public:
  virtual bool LoadFromDisk(int fontNumber, int fontSize);
  virtual void FreeMemory(int fontNumber);
  virtual bool SupportsExtendedCharacters(int fontNumber) { return true; }
  virtual int GetTextWidth(const char *text, int fontNumber);
  virtual int GetTextHeight(const char *text, int fontNumber);
  virtual void RenderText(const char *text, int fontNumber, BITMAP *destination, int x, int y, int colour) ;
  virtual void AdjustYCoordinateForFont(int *ycoord, int fontNumber);
  virtual void EnsureTextValidForFont(char *text, int fontNumber);
};

extern bool ShouldAntiAliasText();
extern int our_eip;
int texttrans = 0;
int textcol;
int wtext_multiply = 1;
wgtfont fonts[MAX_FONTS];
IAGSFontRenderer* fontRenderers[MAX_FONTS];
WFNFontRenderer wfnRenderer;
TTFFontRenderer ttfRenderer;

#ifdef USE_ALFONT
ALFONT_FONT *tempttffnt;
ALFONT_FONT *get_ttf_block(wgtfont fontptr)
{
  memcpy(&tempttffnt, &fontptr[4], sizeof(tempttffnt));
  return tempttffnt;
}
#endif // USE_ALFONT

#ifndef THIS_IS_THE_ENGINE
extern GameSetupStruct thisgame;
void check_font(int *fontnum)
{
  // Stop roomedit crashing if they specify an invalid font number
  if (fontnum[0] >= thisgame.numfonts)
    fontnum[0] = 0;
}
#endif

void init_font_renderer()
{
#ifdef USE_ALFONT
  alfont_init();
  alfont_text_mode(-1);
#endif

  for (int i = 0; i < MAX_FONTS; i++)
    fontRenderers[i] = NULL;
  wtexttransparent(TEXTFG);
}

void shutdown_font_renderer()
{
#ifdef USE_ALFONT
#ifdef THIS_IS_THE_ENGINE
  our_eip = 9919;
#endif
  alfont_exit();
#endif
}

void adjust_y_coordinate_for_text(int* ypos, int fontnum)
{
  fontRenderers[fontnum]->AdjustYCoordinateForFont(ypos, fontnum);
}

void ensure_text_valid_for_font(char *text, int fontnum)
{
  fontRenderers[fontnum]->EnsureTextValidForFont(text, fontnum);
}

int wgettextwidth(const char *texx, int fontNumber)
{
  return fontRenderers[fontNumber]->GetTextWidth(texx, fontNumber);
}

int wgettextheight(const char *text, int fontNumber)
{
  return fontRenderers[fontNumber]->GetTextHeight(text, fontNumber);
}

void wouttextxy(int xxx, int yyy, int fontNumber, const char *texx)
{
  if (yyy > abuf->cb)
    return;                   // each char is clipped but this speeds it up

  if (fontRenderers[fontNumber] != NULL)
  {
    fontRenderers[fontNumber]->RenderText(texx, fontNumber, abuf, xxx, yyy, textcol);
  }
}

// Loads a font from disk
bool wloadfont_size(int fontNumber, int fsize)
{
  if (ttfRenderer.LoadFromDisk(fontNumber, fsize))
  {
    fontRenderers[fontNumber] = &ttfRenderer;
    return true;
  }
  else if (wfnRenderer.LoadFromDisk(fontNumber, fsize))
  {
    fontRenderers[fontNumber] = &wfnRenderer;
    return true;
  }

  return false;
}

void wgtprintf(int xxx, int yyy, int fontNumber, char *fmt, ...)
{
  char tbuffer[2000];
  va_list ap;

  va_start(ap, fmt);
  vsprintf(tbuffer, fmt, ap);
  va_end(ap);
  wouttextxy(xxx, yyy, fontNumber, tbuffer);
}

void wtextcolor(int nval)
{
  __my_setcolor(&textcol, nval);
}

void wfreefont(int fontNumber)
{
  if (fontRenderers[fontNumber] != NULL)
    fontRenderers[fontNumber]->FreeMemory(fontNumber);

  fontRenderers[fontNumber] = NULL;
}

void wtexttransparent(int coo)
{
  texttrans = coo;
}


// **** WFN Renderer ****

const char *WFN_FILE_SIGNATURE = "WGT Font File  ";

void WFNFontRenderer::AdjustYCoordinateForFont(int *ycoord, int fontNumber)
{
  // Do nothing
}


void WFNFontRenderer::EnsureTextValidForFont(char *text, int fontNumber)
{
  if ( extendedCharacters[fontNumber] ) return; 

  // replace any extended characters with question marks
  while (text[0]!=0) {
	if ((unsigned char)text[0] > 127 ) // Why was this "> 126" ?
    {
      text[0] = '?';
    }
    text++;
  }
}

int WFNFontRenderer::GetTextWidth(const char *texx, int fontNumber)
{
  wgtfont foon = fonts[fontNumber];
  unsigned short *tabaddr;
  int totlen = 0;
  unsigned int dd;

  unsigned char thisCharacter;
  for (dd = 0; dd < strlen(texx); dd++) 
  {
    thisCharacter = texx[dd];
     if ( !extendedCharacters[fontNumber] && (thisCharacter >= 128) )  thisCharacter = '?';
#ifndef ALLEGRO_BIG_ENDIAN
    tabaddr = (unsigned short *)&foon[15];
    tabaddr = (unsigned short *)&foon[tabaddr[0]];     // get address table
    tabaddr = (unsigned short *)&foon[tabaddr[thisCharacter]];      // use table to find character
    totlen += tabaddr[0];
#else
    tabaddr = (unsigned short *)&foon[15];
    tabaddr = (unsigned short *)&foon[__short_swap_endian(tabaddr[0])];     // get address table
    tabaddr = (unsigned short *)&foon[__short_swap_endian(tabaddr[thisCharacter])];      // use table to find character
    totlen += __short_swap_endian(tabaddr[0]);
#endif
  }
  return totlen * wtext_multiply;
}

int WFNFontRenderer::GetTextHeight(const char *texx, int fontNumber)
{
  unsigned short *tabaddr;
  int highest = 0;
  unsigned int dd;
  wgtfont foon = fonts[fontNumber];

  unsigned char thisCharacter;
  for (dd = 0; dd < strlen(texx); dd++) 
  {
    thisCharacter = texx[dd];
    if ( !extendedCharacters[fontNumber] && (thisCharacter >= 128) )  thisCharacter = '?';
#ifndef ALLEGRO_BIG_ENDIAN
    tabaddr = (unsigned short *)&foon[15];
    tabaddr = (unsigned short *)&foon[tabaddr[0]];     // get address table
    tabaddr = (unsigned short *)&foon[tabaddr[thisCharacter]];      // use table to find character
    int charHeight = tabaddr[1];
#else
    tabaddr = (unsigned short *)&foon[15];
    tabaddr = (unsigned short *)&foon[__short_swap_endian(tabaddr[0])];     // get address table
    tabaddr = (unsigned short *)&foon[__short_swap_endian(tabaddr[thisCharacter])];      // use table to find character
    int charHeight = __short_swap_endian(tabaddr[1]);
#endif
    if (charHeight > highest)
      highest = charHeight;
  }
  return highest * wtext_multiply;
}

void WFNFontRenderer::RenderText(const char *text, int fontNumber, BITMAP *destination, int x, int y, int colour)
{
  unsigned int ee;

#ifdef THIS_IS_THE_ENGINE
  int oldeip = our_eip;
  our_eip = 415;
#endif

  for (ee = 0; ee < strlen(text); ee++)
    x += printchar(x, y, fontNumber, text[ee]);

#ifdef THIS_IS_THE_ENGINE
  our_eip = oldeip;
#endif
}

int WFNFontRenderer::printchar(int xxx, int yyy, int fontNumber, int charr)
{
  wgtfont foo = fonts[fontNumber];
  unsigned short *tabaddr = (unsigned short *)&foo[15];
  unsigned char *actdata;
  int tt, ss, bytewid, orixp = xxx;

  if ( !extendedCharacters[fontNumber] && ((unsigned char)charr >= 128) ) 
    charr = '?';

  

#ifndef ALLEGRO_BIG_ENDIAN
  tabaddr = (unsigned short *)&foo[tabaddr[0]];        // get address table
  tabaddr = (unsigned short *)&foo[tabaddr[(unsigned char)charr]];    // use table to find character (MUST BE UNSIGNED! - Alan)
  int charWidth = tabaddr[0];
  int charHeight = tabaddr[1];
#else
  tabaddr = (unsigned short *)&foo[__short_swap_endian(tabaddr[0])];
  tabaddr = (unsigned short *)&foo[__short_swap_endian(tabaddr[(unsigned char)charr])];
  int charWidth = __short_swap_endian(tabaddr[0]);
  int charHeight = __short_swap_endian(tabaddr[1]);
#endif
  actdata = (unsigned char *)&tabaddr[2];
  bytewid = ((charWidth - 1) / 8) + 1;

  // MACPORT FIX: switch now using charWidth and charHeight
  for (tt = 0; tt < charHeight; tt++) {
    for (ss = 0; ss < charWidth; ss++) {
      if (((actdata[tt * bytewid + (ss / 8)] & (0x80 >> (ss % 8))) != 0)) {
        if (wtext_multiply > 1) {
          rectfill(abuf, xxx + ss, yyy + tt, xxx + ss + (wtext_multiply - 1),
                   yyy + tt + (wtext_multiply - 1), textcol);
        } 
        else
        {
          putpixel(abuf, xxx + ss, yyy + tt, textcol);
        }
      }

      xxx += wtext_multiply - 1;
    }
    yyy += wtext_multiply - 1;
    xxx = orixp;
  }
  return charWidth * wtext_multiply;
}

bool WFNFontRenderer::LoadFromDisk(int fontNumber, int fontSize)
{
  char filnm[20];
  FILE *ffi = NULL;
  char mbuffer[16];
  long lenof;

  sprintf(filnm, "agsfnt%d.wfn", fontNumber);
  ffi = fopen(filnm, "rb");
  if (ffi == NULL)
  {
    // actual font not found, try font 0 instead
    strcpy(filnm, "agsfnt0.wfn");
    ffi = fopen(filnm, "rb");
    if (ffi == NULL)
      return false;
  }

  mbuffer[15] = 0;
  fread(mbuffer, 15, 1, ffi);
  if (strcmp(mbuffer, WFN_FILE_SIGNATURE) != 0) {
    fclose(ffi);
    return false;
  }

#ifdef THIS_IS_THE_ENGINE
  // clibfopen will have set last_opened_size
  lenof = last_opened_size;
#else
  // in the editor, we don't read from clib, only from disk
  lenof = filelength(fileno(ffi));
#endif

  wgtfont tempalloc = (wgtfont) malloc(lenof + 40);
  fclose(ffi);

  ffi = fopen(filnm, "rb");
  fread(tempalloc, lenof, 1, ffi);
  fclose(ffi);

  fonts[fontNumber] = tempalloc;

  // Check if this font supports extended ASCII - Alan
  unsigned short *tabaddr = (unsigned short *)&fonts[fontNumber][15];
  extendedCharacters[fontNumber] = ((lenof - tabaddr[0])/2 > 128); // Are there more than 128 entries ?
  // --

  return true;
}

void WFNFontRenderer::FreeMemory(int fontNumber)
{
  free(fonts[fontNumber]);
  fonts[fontNumber] = NULL;
}


// ***** TTF RENDERER *****
#ifdef USE_ALFONT

void TTFFontRenderer::AdjustYCoordinateForFont(int *ycoord, int fontNumber)
{
  // TTF fonts already have space at the top, so try to remove the gap
  ycoord[0]--;
}

void TTFFontRenderer::EnsureTextValidForFont(char *text, int fontNumber)
{
  // do nothing, TTF can handle all characters
}

int TTFFontRenderer::GetTextWidth(const char *text, int fontNumber)
{
  return alfont_text_length(get_ttf_block(fonts[fontNumber]), text);
}

int TTFFontRenderer::GetTextHeight(const char *text, int fontNumber)
{
  return alfont_text_height(get_ttf_block(fonts[fontNumber]));
}

void TTFFontRenderer::RenderText(const char *text, int fontNumber, BITMAP *destination, int x, int y, int colour)
{
  if (y > destination->cb)  // optimisation
    return;

  ALFONT_FONT *alfpt = get_ttf_block(fonts[fontNumber]);
  // Y - 1 because it seems to get drawn down a bit
  if ((ShouldAntiAliasText()) && (bitmap_color_depth(abuf) > 8))
    alfont_textout_aa(abuf, alfpt, text, x, y - 1, colour);
  else
    alfont_textout(abuf, alfpt, text, x, y - 1, colour);
}

bool TTFFontRenderer::LoadFromDisk(int fontNumber, int fontSize)
{
  char filnm[20];
  sprintf(filnm, "agsfnt%d.ttf", fontNumber);

  // we read the font in manually to make it load from library file
  FILE *reader = clibfopen(filnm, "rb");
  char *membuffer;

  if (reader == NULL)
    return false;

  long lenof = clibfilesize(filnm);

  // if not in the library, get size manually
  if (lenof < 1)
  {
	  lenof = _filelength(_fileno(reader));
  }

  membuffer = (char *)malloc(lenof);
  fread(membuffer, lenof, 1, reader);
  fclose(reader);

  ALFONT_FONT *alfptr = alfont_load_font_from_mem(membuffer, lenof);
  free(membuffer);

  if (alfptr == NULL)
    return false;

  if (fontSize > 0)
    alfont_set_font_size(alfptr, fontSize);

  wgtfont tempalloc = (wgtfont) malloc(20);
  strcpy((char *)tempalloc, "TTF");
  memcpy(&((char *)tempalloc)[4], &alfptr, sizeof(alfptr));
  fonts[fontNumber] = tempalloc;

  return true;
}

void TTFFontRenderer::FreeMemory(int fontNumber)
{
  alfont_destroy_font(get_ttf_block(fonts[fontNumber]));
  free(fonts[fontNumber]);
  fonts[fontNumber] = NULL;
}

#endif   // USE_ALFONT
