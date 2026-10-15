#include <cstddef>
#include <stdarg.h>
namespace std {
static void add(char*&p,size_t&left,const char*s){if(!s)return;while(*s&&left>1){*p++=*s++;--left;}}
static void integer(char*&p,size_t&left,long v,int pad){char n[25];int i=0;unsigned long x;
 if(v<0){add(p,left,"-");x=(unsigned long)(-(v+1))+1;}else x=(unsigned long)v;
 do{n[i++]='0'+x%10;x/=10;}while(x&&i<24);while(i<pad&&i<24)n[i++]='0';
 while(i&&left>1){*p++=n[--i];--left;}}
int snprintf(char* buf,size_t max,const char* fmt,...){if(!buf||!max)return 0;char* p=buf;size_t left=max;va_list args;va_start(args,fmt);
 for(;*fmt&&left>1;fmt++){
  if(*fmt!='%'){*p++=*fmt;--left;continue;}
  ++fmt;int pad=0;if(*fmt=='0'){pad=2;fmt++;}
  if(*fmt=='s')add(p,left,va_arg(args,const char*));
  else if(*fmt=='d')integer(p,left,va_arg(args,int),pad);
  else if(*fmt=='u')integer(p,left,(long)va_arg(args,unsigned),pad);
  else if(*fmt=='%'){*p++='%';--left;}
 }
 va_end(args);*p=0;return (int)(p-buf);
}
}
