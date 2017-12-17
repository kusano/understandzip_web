#include <cstdio>
#include <cstdint>
#include <vector>
#include <set>
using namespace std;

//  暗号文
vector<uint8_t> C;
//  平文
vector<uint8_t> P;

uint32_t crcTable[256];
uint32_t crcTableInv[256];
void makeCrcTable()
{
    for (int i=0; i<256; i++) {
        uint32_t c = i;
        for (int j=0; j<8; j++)
            c = c>>1 ^ ((c&1)!=0 ? 0xedb88320 : 0);
        crcTable[i] = c;
        crcTableInv[c>>24] = c<<8^i;
    }
}

uint32_t crc32(uint32_t c, uint8_t d)
{
    return c>>8 ^ crcTable[c&0xff^d];
}

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

uint32_t key2Table[256][64];
void makeKey2Table()
{
    Key key;
    int count[256] = {};
    for (uint32_t key2=0; key2<0x10000; key2+=4) {
        key.key2 = key2;
        uint8_t d = key.decrypt_byte();
        key2Table[d][count[d]++] = key2;
    }
}

uint32_t key1[12];
uint32_t key2[12];

uint32_t key1inv = 0xd94fa8cdU; //  key1inv * 0x08088405 = 1

bool guessKey0()
{
    uint32_t key0 = 0;
    for (int p=7; p>=4; p--) {
        uint8_t lsb = (key1[p]-1)*key1inv - key1[p-1];
        key0 = (key0 ^ crcTable[lsb^P[p]])<<8 | lsb;
    }
    bool ok = true;
    uint32_t k0 = key0;
    for (int i=4; i<12; i++) {
        uint8_t lsb = (key1[i]-1)*key1inv - key1[i-1];
        if ((k0&0xff) != lsb) {
            ok = false;
            break;
        }
        k0 = k0>>8 ^ crcTable[lsb^P[i]];
    }
    if (ok)
        printf("Key found! pos=4, key0=%08x, key1=%08x, key2=%08x\n",
            key0, key1[4], key2[4]);
    return ok;
}

bool guessKey1(int p)
{
    if (p==2)
        return guessKey0();

    uint32_t base;
    int count;
    if (p==11) {
        uint8_t msb = key2[p]<<8 ^ crcTableInv[key2[p]>>24] ^ key2[p-1];
        //  base-iで最上位バイトがmsbになる
        base = ((msb+1)<<24)-1;
        count = 0x1000000;
    } else {
        base = (key1[p+1]-1)*key1inv;
        count = 0x100;
    }
    uint8_t prevMsb = key2[p-1]<<8 ^ crcTableInv[key2[p-1]>>24] ^ key2[p-2];
    for (int i=0; i<count; i++) {
        uint32_t k1 = base - i;
        if (((k1-1)*key1inv     )>>24 == prevMsb &&
            ((k1-1)*key1inv-0xff)>>24 == prevMsb) {
            key1[p] = k1;
            if (guessKey1(p-1))
                return true;
        }
    }
    return false;
}

/*
bool guessKey2(int p)
{
    if (p == -1) {
        for (int i=0; i<12; i++)
            printf(" %08x", key2[i]);
        printf("\n");
        return false;
    }

    uint32_t inv = crcTableInv[key2[p+1]>>24];
    for (int i=0; i<64; i++) {
        uint32_t left = key2Table[C[p+12]^P[p]][i];
        uint32_t right = key2[p+1]<<8 ^ inv;
        if ((left&0xfc00) == (right&0xfc00)) {
            key2[p] = right&0xffff0000 | left;
            key2[p+1] = key2[p+1]&~0x3 | (left^inv)>>8&0x3;
            if (guessKey2(p-1))
                return true;
        }
    }
    return false;
}

/*
void attack() {
    for (uint32_t i=0; i<0x10000; i++)
        for (int j=0; j<64; j++) {
            key2[11] = i<<16 | key2Table[C[11+12]^P[11]][j];
            guessKey2(10);
        }
}
//*/

//*
bool guessKey2(int p)
{
    if (p == -1)
        return guessKey1(11);

    uint32_t inv = crcTableInv[key2[p+1]>>24];
    for (int i=0; i<64; i++) {
        uint32_t left = key2Table[C[p+12]^P[p]][i];
        uint32_t right = key2[p+1]<<8 ^ inv;
        if ((left&0xfc00) == (right&0xfc00)) {
            key2[p] = right&0xffff0000 | left;
            key2[p+1] = key2[p+1]&~0x3 | (left^inv)>>8&0x3;
            if (guessKey2(p-1))
                return true;
        }
    }
    return false;
}
//*/

//*
void attack() {
    size_t n = 10000;   //  <= P.size();
    
    set<uint32_t> key2s;
    for (uint32_t i=0; i<0x10000; i++)
        for (int j=0; j<64; j++)
            key2s.insert(i<<16 | key2Table[C[n-1+12]^P[n-1]][j]);
    printf("%d\n", (int)key2s.size());
    
    for (size_t p=n-2; p>=11; p--) {
        set<uint32_t> key2sTmp;
        for (uint32_t k2: key2s)
            for (int i=0; i<64; i++) {
                uint32_t left = key2Table[C[p+12]^P[p]][i];
                uint32_t right = k2<<8 ^ crcTableInv[k2>>24];
                if ((left&0xfc00) == (right&0xfc00))
                    key2sTmp.insert(right&0xffff0000 | left);
            }
        key2s = key2sTmp;
        printf("%d %d\n", (int)p, (int)key2s.size());
    }

    for (uint32_t k2: key2s) {
        printf("Try key2=%08x\n", k2);
        key2[11] = k2;
        if (guessKey2(10))
            break;
    }
}
//*/


/*
int main()
{
    makeCrcTable();
    makeKey2Table();

    size_t n = 0x5517b;
    //  先頭12バイト個はencryption header
    C = vector<uint8_t>(n+12);
    P = vector<uint8_t>(n);

    FILE *f = fopen("secret.zip", "rb");
    fseek(f, 0x46, SEEK_SET);
    fread(&C[0], 1, n+12, f);
    fclose(f);

    f = fopen("kusano_k.png", "rb");
    fread(&P[0], 1, n, f);
    fclose(f);

    attack();
}
//*/

//  Key found! pos=4, key0=75a4dda6, key1=0eaf62f4, key2=4e243990

/*
int main()
{
    makeCrcTable();

    vector<uint8_t> known(0x55187);
    vector<uint8_t> secret(0x39);

    FILE *f = fopen("secret.zip", "rb");
    fseek(f, 0x46, SEEK_SET);
    fread(&known[0], 1, known.size(), f);
    fseek(f, 0x55221, SEEK_SET);
    fread(&secret[0], 1, secret.size(), f);
    fclose(f);

    uint32_t key0 = 0x75a4dda6;
    uint32_t key1 = 0x0eaf62f4;
    uint32_t key2 = 0x4e243990;
    for (int p=12+3; p>=0; p--) {
        key2 = key2<<8 ^ crcTableInv[key2>>24] ^ key1>>24;
        key1 = (key1-1)*key1inv - (key0&0xff);
        uint8_t plain = ((key2|2)*((key2|2)^1))>>8&0xff ^ known[p];
        key0 = key0<<8 ^ crcTableInv[key0>>24] ^ plain;
    }
    printf("%08x %08x %08x\n", key0, key1, key2);

    Key key;
    key.key0 = key0;
    key.key1 = key1;
    key.key2 = key2;
    for (uint8_t &c: secret) {
        c ^= key.decrypt_byte();
        key.update_keys(c);
    }

    printf("%.*s\n", (int)(secret.size()-12), (const char *)&secret[12]);
}
//*/

/*
int main()
{
    makeCrcTable();
    uint32_t basis[32] = {};
    for (uint32_t x=0xffffffff; x>0; x--) {
        uint32_t crc = 0;
        for (int i=0; i<32; i+=8)
            crc = crc32(crc, (uint8_t)(x>>i));
        for (int i=0; i<32; i++)
            if (crc == 1<<i)
                basis[i] = x;
    }
    for (int i=0; i<32; i++)
        printf("%08x %08x\n", 1<<i, basis[i]);
}
//*/

//  CRCがstartからendに変換する入力を求める
vector<uint8_t> recoverCrc(uint32_t start, uint32_t end, int n)
{
    uint32_t basis[32] = {
        0xdb710641U, 0x6d930ac3U, 0xdb261586U, 0x6d3d2d4dU,
        0xda7a5a9aU, 0x6f85b375U, 0xdf0b66eaU, 0x6567cb95U,
        0xcacf972aU, 0x4eee2815U, 0x9ddc502aU, 0xe0c9a615U,
        0x1ae24a6bU, 0x35c494d6U, 0x6b8929acU, 0xd7125358U,
        0x7555a0f1U, 0xeaab41e2U, 0x0e278585U, 0x1c4f0b0aU,
        0x389e1614U, 0x713c2c28U, 0xe2785850U, 0x1f81b6e1U,
        0x3f036dc2U, 0x7e06db84U, 0xfc0db708U, 0x236a6851U,
        0x46d4d0a2U, 0x8da9a144U, 0xc02244c9U, 0x5b358fd3U,
    };

    for (int i=0; i<n; i++)
        start = start>>8 ^ crcTable[start&0xff];

    uint32_t x = 0;
    for (int i=0; i<32; i++)
        if (((start^end)>>i&1) != 0)
            x ^= basis[i];

    vector<uint8_t> input;
    for (int i=4-n; i<4; i++)
        input.push_back(x>>(8*i)&0xff);
    return input;
}

bool checkPassword(uint32_t key0, uint32_t key1, uint32_t key2,
    vector<uint8_t> password)
{
    Key key;
    for (uint8_t c: password)
        key.update_keys(c);
    return key.key0==key0 && key.key1==key1 && key.key2==key2;
}

/*
int main()
{
    makeCrcTable();

    uint32_t key0 = 0x58927f21U;
    uint32_t key1 = 0xc0b74fb5U;
    uint32_t key2 = 0x8297062bU;

    vector<uint8_t> password;
    for (int n=0; n<=4; n++) {
        password = recoverCrc(0x12345678U, key0, n);
        if (checkPassword(key0, key1, key2, password))
            break;
    }
    printf("%.*s\n", (int)password.size(), &password[0]);
}
*/

//  prefixに6文字を繋げたパスワードを探索
bool searchPassword6(vector<uint8_t> prefix, uint32_t key0_6, uint32_t key1_6,
    uint32_t key2_6)
{
    Key key;
    for (uint8_t c: prefix)
        key.update_keys(c);
    uint32_t key0_0 = key.key0;
    uint32_t key1_0 = key.key1;
    uint32_t key2_0 = key.key2;

    uint32_t key2_5 = key2_6<<8 ^ crcTableInv[key2_6>>24] ^ key1_6>>24;
    uint32_t key1_5 = (key1_6-1)*key1inv - (key0_6&0xff);
    uint32_t key2_4 = key2_5<<8 ^ crcTableInv[key2_5>>24] ^ key1_5>>24;

    vector<uint8_t> key1msb = recoverCrc(key2_0, key2_4, 4);
    key1msb.insert(key1msb.begin(), 0);

    //  key0_1とkey0_2のLSBを探索
    for (int key0_1lsb=0; key0_1lsb<0x100; key0_1lsb++) {
    uint32_t key1_1 = (key1_0 + key0_1lsb)*0x08088405U + 1;
    if (key1_1>>24 == key1msb[1]) {
        for (int key0_2lsb=0; key0_2lsb<0x100; key0_2lsb++) {
        uint32_t key1_2 = (key1_1 + key0_2lsb)*0x08088405U + 1;
        if (key1_2>>24 == key1msb[2]) {
            //  パスワードの先頭2文字を探索
            for (int password_0=0; password_0<0x100; password_0++) {
            uint32_t key0_1 = crc32(key0_0, password_0);
            if ((key0_1&0xff) == key0_1lsb) {
                for (int password_1=0; password_1<0x100; password_1++) {
                uint32_t key0_2 = crc32(key0_1, password_1);
                if ((key0_2&0xff) == key0_2lsb) {
                    //  パスワードの残り4文字を探索
                    vector<uint8_t> pass4 = recoverCrc(key0_2, key0_6, 4);
                    vector<uint8_t> password = prefix;
                    password.push_back(password_0);
                    password.push_back(password_1);
                    password.insert(password.end(), pass4.begin(), pass4.end());
                    if (checkPassword(key0_6, key1_6, key2_6, password)) {
                        printf("Password: %.*s\n", (int)password.size(),
                            &password[0]);
                        return true;
                    }
                }}
            }}
        }}
    }}
    return false;
}

bool searchPassword(vector<uint8_t> prefix, uint32_t key0, uint32_t key1,
    uint32_t key2, int l)
{
    if (l==6)
        return searchPassword6(prefix, key0, key1, key2);
    for (int i=0; i<0x100; i++) {
        prefix.push_back(i);
        if (searchPassword(prefix, key0, key1, key2,l-1))
            return true;
        prefix.pop_back();
    }
    return false;
}

//*
int main()
{
    makeCrcTable();

    uint32_t key0 = 0xe4b115e8U;
    uint32_t key1 = 0x01b5f84cU;
    uint32_t key2 = 0x98b20d06U;

    bool found = false;
    for (int l=0; !found; l++) {
        printf("l = %d\n", l);
        if (l<=4) {
            vector<uint8_t> password = recoverCrc(0x12345678U, key0, l);
            if (checkPassword(key0, key1, key2, password)) {
                printf("Password: %.*s\n", (int)password.size(), &password[0]);
                found = true;
            }
        } else if (l==5) {
            for (int p=0; p<0x100; p++) {
                Key key;
                key.update_keys((uint8_t)p);
                vector<uint8_t> pass4 = recoverCrc(key.key0, key0, 4);
                vector<uint8_t> password(1, (uint8_t)p);
                password.insert(password.end(), pass4.begin(), pass4.end());
                if (checkPassword(key0, key1, key2, password)) {
                    printf("Password: %.*s\n", (int)password.size(),
                        &password[0]);
                    found = true;
                }
            }
        } else
            if (searchPassword({}, key0, key1, key2, l))
                found = true;
    }
}
//*/