#include <cstdio>
#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <random>
using namespace std;

#pragma pack(push, 1)

struct LocalFileHeader
{
    uint32_t signature = 0x04034b50u;
    uint16_t versionNeeded = 0;
    uint16_t flag = 0;
    uint16_t compression = 0;
    uint16_t modifiedTime = 0;
    uint16_t modifiedDate = 0;
    uint32_t crc = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLength = 0;
    uint16_t extraFieldLength = 0;
};

struct CentralDirectoryHeader
{
    uint32_t signature = 0x02014b50u;
    uint16_t versionMade = 0;
    uint16_t versionNeeded = 0;
    uint16_t flag = 0;
    uint16_t compression = 0;
    uint16_t modifiedTime = 0;
    uint16_t modifiedDate = 0;
    uint32_t crc = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLength = 0;
    uint16_t extraFieldLength = 0;
    uint16_t commentLength = 0;
    uint16_t disk = 0;
    uint16_t internalAttr = 0;
    uint32_t externalAttr = 0;
    uint32_t offset = 0;
};

struct ExtraFieldHeader
{
    uint16_t id = 0;
    uint16_t size = 0;
};

struct EndOfCentralDirectoryRecord
{
    uint32_t signature = 0x06054b50u;
    uint16_t disk = 0;
    uint16_t centralDirDisk = 0;
    uint16_t entryNumber = 0;
    uint16_t totalEntryNumber = 0;
    uint32_t centralDirSize = 0;
    uint32_t centralDirOffset = 0;
    uint16_t commentLength = 0;
};

#pragma pack(pop)

/*
int main()
{
    FILE *f = fopen("empty.zip", "wb");
    EndOfCentralDirectoryRecord end;
    fwrite(&end, sizeof end, 1, f);
    fclose(f);
}
*/

/*
int main()
{
    FILE *f = fopen("comment.zip", "wb");
    EndOfCentralDirectoryRecord end;
    end.commentLength = 7;
    fwrite(&end, sizeof end, 1, f);
    fwrite("comment", 1, 7, f);
    fclose(f);
}
*/

uint32_t crc_tmp1(vector<uint8_t> data)
{
    //  被除数
    vector<int> a;
    for (size_t i=0; i<data.size(); i++)
        for (int j=0; j<8; j++)
            a.push_back(data[i]>>j&1);
    for (int i=0; i<32; i++)
        a.push_back(0);
    for (int i=0; i<32; i++)
        a[i] ^= 1;
    //  除数
    vector<int> b(1, 1);
    for (int i=31; i>=0; i--)
        b.push_back(0x04C11DB7>>i&1);
    //  除算
    for (size_t i=0; i<data.size()*8; i++)
        if (a[i]!=0)
            for (int j=0; j<33; j++)
                a[i+j] ^= b[j];
    //  CRC
    uint32_t c = 0;
    for (int i=0; i<32; i++)
        c |= a[data.size()*8+i]<<i;
    return ~c;
}

vector<uint8_t> stringToUint8(string str)
{
    vector<uint8_t> uint8;
    for (char c: str)
        uint8.push_back((uint8_t)c);
    return uint8;
}

/*
int main()
{
    printf("%08x\n", crc_tmp1(stringToUint8("understand zip")));
    //  3cde314f
}
*/

uint32_t crc_tmp2(vector<uint8_t> data)
{
    uint32_t c = 0;
    for (size_t i=0; i<data.size()+4; i++)
        for (int j=0; j<8; j++) {
            int lsb = c&1;
            c = ((i<data.size() ? data[i]>>j&1 : 0) ^ (i<4 ? 1 : 0))<<31 | c>>1;
            if (lsb != 0)
                c ^= 0xedb88320;
        }
    return ~c;
}

uint32_t crc_tmp3(vector<uint8_t> data)
{
    uint32_t c = 0xffffffffu;
    for (size_t i=0; i<data.size(); i++)
        for (int j=0; j<8; j++) {
            int lsb = c&1 ^ data[i]>>j&1;
            c >>= 1;
            if (lsb != 0)
                c ^= 0xedb88320;
        }
    return ~c;
}

uint32_t crcTable[256];
void makeCrcTable()
{
    for (int i=0; i<256; i++) {
        uint32_t c = i;
        for (int j=0; j<8; j++)
            c = c>>1 ^ ((c&1)!=0 ? 0xedb88320 : 0);
        crcTable[i] = c;
    }
}

uint32_t crc32(uint32_t c, uint8_t d)
{
    return c>>8 ^ crcTable[c&0xff^d];
}

uint32_t crc(vector<uint8_t> data)
{
    uint32_t c = 0xffffffffu;
    for (size_t i=0; i<data.size(); i++)
        c = crc32(c, data[i]);
    return ~c;
}

void copyHeader(const CentralDirectoryHeader &central, LocalFileHeader *local)
{
    local->versionNeeded = central.versionNeeded;
    local->flag = central.flag;
    local->compression = central.compression;
    local->modifiedTime = central.modifiedTime;
    local->modifiedDate = central.modifiedDate;
    local->crc = central.crc;
    local->compressedSize = central.compressedSize;
    local->uncompressedSize = central.uncompressedSize;
    local->fileNameLength = central.fileNameLength;
    local->extraFieldLength = central.extraFieldLength;
}

void timeToDos(time_t time, uint16_t *dosTime, uint16_t *dosDate)
{
    tm *local = localtime(&time);
    *dosTime =
        local->tm_hour<<11 |
        local->tm_min<<5 |
        local->tm_sec/2;
    *dosDate =
        (local->tm_year + 1900 - 1980)<<9 |
        (local->tm_mon + 1)<<5 |
        local->tm_mday;
}

/*
int main()
{
    makeCrcTable();

    string name[3] = {"file1.txt", "file2.txt", "file3.txt"};
    string content[3] = {"understand zip", "abc", ""};

    FILE *f = fopen("uncompressed.zip", "wb");

    uint16_t modTime, modDate;
    timeToDos(time(nullptr), &modTime, &modDate);

    CentralDirectoryHeader central[3];
    for (int i=0; i<3; i++) {
        central[i].versionMade = 0<<8 | 20;
        central[i].versionNeeded = 20;
        central[i].modifiedTime = modTime;
        central[i].modifiedDate = modDate;
        central[i].crc = crc(stringToUint8(content[i]));
        central[i].compressedSize = (uint32_t)content[i].size();
        central[i].uncompressedSize = (uint32_t)content[i].size();
        central[i].fileNameLength = (uint32_t)name[i].size();
        central[i].offset = (uint32_t)ftell(f);

        LocalFileHeader local;
        copyHeader(central[i], &local);

        fwrite(&local, sizeof local, 1, f);
        fwrite(name[i].c_str(), name[i].size(), 1, f);
        fwrite(content[i].c_str(), content[i].size(), 1, f);
    }

    EndOfCentralDirectoryRecord end;
    end.centralDirOffset = (uint32_t)ftell(f);
    for (int i=0; i<3; i++) {
        fwrite(&central[i], sizeof central[i], 1, f);
        fwrite(name[i].c_str(), name[i].size(), 1, f);
    }

    end.entryNumber = 3;
    end.totalEntryNumber = 3;
    end.centralDirSize = (uint32_t)ftell(f) - end.centralDirOffset;
    fwrite(&end, sizeof end, 1, f);

    fclose(f);
}
*/

vector<int> packMerge(vector<int> count, int L)
{
    struct Coin
    {
        int value;
        vector<int> symbol;
        Coin(int value, vector<int> symbol): value(value), symbol(symbol) {}
        bool operator<(const Coin &c) const {return value<c.value;}
    };
    vector<vector<Coin>> denom(L+1);
    size_t n = 0;
    for (size_t i=0; i<count.size(); i++)
        if (count[i]>0) {
            for (int d=0; d<L; d++)
                denom[d].push_back(Coin(count[i], vector<int>(1, (int)n)));
            n++;
        }
    vector<int> lengthTmp(n);
    switch (n) {
    case 0:
        break;
    case 1:
        lengthTmp[0] = 1;
        break;
    default:
        for (int d=0; d<L; d++) {
            sort(denom[d].begin(), denom[d].end());
            for (size_t i=0; i+1<denom[d].size(); i+=2) {
                Coin c = denom[d][i];
                c.value += denom[d][i+1].value;
                c.symbol.insert(c.symbol.end(), denom[d][i+1].symbol.begin(),
                    denom[d][i+1].symbol.end());
                denom[d+1].push_back(c);
            }
        }
        sort(denom[L].begin(), denom[L].end());    
        for (size_t i=0; i<n-1; i++)
            for (int s: denom[L][i].symbol)
                lengthTmp[s]++;
    }
    vector<int> length;
    size_t c = 0;
    for (size_t i=0; i<count.size(); i++)
        length.push_back(count[i]>0 ? lengthTmp[c++] : 0);
    return length;
}

/*
int main()
{
    vector<int> length = packMerge({1, 17, 2, 3, 5, 5}, 4);
    //vector<int> length = packMerge({1, 17, 2, 3, 5, 5}, 3)
    for (size_t i=0; i<length.size(); i++)
        printf("%d %d\n", (int)i, length[i]);
}
*/

class BitStream
{
public:
    vector<uint8_t> data;
    size_t c;
    BitStream(): c(0) {}
    void write(int bit) {
        if (c%8 == 0)
            data.push_back(0);
        data[data.size()-1] |= bit<<c%8;
        c++;
    }
    void writeBits(int bits, int len) {
        for (int i=0; i<len; i++)
            write(bits>>i&1);
    }
};

class Huffman
{
    vector<int> code;
    vector<int> length;
public:
    Huffman(vector<int> length): length(length) {
        code = vector<int>(length.size());
        size_t n = 0;
        for (int l: length)
            if (l==0)
                n++;
        int c = 0;
        for (int l=1; n<code.size(); l++, c<<=1)
            for (size_t i=0; i<length.size(); i++)
                if (length[i]==l) {
                    code[i] = c++;
                    n++;
                }
    }
    void write(int symbol, BitStream *stream) {
        for (int i=length[symbol]-1; i>=0; i--)
            stream->write(code[symbol]>>i&1);
    }
};

/*
int main()
{
    Huffman huffman({4, 1, 4, 3, 3, 3});
    BitStream stream;
    for (int i=0; i<6; i++)
        huffman.write(i, &stream);
    for (uint8_t c: stream.data)
        printf(" %02x", c);
    printf("\n");
    //  e7 d3 01
}
*/

struct Code
{
    int code = 0;
    int ext = 0;
    int extLen = 0;
    Code() {}
    Code(int code, int ext, int extLen): code(code), ext(ext), extLen(extLen) {}
};

//  valueからコードと拡張ビットを求める
Code calcCodeExt(int value, const int *table, int tableLen, int offset)
{
    for (int i=0; i<tableLen; i++) {
        if (value <= 1<<table[i])
            return Code(i+offset, value, table[i]);
        value -= 1<<table[i];
    }
    //  erorr
    return Code();
}

vector<uint8_t> deflate(vector<uint8_t> data)
{
    //  リテラル・一致長と一致距離の拡張ビット長
    static int literalExtTable[29] = {
        0, 0, 0, 0, 0, 0, 0, 0,  1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4,  5, 5, 5, 5, 0,
    };
    static int distExtTable[30] = {
        0, 0, 0, 0, 1, 1, 2, 2,  3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9,10,10, 11,11,12,12,13,13,
    };

    //  入力をリテラル・一致長と一致距離に圧縮する
    vector<Code> literal;
    vector<Code> dist;
    //  添え字の3文字が過去に出現した位置を記録する
    map<uint32_t, size_t> hash;
    for (size_t i=0; i<data.size();)
    {
        int len = 0;
        if (i+3 <= data.size()) {
            uint32_t h = data[i]<<16 | data[i+1]<<8 | data[i+2];
            if (hash.count(h) > 0 && i-hash[h] <= 32768) {
                //  過去に出現していれば、一致長と一致距離に圧縮
                size_t p = hash[h];
                len = 3;
                while (len<258 && i+len<data.size() && data[p+len]==data[i+len])
                    len++;
                literal.push_back(calcCodeExt(len-3, literalExtTable, 29, 257));
                dist.push_back(
                    calcCodeExt((int)(i-hash[h]), distExtTable, 30, 0));
            }
        }
        if (len == 0) {
            //  過去に出現していなければ、そのままリテラルとして書き出す
            literal.push_back(Code(data[i], 0, 0));
            dist.push_back(Code(-1, 0, 0));
            len = 1;
        }
        for (int j=0; j<len; j++)
            if (2 <= i+j)
                hash[data[i+j-2]<<16 | data[i+j-1]<<8 | data[i+j]] = i+j-2;
        i += len;
    }
    literal.push_back(Code(256, 0, 0));
    dist.push_back(Code(-1, 0, 0));

    //  リテラル・一致長と一致距離の最適なビット長を求める
    vector<int> literalCount(286);
    for (Code l: literal)
        literalCount[l.code]++;
    vector<int> literalLen = packMerge(literalCount, 15);
    vector<int> distCount(30);
    for (Code d: dist)
        if (d.code>=0)
            distCount[d.code]++;
    vector<int> distLen = packMerge(distCount, 15);

    //  ビット長の配列を作り、圧縮する
    int HLIT = 286 - 257;
    while (0<HLIT && literalLen[HLIT+257-1]==0)
        HLIT--;
    int HDIST = 30 - 1;
    while (0<HDIST && distLen[HDIST+1-1]==0)
        HDIST--;
    
    vector<int> codeLen;
    for (int i=0; i<HLIT+257; i++)
        codeLen.push_back(literalLen[i]);
    for (int i=0; i<HDIST+1; i++)
        codeLen.push_back(distLen[i]);
    vector<Code> codeLenCode;
    for (size_t i=0; i<codeLen.size();) {
        int prevLen = 0;
        if (i>0)
            while (prevLen<6 && i+prevLen<codeLen.size() &&
                codeLen[i+prevLen]==codeLen[i-1])
                prevLen++;
        int zeroLen = 0;
        while (zeroLen<138 && i+zeroLen<codeLen.size() &&
            codeLen[i+zeroLen]==0)
            zeroLen++;
        if (zeroLen>=11) {
            codeLenCode.push_back(Code(18, zeroLen-11, 7));
            i += zeroLen;
        } else if (zeroLen>=3) {
            codeLenCode.push_back(Code(17, zeroLen-3, 3));
            i += zeroLen;
        } else if (prevLen>=3) {
            codeLenCode.push_back(Code(16, prevLen-3, 3));
            i += prevLen;
        } else {
            codeLenCode.push_back(Code(codeLen[i], 0, 0));
            i++;
        }
    }
    vector<int> codeLenCount(19);
    for (Code c: codeLenCode)
        codeLenCount[c.code]++;
    vector<int> codeLenCodeLen = packMerge(codeLenCount, 7);

    static int codeLenTrans[] = {
        16,17,18, 0, 8, 7, 9, 6, 10, 5,11, 4,12, 3,13, 2,
        14, 1,15
    };
    int HCLEN = 19 - 4;
    while (0<HCLEN && codeLenCodeLen[codeLenTrans[HCLEN+4-1]]==0)
        HCLEN--;

    //  圧縮結果を書き出す
    BitStream stream;
    stream.write(1);        //  BFINAL = 1
    stream.writeBits(2, 2); //  BTYPE = 10
    
    stream.writeBits(HLIT, 5);
    stream.writeBits(HDIST, 5);
    stream.writeBits(HCLEN, 4);

    for (int i=0; i<HCLEN+4; i++)
        stream.writeBits(codeLenCodeLen[codeLenTrans[i]], 3);

    Huffman huffmanCodeLen(codeLenCodeLen);
    for (size_t i=0; i<codeLenCode.size(); i++) {
        huffmanCodeLen.write(codeLenCode[i].code, &stream);
        stream.writeBits(codeLenCode[i].ext, codeLenCode[i].extLen);
    }

    Huffman huffmanLiteral(literalLen);
    Huffman huffmanDist(distLen);
    for (size_t i=0; i<literal.size(); i++) {
        huffmanLiteral.write(literal[i].code, &stream);
        stream.writeBits(literal[i].ext, literal[i].extLen);
        if (dist[i].code>=0) {
            huffmanDist.write(dist[i].code, &stream);
            stream.writeBits(dist[i].ext, dist[i].extLen);
        }
    }

    return stream.data;
}

/*
int main()
{
    vector<uint8_t> data;
    for (int i=0; i<8; i++) {
        data.push_back(122);
        data.push_back(105);
        data.push_back(112);
    }
    vector<uint8_t> comp = deflate(data);
    for (uint8_t c: comp)
        printf(" %02x", c);
    printf("\n");
    //   6d c2 31 0d 00 00 00 83 30 bd fb 76 a3 1e 03 24 65 4f 02
}
*/

/*
int main()
{
    makeCrcTable();

    FILE *f = fopen("compressed.zip", "wb");

    string name = "zip.txt";
    string content_ = "";
    for (int i=0; i<8; i++)
        content_ += "zip";
    vector<uint8_t> content = stringToUint8(content_);
    vector<uint8_t> compressed = deflate(content);

    CentralDirectoryHeader central;
    central.versionMade = 0<<8 | 20;
    central.versionNeeded = 20;
    central.compression = 8;    //  Deflate
    central.crc = crc(content);
    central.compressedSize = (uint32_t)compressed.size();
    central.uncompressedSize = (uint32_t)content.size();
    central.fileNameLength = (uint32_t)name.size();
    central.offset = 0;

    LocalFileHeader local;
    copyHeader(central, &local);

    fwrite(&local, sizeof local, 1, f);
    fwrite(name.c_str(), name.size(), 1, f);
    fwrite(&compressed[0], compressed.size(), 1, f);

    EndOfCentralDirectoryRecord end;
    end.centralDirOffset = (uint32_t)ftell(f);

    fwrite(&central, sizeof central, 1, f);
    fwrite(name.c_str(), name.size(), 1, f);

    end.entryNumber = 1;
    end.totalEntryNumber = 1;
    end.centralDirSize = (uint32_t)ftell(f) - end.centralDirOffset;
    fwrite(&end, sizeof end, 1, f);

    fclose(f);
}
*/

struct Key
{
    uint32_t key0 = 0x12345678U;
    uint32_t key1 = 0x23456789U;
    uint32_t key2 = 0x34567890U;
    void update_keys(uint8_t c) {
        key0 = crc32(key0, c);
        key1 = (key1 + (key0&0xff)) * 0x08088405U + 1;
        key2 = crc32(key2, key1>>24);
    }
    uint8_t decrypt_byte() {
        uint16_t temp = (uint16_t)(key2 | 2);
        return (temp*(temp^1))>>8;
    }
};

/*
int main()
{
    makeCrcTable();
    Key key;
    for (uint8_t c=0; c<8; c++) {
        key.update_keys(c);
        printf(" %02x", key.decrypt_byte());
    }
    printf("\n");
    //  1a 63 54 9f e7 41 0e 83
}
*/

int main()
{
    makeCrcTable();

    FILE *f = fopen("encrypted.zip", "wb");

    string name = "secret.txt";
    vector<uint8_t> content = stringToUint8("secret message");
    vector<uint8_t> password = stringToUint8("p@ssw0rd");

    vector<uint8_t> encryption(12);
    random_device rand;
    for (size_t i=0; i<11; i++)
        encryption[i] = (uint8_t)rand();
    uint32_t crc32 = crc(content);
    encryption[11] = (uint8_t)(crc32>>24);

    //  暗号化
    Key key;
    for (uint8_t c: password)
        key.update_keys(c);
    for (uint8_t &c: encryption) {
        uint8_t t = key.decrypt_byte();
        key.update_keys(c);
        c ^= t;
    }
    for (uint8_t &c: content) {
        uint8_t t = key.decrypt_byte();
        key.update_keys(c);
        c ^= t;
    }

    CentralDirectoryHeader central;
    central.versionMade = 0<<8 | 20;
    central.versionNeeded = 20;
    central.flag = 1;   //  encrypted
    central.crc = crc32;
    central.compressedSize = (uint32_t)content.size() + 12;
    central.uncompressedSize = (uint32_t)content.size();
    central.fileNameLength = (uint32_t)name.size();
    central.offset = 0;

    LocalFileHeader local;
    copyHeader(central, &local);

    fwrite(&local, sizeof local, 1, f);
    fwrite(name.c_str(), name.size(), 1, f);
    fwrite(&encryption[0], 12, 1, f);
    fwrite(&content[0], content.size(), 1, f);

    EndOfCentralDirectoryRecord end;
    end.centralDirOffset = (uint32_t)ftell(f);

    fwrite(&central, sizeof central, 1, f);
    fwrite(name.c_str(), name.size(), 1, f);

    end.entryNumber = 1;
    end.totalEntryNumber = 1;
    end.centralDirSize = (uint32_t)ftell(f) - end.centralDirOffset;
    fwrite(&end, sizeof end, 1, f);

    fclose(f);
}
