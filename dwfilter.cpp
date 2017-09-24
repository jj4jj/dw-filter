#include "dwfilter.h"
#include <unordered_map>

enum  MtachNodeFlag {
    //LEVEL:0X01, 0X02, 0X04, 0X08, 0X10, 0X20, 0X40, 0X80
    MATCH_NODE_FLAG_END_MATCH = 0x0100,
    MATCH_NODE_FLAG_END_VOCAB = 0X0200,
};

struct MatchNode {
    std::unordered_map<int, MatchNode*>   Dict;
    uint16_t                              wAlphaUTF16LE;
    uint16_t                              wFlags;
};

static MatchNode  s_MatchTreeRoot;
static char s_szUTF8[1024 * 1024 * 4];
static int s_iMatchWordsNum;

static void FreeMatchTree(MatchNode * root){
    auto it = root->Dict.begin();
    while(it != root->Dict.end()){
        FreeMatchTree(it->second);
        delete it->second;
        ++it;
    }
}
int  dwf::Clear(){
    //FREE NODE
    FreeMatchTree(&s_MatchTreeRoot);
    s_MatchTreeRoot.Dict.clear();
    s_iMatchWordsNum = 0;
    ////////////////////////////////
    return 0;
}

int  dwf::Destory(){
    Clear();
    return 0;
}
#define LOG_ERR(...)

//////////////////////////////////////////////////////////////////////////

// Convert Unicode big endian to Unicode little endian
unsigned ucs2_be_to_le(unsigned short *ucs2bige, unsigned int size)
{
    if (!ucs2bige) {
        return 0;
    }    
    unsigned int length = size;
    unsigned short *tmp = ucs2bige;    
    while (*tmp && length) {        
        length--;
        unsigned char val_high = *tmp >> 8;
        unsigned char val_low = (unsigned char)*tmp;        
        *tmp = val_low << 8 | val_high;        
        tmp++;
    }    
    return size - length;
}

// Convert Ucs-2 to Utf-8
unsigned int ucs2_to_utf8(unsigned short *ucs2, unsigned int ucs2_size, 
        unsigned char *utf8, unsigned int utf8_size){
    unsigned int length = 0;    
    if (!ucs2) {
        return 0;
    }    
    unsigned short *inbuf = ucs2;
    unsigned char *outbuf = utf8;
    
    if (*inbuf == 0xFFFE) {
        ucs2_be_to_le(inbuf, ucs2_size);
    }
    
    if (!utf8) {
        unsigned int insize = ucs2_size;
        
        while (*inbuf && insize) {
            insize--;
            
/*            if (*inbuf == 0xFEFF) {
                inbuf++;
                continue;
            }*/
            
            if (0x0080 > *inbuf) {
                length++;
            } else if (0x0800 > *inbuf) {
                length += 2;                
            } else {
                length += 3;
            }
            
            inbuf++;
        }
        return length;
        
    } else {        
        unsigned int insize = ucs2_size;
        
        while (*inbuf && insize && length < utf8_size) {            
            insize--;
            
            if (*inbuf == 0xFFFE) {
                inbuf++;
                continue;
            }
            
            if (0x0080 > *inbuf) {
                /* 1 byte UTF-8 Character.*/
                *outbuf++ = (unsigned char)(*inbuf);
                length++;
            } else if (0x0800 > *inbuf) {
                /*2 bytes UTF-8 Character.*/
                *outbuf++ = 0xc0 | ((unsigned char)(*inbuf >> 6));
                *outbuf++ = 0x80 | ((unsigned char)(*inbuf & 0x3F));
                length += 2;

            } else {
                /* 3 bytes UTF-8 Character .*/
                *outbuf++ = 0xE0 | ((unsigned char)(*inbuf >> 12));
                *outbuf++ = 0x80 | ((unsigned char)((*inbuf >> 6) & 0x3F));
                *outbuf++ = 0x80 | ((unsigned char)(*inbuf & 0x3F));
                length += 3; 
            }
            
            inbuf++;
        }
        
        return length;
    }
}

// Convert Utf-8 to Ucs-2 
unsigned int utf8_to_ucs2(unsigned char *utf8, unsigned int utf8_size, 
        unsigned short *ucs2, unsigned int ucs2_size)
{
    int length = 0;
    unsigned int insize = utf8_size;
    unsigned char *inbuf = utf8;

    if(!utf8)
        return 0;

    if(!ucs2) {
        while(*inbuf && insize) {
            unsigned char c = *inbuf;
            if((c & 0x80) == 0) {
                length += 1;
                insize -= 1;
                inbuf++;
            }
            else if((c & 0xE0) == 0xC0) {
                length += 1;
                insize -= 2;
                inbuf += 2;
            } else if((c & 0xF0) == 0xE0) {
                length += 1;
                insize -= 3;
                inbuf += 3;
            }
        }
        return length;

    } else {
        unsigned short *outbuf = ucs2;
        unsigned int outsize = ucs2_size;

        while(*inbuf && insize && length < (int)outsize) {
            unsigned char c = *inbuf;
            if((c & 0x80) == 0) {
                *outbuf++ = c;
                inbuf++;
                length++;
                insize--;
            } else if((c & 0xE0) == 0xC0) {
                unsigned short val;

                val = (c & 0x3F) << 6;
                inbuf++;
                c = *inbuf;
                val |= (c & 0x3F);
                inbuf++;

                length++;
                insize -= 2;

                *outbuf++ = val;
            } else if((c & 0xF0) == 0xE0) {
                unsigned short val;

                val = (c & 0x1F) << 12;
                inbuf++;
                c = *inbuf;
                val |= (c & 0x3F) << 6;
                inbuf++;
                c = *inbuf;
                val |= (c & 0x3F);
                inbuf++;

                insize -= 3;
                length++;

                *outbuf++ = val;
            }
            else {
                //skip
                insize--;
                inbuf++;
            }
        }
        return length;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
int utf8_to_utf16le(std::vector<uint16_t> & vecUTF16LE, const std::string & strUTF8){
    vecUTF16LE.clear();
    if(strUTF8.empty()){
        return 0;
    }
    char * pszUTF8 = (char*)strUTF8.data();
    static    uint16_t  s_vecUCS2[1024*1024*2];
    int iSize = utf8_to_ucs2((unsigned char*)pszUTF8, strUTF8.size(), s_vecUCS2, sizeof(s_vecUCS2));
    for(int i = 0;i < iSize; ++i){
        vecUTF16LE.push_back(s_vecUCS2[i]);    
    }
    return 0;
}

int utf16le_to_utf8(std::string & strUTF8, const std::vector<uint16_t> & vecUTF16LE) {
    strUTF8.clear();
    if (vecUTF16LE.empty()) {
        return 0;
    }
    int iSize = ucs2_to_utf8((short unsigned int*)vecUTF16LE.data(), vecUTF16LE.size(), (unsigned char *)s_szUTF8, sizeof(s_szUTF8));
    s_szUTF8[iSize] = 0;
    strUTF8 = s_szUTF8;
    return 0;
}
int  dwf::TotalWords(){
    return s_iMatchWordsNum;
}

int  dwf::Remove(const std::string & strUTF8){
    if (strUTF8.empty()) {
        return -1;
    }
    static std::vector<uint16_t> vecUTF16LE;
    int iRetCode = utf8_to_utf16le(vecUTF16LE, strUTF8);
    if (iRetCode) {
        LOG_ERR("convert utf8 error :%s !", strSentenseUTF8.data());
        return -1;
    }
    MatchNode * pItr = &s_MatchTreeRoot;
    for (int i = 0; i < (int)vecUTF16LE.size(); ++i) {
        auto it = pItr->Dict.find(vecUTF16LE[i]);
        if (it == pItr->Dict.end()) {
            return -1;
        }
        pItr = it->second;
    }

    if(pItr->wFlags & MATCH_NODE_FLAG_END_MATCH){
        pItr->wFlags &= (~(MATCH_NODE_FLAG_END_MATCH));
        --s_iMatchWordsNum;
    }

    return 0;
}
int  dwf::AddWords(const std::string & strUTF8, int iMatchLevel, bool bIsVocaulary) {
    if(strUTF8.empty()){
        return -1;
    }
    static std::vector<uint16_t> vecUTF16LE;
    int iRetCode = utf8_to_utf16le(vecUTF16LE, strUTF8);
    if(iRetCode){
        LOG_ERR("convert utf8 error :%s !", strUTF8.data());
        return -1;
    }
    iMatchLevel &= 0xFF;
    MatchNode * pItr = &s_MatchTreeRoot;
    for(int i = 0 ;i < (int)vecUTF16LE.size(); ++i){
        auto it = pItr->Dict.find(vecUTF16LE[i]);
        if(it == pItr->Dict.end()){
            //insert all
            MatchNode * pMatch = new MatchNode();
            pMatch->wAlphaUTF16LE = vecUTF16LE[i];
            pMatch->wFlags = 0;
            if(i == (int)(vecUTF16LE.size() - 1)){
                pMatch->wFlags |= MATCH_NODE_FLAG_END_MATCH;
                if(bIsVocaulary){
                    pMatch->wFlags |= MATCH_NODE_FLAG_END_VOCAB;
                }
                ++s_iMatchWordsNum;
                //printf("add new words:%s match level:%d \n", strUTF8.c_str(), iMatchLevel);
            }
            pMatch->wFlags |= iMatchLevel;
            pItr->Dict[vecUTF16LE[i]] = pMatch;
            pItr = pMatch;
        }
        else {
            if(iMatchLevel < (pItr->wFlags & 0xFF)){
                pItr->wFlags &= 0xFF00 ;
                pItr->wFlags |= iMatchLevel;
            }
            pItr = it->second;
        }    
    }
    return 0;
}
int  dwf::MatchWords(std::vector<std::string> & strMatchedList, const std::string & strSentenseUTF8, bool bGlobal) {
    if (strSentenseUTF8.empty()) {
        return -1;
    }
    strMatchedList.clear();
    static std::vector<uint16_t> vecUTF16LE;
    int iRetCode = utf8_to_utf16le(vecUTF16LE, strSentenseUTF8);
    if (iRetCode) {
        LOG_ERR("convert utf8 error :%s !", strSentenseUTF8.data());
        return -1;
    }
    int iMatchedMinLevel = 100;
    for (int i = 0; i < (int)vecUTF16LE.size(); ++i) {
        MatchNode * pItr = &s_MatchTreeRoot;
        auto it = pItr->Dict.find(vecUTF16LE[i]);
        if (it == pItr->Dict.end()) {
            continue;
        }
        //
        pItr = it->second;
        std::vector<uint16_t>    vecMatchedUTF16;
        vecMatchedUTF16.push_back(vecUTF16LE[i]);
        int j = i+1;
        for(j = i+1; j < (int)vecUTF16LE.size(); ++j){
            auto sfit = pItr->Dict.find(vecUTF16LE[j]);
            if(sfit != pItr->Dict.end()){
                vecMatchedUTF16.push_back(vecUTF16LE[j]);
                pItr = sfit->second;
            }
            else {                
                break;
            }
        }
        if (pItr->wFlags & MATCH_NODE_FLAG_END_MATCH) {
            if (pItr->wFlags & MATCH_NODE_FLAG_END_VOCAB) {
                if (j + 1 == (int)vecUTF16LE.size() ||
                    vecUTF16LE[j] == ' ' ||
                    vecUTF16LE[j] == '\t' ||
                    vecUTF16LE[j] == '\r' ||
                    vecUTF16LE[j] == '\n' ||
                    vecUTF16LE[j] == '\0') {
                    //
                    //OK       
                }
                else {
                    //printf("matched but not a vocab:%s", &strSentenseUTF8[i]);
                    //VOCA , BUT HAS MORE
                    break;
                }
            }
            std::string strUTF8;
            utf16le_to_utf8(strUTF8, vecMatchedUTF16);
            int iMatchedLevel = (pItr->wFlags & 0xFF);
            if (!strUTF8.empty()) {
                strMatchedList.push_back(strUTF8);
                if (!bGlobal) {
                    return iMatchedLevel;
                }
                if (iMatchedMinLevel > iMatchedLevel) {
                    iMatchedMinLevel = iMatchedLevel;
                }
            }
            //skip some
            i = j;
        }

    }
    return iMatchedMinLevel;
}
