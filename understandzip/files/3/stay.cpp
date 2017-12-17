#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
using namespace std;

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

uint8_t R[5][10];
uint8_t C[5][12];

void guessRand()
{
    for (int i=0; ; i++) {
        int seed = time(nullptr)^i;
        srand(seed);
        bool ok = true;
        for (int j=0; j<5; j++) {
            if ((rand()>>7&0xff) != C[j][0])
                ok = false;
            for (int k=0; k<9; k++)
                rand();
        }
        if (ok) {
            printf("Seed: %08x\n", seed);
            srand(seed);
            for (int j=0; j<5; j++)
                for (int k=0; k<10; k++)
                    R[j][k] = rand()>>7&0xff;
            break;
        }
    }
}

uint8_t decrypt_byte(uint32_t key2) {
    uint16_t temp = (uint16_t)(key2 | 2);
    return (temp*(temp^1))>>8;
}

uint8_t msb(uint32_t x) {
    return x>>24;
}

uint32_t key0_crc;      //  crc32(key0, 0)
uint32_t key1_mul1;     //  key1*0x08088405
uint32_t key1_mul2;     //  key1*0x08088405*0x08088405
uint32_t key2_crc;      //  crc32(key2, 0)
uint32_t s0[5][2];     //  s0[f][p] = R[f][p]^P[f][p]
uint32_t s1[5][2];     //  s1[f][p] = P[f][p]^C[f][p]

uint8_t key0_10lsb[5], key0_11lsb[5];   //  LSB(key0[1])
uint8_t key1_10msb[5], key1_11msb[5];   //  MSB(key1[1])
uint8_t key0_20lsb[5], key0_21lsb[5];   //  LSB(key0[2])
uint8_t key1_20msb[5], key1_21msb[5];   //  MSB(key1[2])

uint32_t key0;
uint32_t key1;
uint32_t key2;

bool attack6()
{
    //  key0
    for (uint32_t g0=0; g0<0x100; g0++)
    for (uint32_t g1=0; g1<0x100; g1++) {
        uint32_t key0_h = g1<<16 | key0_crc;
        key0 = (key0_h^ crcTable[g0])<<8 | g0;

        Key keys0;
        keys0.key0 = key0;
        keys0.key1 = key1;
        keys0.key2 = key2;
        Key keys1 = keys0;
        bool ok = true;
        for (int i=0; i<10 && ok; i++) {
            uint8_t t = C[0][i] ^ keys1.decrypt_byte();
            keys1.update_keys(t);
            t ^= keys0.decrypt_byte();
            keys0.update_keys(t);
            if (t != R[0][i])
                ok = false;
        }
        if (ok)
            return true;
    }
    return false;
}

bool attack5()
{
    //  key1
    for (uint32_t g12=0; g12<0x1000000; g12++) {
        key1 = (key1_mul1 | g12) * 0xd94fa8cdU;
        if (msb(key1*0xd4652819U) == msb(key1_mul2)) {
            bool ok = true;
            for (int f=0; f<5 && ok; f++) {
                uint32_t k1 = (key1 + key0_10lsb[f])*0x08088405U + 1;
                if (msb(k1)!=key1_10msb[f]) {ok=false; break;}
                k1 = (k1 + key0_20lsb[f])*0x08088405U + 1;
                if (msb(k1)!=key1_20msb[f]) {ok=false; break;}
                k1 = (key1 + key0_11lsb[f])*0x08088405U + 1;
                if (msb(k1)!=key1_11msb[f]) {ok=false; break;}
                k1 = (k1 + key0_21lsb[f])*0x08088405U + 1;
                if (msb(k1)!=key1_21msb[f]) {ok=false; break;}
            }
            if (ok)
                if (attack6())
                    return true;
        }
    }
    return false;
}

bool attack4()
{
    printf("  attack4 %08x %08x %08x\n", key0_crc, key1_mul2, key2_crc);
    //  key2
    for (uint32_t g11=0; g11<0x100; g11++) {
        key2 = (key2_crc ^ crcTable[g11])<<8 | g11;
        if (decrypt_byte(key2) == s0[0][0])
            if (attack5())
                return true;
    }
    return false;
}

bool attack3(int f)
{
    if (f==5)
        return attack4();

    for (uint32_t g0=0; g0<2; g0++)
    for (uint32_t g1=0; g1<2; g1++) {
        key0_20lsb[f] = crc32(key0_crc^crcTable[R[f][0]], R[f][1]);
        key1_20msb[f] = msb(key1_mul2) + 0x08 + g0 +
            msb(key0_10lsb[f]*0xd4652819+key0_20lsb[f]*0x08088405);
        uint32_t k20 = crc32(key2_crc^crcTable[key1_10msb[f]], key1_20msb[f]);
        uint8_t s20 = decrypt_byte(k20);
        key0_21lsb[f] = crc32(key0_crc^crcTable[R[f][0]^s0[0][0]],
            R[f][1]^s0[f][1]);
        key1_21msb[f] = msb(key1_mul2) + 0x08 + g1 +
            msb(key0_11lsb[f]*0xd4652819+key0_21lsb[f]*0x08088405);
        uint32_t k21 = crc32(key2_crc^crcTable[key1_11msb[f]], key1_21msb[f]);
        uint8_t s21 = decrypt_byte(k21);
        if ((R[f][2]^s20^s21) == C[f][2])
            if (attack3(f+1))
                return true;
    }
    return false;
}

bool attack2()
{
    printf("attack2 %08x %08x %08x\n", key0_crc, key1_mul1, key2_crc);

    //  crc32(key0,0) & 0x0000ff00
    for (uint32_t g0=0; g0<0x100; g0++) {   //  0xdb
        key0_crc = key0_crc&~0xff00 | g0<<8;
        //  MSB(key1*0x08088405*0x08088405)
        for (uint32_t g1=0; g1<0x100; g1++) {   //  0x7f
            key1_mul2 = g1<<24;
            //  crc32(key2,0)&0x00ff0003
            for (uint32_t g2=0; g2<0x100; g2++) //  0xfb
            for (uint32_t g3=0; g3<4; g3++) {   //  0
                key2_crc = g2<<16 | key2_crc&~0xff0003 | g3;
                if (attack3(0))
                    return true;
            }
        }
    }
    return false;
}

bool attack1(int f)
{
    if (f==5)
        return attack2();

    for (uint32_t g0=0; g0<2; g0++)
    for (uint32_t g1=0; g1<2; g1++) {
        key0_10lsb[f] = key0_crc^crcTable[R[f][0]];
        key1_10msb[f] = msb(key1_mul1) + msb(key0_10lsb[f]*0x08088405) + g0;
        s0[f][1] = decrypt_byte(crcTable[key1_10msb[f]] ^ key2_crc);
        key0_11lsb[f] = key0_crc^crcTable[R[f][0]^s0[0][0]];
        key1_11msb[f] = msb(key1_mul1) + msb(key0_11lsb[f]*0x08088405) + g1;
        s1[f][1] = decrypt_byte(crcTable[key1_11msb[f]] ^ key2_crc);
        if ((R[f][1]^s0[f][1]^s1[f][1]) == C[f][1])
            if (attack1(f+1))
                return true;
    }
    return false;
}

bool attack0()
{
    //  LSB(crc32(key0,0))
    for (uint32_t g0=0; g0<0x100; g0++) {   //  0x5f
        key0_crc = g0;
        //  MSB(key1*0x08088405)
        for (uint32_t g1=0; g1<0x100; g1++) {   //   0xfe
            key1_mul1 = g1<<24;
            //  crc32(key2,0)&0x0000fffc
            for (uint32_t g2=0; g2<0x4000; g2++) {  //  0x5ce
                key2_crc = g2<<2;
                //  R[x][0]^P[x][0]
                for (uint32_t g3=0; g3<0x100; g3++) {   //  0xa9
                    s0[0][0] = g3;
                    if (attack1(0))
                        return true;
                }
            }
        }
    }
    return false;
}

int main()
{
    makeCrcTable();

    int pos[5] = {0x43, 0xa7, 0x10a, 0x16d, 0x1d1};
    FILE *f = fopen("rabbit.zip", "rb");
    for (int i=0; i<5; i++) {
        fseek(f, pos[i], SEEK_SET);
        fread(C[i], 1, 12, f);
    }
    fclose(f);

    guessRand();
    //uint8_t R_[5][10] = {
    //    {0xdd, 0xb7, 0x42, 0x72, 0x6c, 0x74, 0x9d, 0x9c, 0xe6, 0x40},
    //    {0xf7, 0x39, 0xf5, 0x01, 0x4f, 0x1b, 0x45, 0xcf, 0x60, 0x3f},
    //    {0xc8, 0xfa, 0x4b, 0xec, 0x32, 0x77, 0xab, 0x9d, 0x58, 0xfd},
    //    {0x24, 0x35, 0xb5, 0x67, 0xa7, 0x21, 0xdc, 0x45, 0xbd, 0xc2},
    //    {0x85, 0xb5, 0xfb, 0x7a, 0xb6, 0x4b, 0x95, 0xfb, 0x1b, 0xf6},
    //};
    //memcpy(R, R_, sizeof R);


    if (attack0()) {
        printf("key0: %08x\n", key0);
        printf("key1: %08x\n", key1);
        printf("key2: %08x\n", key2);
    }

    printf("%.2f\n", (double)clock()/CLOCKS_PER_SEC);
}
