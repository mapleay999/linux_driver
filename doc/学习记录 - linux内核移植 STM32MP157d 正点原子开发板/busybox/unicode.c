/* 
 * libbb/unicode.c 文件中 unicode_conv_to_printable2 函数替换内容。
 */
static char* FAST_FUNC unicode_conv_to_printable2(uni_stat_t 
*stats, const char *src, unsigned width, int flags)
{ 
   char *dst; 
   unsigned dst_len; 
   unsigned uni_count; 
   unsigned uni_width; 
 
   if (unicode_status != UNICODE_ON) { 
       char *d; 
       if (flags & UNI_FLAG_PAD) { 
           d = dst = xmalloc(width + 1); 
           while ((int)--width >= 0) { 
               unsigned char c = *src; 
               if (c == '\0') { 
                   do 
                       *d++ = ' '; 
                   while ((int)--width >= 0); 
                   break; 
               } 
               /* 修改下面一行代码 */ 
               /* *d++ = (c >= ' ' && c < 0x7f) ? c : '?'; */ 
               *d++ = (c >= ' ') ? c : '?'; 
               src++; 
           } 
           *d = '\0'; 
       } else { 
           d = dst = xstrndup(src, width); 
           while (*d) { 
               unsigned char c = *d; 
               /* 修改下面一行代码 */ 
               /* if (c < ' ' || c >= 0x7f) */ 
               if(c < ' ') 
                   *d = '?'; 
               d++; 
           } 
       } 
       if (stats) { 
           stats->byte_count = (d - dst); 
           stats->unicode_count = (d - dst); 
           stats->unicode_width = (d - dst); 
       } 
       return dst; 
   } 
   return dst; 
}