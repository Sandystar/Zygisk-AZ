#ifndef __IL2CPP_STRING_H__
#define __IL2CPP_STRING_H__
 
#include <string>
#include <stdint.h>
 
#ifdef __GNUC__
 
#include <endian.h>
 
#endif // __GNUC__
 
//String
struct System_String_Fields {
    int32_t m_stringLength;
    uint16_t m_firstChar;
};
 
struct System_String_o {
    void *klass;
    void *monitor;
    System_String_Fields fields;
};
 
class String
{
public:
    static std::string GetString(System_String_o *o);
    static const char *GetChar(System_String_o *o);
    static System_String_o *ToString(const std::string &s);
};
 
// 从UTF16编码字符串构建，需要带BOM标记
std::string utf16_to_utf8(const std::u16string &u16str);
 
// 从UTF16 LE编码的字符串创建
std::string utf16le_to_utf8(const std::u16string &u16str);
 
// 从UTF16BE编码字符串创建
std::string utf16be_to_utf8(const std::u16string &u16str);
 
// 获取转换为UTF-16 LE编码的字符串
std::u16string utf8_to_utf16le(const std::string &u8str, bool addbom = false, bool *ok = NULL);
 
// 获取转换为UTF-16 BE的字符串
std::u16string utf8_to_utf16be(const std::string &u8str, bool addbom = false, bool *ok = NULL);
 
#endif //__IL2CPP_STRING_H__