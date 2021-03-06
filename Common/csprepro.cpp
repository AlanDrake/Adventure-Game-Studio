/* Preprocessor for script compiler (c) 2000 Chris Jones
*/
#include <stdio.h>
#include <stdlib.h>
#include <String.h>
#include "cscomp.h"

// ******* BEGIN PREPROCESSOR CODE ************

const char *ccSoftwareVersion = "1.0";

#define MAX_LINE_LENGTH 500
#define MAXDEFINES 1500
struct MacroTable {
  int num;
  char*name[MAXDEFINES];
  char*macro[MAXDEFINES];
  void init() {
    num=0; }
  void shutdown();
  int  find_name(char*);
  void add(char*,char*);
  void remove(int index);
  void merge(MacroTable *);

  MacroTable() {
    init();
  }
};
void MacroTable::shutdown() {
  int rr;
  for (rr=0;rr<num;rr++) {
    free(macro[rr]);
    free(name[rr]);
    macro[rr]=NULL;
    name[rr]=NULL;
  }
  num = 0;
}
void MacroTable::merge(MacroTable *others) {

  for (int aa = 0; aa < others->num; aa++) {
    this->add(others->name[aa], others->macro[aa]);
  }

}
int MacroTable::find_name(char* namm) {
  int ss;
  for (ss=0;ss<num;ss++) {
    if (strcmp(namm,name[ss])==0) return ss;
    }
  return -1;
  }
void MacroTable::add(char*namm,char*mac) {
  if (find_name(namm) >= 0) {
    cc_error("macro '%s' already defined",namm);
    return;
    }
  if (num>=MAXDEFINES) {
    cc_error("too many macros defined");
    return;
    }
  name[num]=(char*)malloc(strlen(namm)+5);
  strcpy(name[num],namm);
  macro[num]=(char*)malloc(strlen(mac)+5);
  strcpy(macro[num],mac);
  num++;
  }
void MacroTable::remove(int index) {
  if ((index < 0) || (index >= num)) {
    cc_error("MacroTable::Remove: index out of range");
    return;
  }
  // just blank out the entry, don't bother to remove it
  name[index][0] = 0;
  macro[index][0] = 0;
}

MacroTable macros;

#define MAX_NESTED_IFDEFS 10
char nested_if_include[MAX_NESTED_IFDEFS];
int nested_if_level = 0;

int deletingCurrentLine() {
  if ((nested_if_level > 0) && (nested_if_include[nested_if_level - 1] == 0))
    return 1;

  return 0;
}

int is_whitespace(char cht) {
  // space, tab, EOF, VT (dunno, Chrille had this char appearing)
  if ((cht==' ') || (cht==9) || (cht == 26) || (cht == 11)) return 1;
  return 0;
  }

void skip_whitespace(char**pttt) {
  char*mpt=pttt[0];
  while (is_whitespace(mpt[0])) mpt++;
  pttt[0]=mpt;
  }

int is_digit(int chrac) {
  if ((chrac >= '0') && (chrac <= '9')) return 1;
  return 0;
}

int is_alphanum(int chrac) {
  if ((chrac>='A') & (chrac<='Z')) return 1;
  if ((chrac>='a') & (chrac<='z')) return 1;
  if ((chrac>='0') & (chrac<='9')) return 1;
  if (chrac == '_') return 1;
  if (chrac == '\"') return 1;
  if (chrac == '\'') return 1;
  return 0;
  }

void get_next_word(char*stin,char*wo, bool includeDots = false) {
  if (stin[0] == '\"') {
    // a string
    int breakout = 0;
//    char*oriwo=wo;
    wo[0] = stin[0];
    wo++; stin++;
    while (!breakout) {
      if ((stin[0] == '\"') && (stin[-1] != '\\')) breakout=1;
      if (stin[0] == 0) breakout = 1;
      wo[0] = stin[0];
      wo++; stin++;
      }
    wo[0] = 0;
//    printf("Word: '%s'\n",oriwo);
    return;
    }
  else if (stin[0] == '\'') {
    // a character constant
    int breakout = 0;
    wo[0] = stin[0];
    wo++; stin++;
    while (!breakout) {
      if ((stin[0] == '\'') && (stin[-1] != '\\')) breakout=1;
      if (stin[0] == 0) breakout = 1;
      wo[0] = stin[0];
      wo++; stin++;
      }
    wo[0] = 0;
    return;
  }

  while ((is_alphanum(stin[0])) || ((includeDots) && (stin[0] == '.'))) {
    wo[0] = stin[0];
    wo++;
    stin++;
  }

  wo[0]=0;
  return;
}

void pre_process_directive(char*dirpt,FMEM*outfl) {
  char*shal=dirpt;
  // seek to the end of the first word (eg. #define)
  while ((!is_whitespace(shal[0])) && (shal[0]!=0)) shal++;
/*
  if (shal[0]==0) {
    cc_error("unknown preprocessor directive");
    return;
  }
  */
  // extract the directive name
  int shalwas = shal[0];
  shal[0] = 0;
  char dname[150];
  strcpy(dname,&dirpt[1]);
  strlwr(dname);
  shal[0] = shalwas;
  // skip to the next word on the line
  skip_whitespace(&shal);

  // write a blank line to the output so that line numbers are correct
  fmem_puts("",outfl);

  if ((stricmp(dname, "ifdef") == 0) ||
      (stricmp(dname, "ifndef") == 0) ||
      (stricmp(dname, "ifver") == 0) ||
      (stricmp(dname, "ifnver") == 0)) {

    if (nested_if_level >= MAX_NESTED_IFDEFS) {
      cc_error("too many nested #ifdefs");
      return;
    }
    int isVersionCheck = 0;
    int wantDefined = 1;

    if (dname[2] == 'n')
      wantDefined = 0;

    if (strstr(dname, "ver") != NULL)
      isVersionCheck = 1;

    char macroToCheck[100];
    get_next_word(shal, macroToCheck, true);

    if (macroToCheck[0] == 0) {
      cc_error("Expected token after space");
      return;
    }

    if ((isVersionCheck) && (!is_digit(macroToCheck[0]))) {
      cc_error("Expected version number");
      return;
    }

    if ((nested_if_level > 0) && (nested_if_include[nested_if_level - 1] == 0)) {
      // if nested inside a False one, then this one must be false
      // as well
      nested_if_include[nested_if_level] = 0;
    }
    else if ((isVersionCheck) && (strcmp(ccSoftwareVersion, macroToCheck) >= 0)) {
      nested_if_include[nested_if_level] = wantDefined;
    }
    else if ((!isVersionCheck) && (macros.find_name(macroToCheck) >= 0)) {
      nested_if_include[nested_if_level] = wantDefined;
    }
    else {
      nested_if_include[nested_if_level] = !wantDefined;
    }
    nested_if_level++;
  }
  else if (stricmp(dname, "endif") == 0) {
    if (nested_if_level < 1) {
      cc_error("#endif without #if");
      return;
    }
    nested_if_level--;
  }
  else if (deletingCurrentLine()) {
    // inside a non-defined #ifdef block, don't process anything
  }
  else if (stricmp(dname, "error") == 0) {
    cc_error("User error: %s", shal);
    return;
  }
  else if (stricmp(dname,"define")==0) {
    char nambit[100];
    int nin=0;
    while ((!is_whitespace(shal[0])) && (shal[0]!=0)) {
      nambit[nin]=shal[0];
      nin++;
      shal++;
    }
    nambit[nin]=0;
    skip_whitespace(&shal);

    macros.add(nambit,shal);
  }
  else if (stricmp(dname, "undef") == 0) {
    char macroToCheck[100];
    get_next_word(shal, macroToCheck);

    if (macroToCheck[0] == 0) {
      cc_error("Expected: macro");
      return;
    }

    int idx = macros.find_name(macroToCheck);
    if (idx < 0) {
      cc_error("'%s' not defined", macroToCheck);
      return;
    }

    macros.remove(idx);
  }
  else if ((stricmp(dname, "sectionstart") == 0) ||
           (stricmp(dname, "sectionend") == 0))
  {
    // do nothing - markers for the editor, just remove them
  }
/*  else if (stricmp(dname,"include")==0) {
    if ((shal[0]=='\"') | (shal[0]=='<')) shal++;
    char inclnam[100];
    int incp=0;
    while ((is_alphanum(shal[0])!=0) | (shal[0]=='.')) {
      inclnam[incp]=shal[0]; shal++; incp++; }
    inclnam[incp]=0;
    // TODO:  Should actually include the file/header INCLNAM
    printf("unimplemented #include of '%s'\n",inclnam);
//    fmem_puts(inclnam,outfl);
    }*/
  else
    cc_error("unknown preprocessor directive '%s'",dname);
}


int in_comment=0;
void remove_comments(char*trf) {
  char tbuffer[MAX_LINE_LENGTH];
  if (in_comment) {  // already in a multi-line comment
    char*cmend=strstr(trf,"*/");
    if (cmend!=NULL) {
      cmend+=2;
      strcpy(tbuffer,cmend);
      strcpy(trf,tbuffer);
      in_comment=0;
      }
    else {
      trf[0]=0;
      return;   // still in the comment
      }
    }
  char*strpt = trf;
  char*comment_start = trf;
  while (strpt[0]!=0) {
    if ((strpt[0] == '\"') && (in_comment == 0)) {
      strpt++;
      while ((strpt[0] != '\"') && (strpt[0]!=0)) strpt++;
      if (strpt[0]==0) break;
      }

    if ((strncmp(strpt,"//",2)==0) && (in_comment == 0)) {
      strpt[0]=0;  // chop off the end of the line
      break;
      }
    if ((strncmp(strpt,"/*",2)==0) && (in_comment == 0)) {
      in_comment = 1;
      comment_start = strpt;
      strpt += 2;
    }
    if ((strncmp(strpt,"*/",2)==0) && (in_comment == 1)) {
      comment_start[0]=0;
      strcpy(tbuffer,trf);
      int carryonat = strlen(tbuffer);
      strcat(tbuffer,&strpt[2]);
      strcpy(trf,tbuffer);
      strpt = &trf[carryonat];
      in_comment = 0;
    }
    else if (strpt[0] != 0)
      strpt++;
  }
  if (in_comment) comment_start[0] = 0;

  // remove leading spaces and tabs on line
  strpt = trf;
  while ((strpt[0] == ' ') || (strpt[0] == '\t'))
    strpt++;
  strcpy(tbuffer,strpt);
  strcpy(trf,tbuffer);

  // remove trailing spaces on line
  if (strlen(trf) > 0) {
    while (trf[strlen(trf)-1]==' ') trf[strlen(trf)-1]=0;
    }
  }

void reset_compiler() {
  ccError=0;
  in_comment=0;
  currentline=0;
  }


void pre_process_line(char*lin) {

  if (deletingCurrentLine()) {
    lin[0] = 0;
    return;
  }

  char*oldl=(char*)malloc(strlen(lin)+5);
  strcpy(oldl,lin);
  char*opt=oldl;
  char charBefore = 0;

  while (opt[0]!=0) {
    while (is_alphanum(opt[0])==0) {
      if (opt[0]==0) break;
      lin[0]=opt[0]; lin++; opt++; }

    if (opt > oldl)
      charBefore = opt[-1];
    else
      charBefore = 0;

    char thisword[MAX_LINE_LENGTH];
    get_next_word(opt,thisword);
    opt+=strlen(thisword);

    int fni = -1;
    if (charBefore != '.') {
      // object.member -- if member is a #define, it shouldn't be replaced
      fni = macros.find_name(thisword);
    }

    if (fni >= 0) {
      strcpy(lin,macros.macro[fni]);
      lin+=strlen(macros.macro[fni]);
    }
    else {
      strcpy(lin,thisword);
      lin+=strlen(thisword);
    }

  }
  free(oldl);
  lin[0]=0;
  }

// preprocess: takes source INPU as input, and copies the preprocessed output
//   to outp. The output has all comments, #defines, and leading spaces
//   removed from all lines.
void cc_preprocess(const char *inpu,char*outp) {
  reset_compiler();
  FMEM*temp=fmem_create();
  FMEM*iii=fmem_open(inpu);
  char linebuffer[1200];
  currentline=0;
  nested_if_level = 0;

  while (!fmem_eof(iii)) {
    fmem_gets(iii,linebuffer);
    currentline++;
    if (strlen(linebuffer) >= MAX_LINE_LENGTH) {  // stop array overflows in subroutines
      cc_error("line too long (%d chars max)", MAX_LINE_LENGTH);
      break; 
    }

    remove_comments(linebuffer);
    // need to check this, otherwise re-defining a macro causes
    // a big problem:
    // #define a int, #define a void  results in #define int void
    if (linebuffer[0]!='#')
      pre_process_line(linebuffer);
    if (ccError) break;
    if (linebuffer[0]=='#')
      pre_process_directive(linebuffer,temp);
    else {  // don't output the #define lines
//      printf("%s\n",linebuffer);
      fmem_puts(linebuffer,temp);
      }
    if (ccError) break;
    }

  strcpy(outp,temp->data);
  fmem_close(temp);
  fmem_close(iii);

  if ((nested_if_level) && (!ccError))
    cc_error("missing #endif");
 
}

void preproc_shutdown() {
  macros.shutdown();
  }
void preproc_startup(MacroTable *preDefinedMacros) {
  macros.init();
  if (preDefinedMacros)
    macros.merge(preDefinedMacros);
}

// ***** END PREPROCESSOR CODE *********
