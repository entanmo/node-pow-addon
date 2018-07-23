typedef struct {
    int count;
    ulong nonce;
    uint hash[8];
}result;

typedef union {
    uchar flag;
    uint data;
} CheckEndian;

#ifndef uint32_t
#define uint32_t unsigned int
#endif

#define H0 0x6a09e667
#define H1 0xbb67ae85
#define H2 0x3c6ef372
#define H3 0xa54ff53a
#define H4 0x510e527f
#define H5 0x9b05688c
#define H6 0x1f83d9ab
#define H7 0x5be0cd19


uint rotr(uint x, int n) {
  if (n < 32) return (x >> n) | (x << (32 - n));
  return x;
}

uint ch(uint x, uint y, uint z) {
  return (x & y) ^ (~x & z);
}

uint maj(uint x, uint y, uint z) {
  return (x & y) ^ (x & z) ^ (y & z);
}

uint sigma0(uint x) {
  return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

uint sigma1(uint x) {
  return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

uint gamma0(uint x) {
  return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

uint gamma1(uint x) {
  return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

char _hex(char src) {
    switch (src) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:  return src + '0';
        case 10: return 'a';
        case 11: return 'b';
        case 12: return 'c';
        case 13: return 'd';
        case 14: return 'e';
        case 15: return 'f';
        default: return ' ';
    }
}

void gethex(uint src, int index, char *hex) {
    CheckEndian endian;
    endian.data = 1;
    if (endian.flag == 1) {
        hex[0] = _hex((((uchar *)&src)[3-index]) / 16);
        // printf("%c", _hex((((uchar *)&src)[3-index]) / 16));
        hex[1] = _hex((((uchar *)&src)[3-index]) % 16);
        // printf("%c", _hex((((uchar *)&src)[3-index]) % 16));
    } else {
        hex[0] = _hex(((uchar *)&src)[index] / 16);
        // printf("%c", _hex(((uchar *)&src)[index] / 16));
        hex[1] = _hex(((uchar *)&src)[index] % 16);
        // printf("%c", _hex(((uchar *)&src)[index] % 16));
    }
    // printf("%c%c", hex[0], hex[1]);
}

void tohex(uint src, char *hex) {
    gethex(src, 0, &hex[0]);
    gethex(src, 1, &hex[2]);
    gethex(src, 2, &hex[4]);
    gethex(src, 3, &hex[6]);
}

__constant uint K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

int number2chararray(ulong number, char *data, int len, int *index) {
    if (number == 0) {
        data[len] = '0';
        *index = len;
        return 1;
    }

    char base;
    int iter = len;
    while (number != 0) {
        base = number % 10;
        number = number / 10;
        data[iter--] = base + '0';
    }
    *index = iter + 1;
    return len - iter;
}

__kernel void search_nonce(__global char *message, uint msgsize, __global char *target, uint targetsize, ulong startNonce, __global result *output) {
    uint hash[8] = { 0 };
    uint W[80];
    uint gid = get_global_id(0);
    // startNonce = 1024;
    ulong nonce = startNonce + gid;
    char chnonce[65] = { 0 };
    int index = 0;
    int count = number2chararray(nonce, chnonce, 63, &index);
    char *pnonce = chnonce + index;
    uint ulen = msgsize + count;
    uint total = ulen % 64 >= 56 ? 2 : 1 + ulen / 64;

    hash[0] = H0;
    hash[1] = H1;
    hash[2] = H2;
    hash[3] = H3;
    hash[4] = H4;
    hash[5] = H5;
    hash[6] = H6;
    hash[7] = H7;
    for (uint item = 0; item < total; ++item) {
        uint A = hash[0], B = hash[1], C = hash[2], D = hash[3],
            E = hash[4], F = hash[5], G = hash[6], H = hash[7];
        
#pragma unroll
        for (int t = 0; t < 80; ++t) {
            W[t] = 0x00000000;
        }
        int msg_pad = item * 64, current_pad;
        if (ulen > msg_pad) {
            current_pad = (ulen -  msg_pad) > 64 ? 64 : (ulen - msg_pad);
        } else {
            current_pad = -1;
        }

        if (current_pad > 0) {
            uint i = current_pad;
            int stop = i / 4;
            int t;
            int i1, i2, i3, i4;
            for (t = 0; t < stop; ++t) {
                i1 = msg_pad + t * 4;
                i2 = msg_pad + t * 4 + 1;
                i3 = msg_pad + t * 4 + 2;
                i4 = msg_pad + t * 4 + 3;
                if (i1 >= msgsize) {
                    W[t] = ((uchar)pnonce[i1 - msgsize]) << 24;
                } else {
                    W[t] = ((uchar)message[i1]) << 24;
                }
                if (i2 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i2 - msgsize]) << 16;
                } else {
                    W[t] |= ((uchar)message[i2]) << 16;
                }
                if (i3 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i3 - msgsize]) << 8;
                } else {
                    W[t] |= ((uchar)message[i3]) << 8;
                }
                if (i4 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i4 - msgsize]);
                } else {
                    W[t] |= ((uchar)message[i4]);
                }
            }
            int mmod = i % 4;
            i1 = msg_pad + t * 4;
            i2 = msg_pad + t * 4 + 1;
            i3 = msg_pad + t * 4 + 2;
            i4 = msg_pad + t * 4 + 3;
            if (mmod == 3) {
                if (i1 >= msgsize) {
                    W[t] = ((uchar)pnonce[i1 - msgsize]) << 24;
                } else {
                    W[t] = ((uchar)message[i1]) << 24;
                }
                if (i2 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i2 - msgsize]) << 16;
                } else {
                    W[t] |= ((uchar)message[i2]) << 16;
                }
                if (i3 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i3 - msgsize]) << 8;
                } else {
                    W[t] |= ((uchar)message[i3]) << 8;
                }
                W[t] |= ((uchar)0x80);
            } else if (mmod == 2) {
                if (i1 >= msgsize) {
                    W[t] = ((uchar)pnonce[i1 - msgsize]) << 24;
                } else {
                    W[t] = ((uchar)message[i1]) << 24;
                }
                if (i2 >= msgsize) {
                    W[t] |= ((uchar)pnonce[i2 - msgsize]) << 16;
                } else {
                    W[t] |= ((uchar)message[i2]) << 16;
                }
                W[t] |= 0x8000;
            } else if (mmod == 1) {
                if (i1 >= msgsize) {
                    W[t] = ((uchar)pnonce[i1 - msgsize]) << 24;
                } else {
                    W[t] = ((uchar)message[i1]) << 24;
                }
                W[t] |= 0x800000;
            } else {
                W[t] = 0x80000000;
            }

            if (current_pad < 56) {
                W[15] = ulen * 8;
            }

        } else if (current_pad < 0) {
            if (ulen % 64 == 0) {
                W[0] = 0x80000000;
            }
            W[15] = ulen * 8;
        }

        for (int t = 0; t < 64; ++t) {
            if (t >= 16) {
                W[t] = gamma1(W[t - 2]) + W[t - 7] + gamma0(W[t - 15]) + W[t - 16];
            }
            uint T1 = H + sigma1(E) + ch(E, F, G) + K[t] + W[t];
            uint T2 = sigma0(A) + maj(A, B, C);
            H = G; G = F; F = E; E = D + T1; D = C; C = B; 
            B = A; A = T1 + T2;
        }

        hash[0] += A;
        hash[1] += B;
        hash[2] += C;
        hash[3] += D;
        hash[4] += E;
        hash[5] += F;
        hash[6] += G;
        hash[7] += H;
    }

    char resulthash[64] = { 0 };
    tohex(hash[0], &resulthash[0]);
    tohex(hash[1], &resulthash[8]);
    tohex(hash[2], &resulthash[16]);
    tohex(hash[3], &resulthash[24]);
    tohex(hash[4], &resulthash[32]);
    tohex(hash[5], &resulthash[40]);
    tohex(hash[6], &resulthash[48]);
    tohex(hash[7], &resulthash[56]);
    for (int i = 0; i < targetsize; i++) {
        if (resulthash[i] != target[i]) {
            return ;
        }
    }
    if (atomic_inc(&output->count)) {
        return ;
    }
    output->nonce = gid;
    output->hash[0] = hash[0];
    output->hash[1] = hash[1];
    output->hash[2] = hash[2];
    output->hash[3] = hash[3];
    output->hash[4] = hash[4];
    output->hash[5] = hash[5];
    output->hash[6] = hash[6];
    output->hash[7] = hash[7];
}
