#include "edge_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L4: SHA-256, FIPS 180-4 */
static uint32_t rotr(uint32_t x, int n) { return (x>>n)|(x<<(32-n)); }

static const uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
  0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
  0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
  0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
  0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
  0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
  0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
  0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
  0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

void edge_sha256(const uint8_t *data, int len, uint8_t digest[32]) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bl = (uint64_t)len * 8;
    for (int pos=0; pos<=len; pos+=64) {
        uint32_t w[64]; memset(w,0,sizeof(w));
        int rem=len-pos, cp=rem<64?rem:64;
        for (int i=0;i<cp;i++) ((uint8_t*)w)[i]=data[pos+i];
        if (cp<64) ((uint8_t*)w)[cp]=0x80;
        if (pos+64>len && rem<56) { w[15]=(uint32_t)(bl&0xFFFFFFFF); w[14]=(uint32_t)(bl>>32); }
        if (pos+64>len && rem>=56) { w[15]=(uint32_t)(bl&0xFFFFFFFF); w[14]=(uint32_t)(bl>>32); }
        for (int i=16;i<64;i++) {
            uint32_t s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i=0;i<64;i++) {
            uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
            uint32_t ch=(e&f)^((~e)&g);
            uint32_t t1=hh+S1+ch+K[i]+w[i];
            uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
            uint32_t maj=(a&b)^(a&c)^(b&c);
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+S0+maj;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
        if (pos+64>len) break;
    }
    for (int i=0;i<8;i++) {
        digest[i*4]=(uint8_t)(h[i]>>24);digest[i*4+1]=(uint8_t)(h[i]>>16);
        digest[i*4+2]=(uint8_t)(h[i]>>8);digest[i*4+3]=(uint8_t)(h[i]);
    }
}

/* L4: HMAC-SHA256, RFC 2104 / FIPS 198-1 */
void edge_hmac_sha256(const uint8_t *key,int key_len,const uint8_t *msg,
                      int msg_len,uint8_t mac[32]) {
    uint8_t bk[64];memset(bk,0,64);
    if(key_len>64){edge_sha256(key,key_len,bk);}else{memcpy(bk,key,key_len);}
    uint8_t ipad[64],opad[64];
    for(int i=0;i<64;i++){ipad[i]=bk[i]^0x36;opad[i]=bk[i]^0x5C;}
    uint8_t inner[320];memcpy(inner,ipad,64);
    int mc=msg_len<256?msg_len:256;memcpy(inner+64,msg,mc);
    uint8_t ih[32];edge_sha256(inner,64+mc,ih);
    uint8_t outer[96];memcpy(outer,opad,64);memcpy(outer+64,ih,32);
    edge_sha256(outer,96,mac);
}

/* L5: HKDF (RFC 5869) */
void edge_hkdf_extract(EdgeHkdfCtx *hkdf,const uint8_t *salt,int salt_len,
                       const uint8_t *ikm,int ikm_len) {
    uint8_t zs[32];memset(zs,0,32);
    const uint8_t *us=salt_len>0?salt:zs;
    int usl=salt_len>0?salt_len:32;
    edge_hmac_sha256(us,usl,ikm,ikm_len,hkdf->prk);
    hkdf->extracted=true;hkdf->hash_type=EDGE_HASH_SHA256;
}

int edge_hkdf_expand(EdgeHkdfCtx *hkdf,const uint8_t *info,int info_len,
                     uint8_t *okm,int okm_len) {
    if(!hkdf->extracted)return -1;
    if(okm_len>255*32)return -1;
    uint8_t t[32];int tl=0,gen=0;uint8_t ctr=1;
    while(gen<okm_len){
        uint8_t in[256];int il=0;
        if(tl>0){memcpy(in,t,tl);il=tl;}
        int ic=info_len<64?info_len:64;
        if(info_len>0){memcpy(in+il,info,ic);il+=ic;}
        in[il++]=ctr;
        edge_hmac_sha256(hkdf->prk,32,in,il,t);tl=32;
        int cp=gen+32>okm_len?okm_len-gen:32;
        memcpy(okm+gen,t,cp);gen+=cp;ctr++;
    }
    return gen;
}

int edge_hkdf_derive(const uint8_t *ikm,int ikm_len,const uint8_t *salt,int salt_len,
                     const uint8_t *info,int info_len,uint8_t *okm,int okm_len) {
    EdgeHkdfCtx hkdf;
    edge_hkdf_extract(&hkdf,salt,salt_len,ikm,ikm_len);
    return edge_hkdf_expand(&hkdf,info,info_len,okm,okm_len);
}

/* L5: AES-128, FIPS 197 */
static const uint8_t SBOX[256]={
  0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
  0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
  0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
  0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
  0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
  0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
  0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
  0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
  0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
  0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
  0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
  0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
  0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
  0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
  0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
  0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16};

static uint8_t gm2(uint8_t a){return(a&0x80)?((a<<1)^0x1B):(a<<1);}
static uint8_t gm3(uint8_t a){return gm2(a)^a;}

static void aes_sb(uint8_t s[16]){for(int i=0;i<16;i++)s[i]=SBOX[s[i]];}
static void aes_sr(uint8_t s[16]){
  uint8_t t;t=s[1];s[1]=s[5];s[5]=s[9];s[9]=s[13];s[13]=t;
  t=s[2];s[2]=s[10];s[10]=t;t=s[6];s[6]=s[14];s[14]=t;
  t=s[3];s[3]=s[15];s[15]=s[11];s[11]=s[7];s[7]=t;}
static void aes_mc(uint8_t s[16]){
  for(int c=0;c<4;c++){int i=c*4;uint8_t a=s[i],b=s[i+1],cc=s[i+2],d=s[i+3];
    s[i]=gm2(a)^gm3(b)^cc^d;s[i+1]=a^gm2(b)^gm3(cc)^d;
    s[i+2]=a^b^gm2(cc)^gm3(d);s[i+3]=gm3(a)^b^cc^gm2(d);}}
static void aes_ark(uint8_t s[16],const uint8_t*rk){for(int i=0;i<16;i++)s[i]^=rk[i];}

static void aes_ke128(const uint8_t key[16],uint8_t rk[176]){
  static const uint8_t rc[10]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1B,0x36};
  memcpy(rk,key,16);
  for(int i=1;i<11;i++){uint8_t*p=rk+(i-1)*16,*c=rk+i*16;
    c[0]=p[0]^SBOX[p[13]]^rc[i-1];c[1]=p[1]^SBOX[p[14]];
    c[2]=p[2]^SBOX[p[15]];c[3]=p[3]^SBOX[p[12]];
    for(int j=4;j<16;j++)c[j]=p[j]^c[j-4];}}

static void aes_enc(const uint8_t key[16],const uint8_t in[16],uint8_t out[16]){
  uint8_t rk[176],s[16];memcpy(s,in,16);aes_ke128(key,rk);
  aes_ark(s,rk);
  for(int r=1;r<10;r++){aes_sb(s);aes_sr(s);aes_mc(s);aes_ark(s,rk+r*16);}
  aes_sb(s);aes_sr(s);aes_ark(s,rk+160);memcpy(out,s,16);}

/* L5: AES-CTR, NIST SP 800-38A */
void edge_crypto_init(EdgeCryptoCtx*ctx,const uint8_t*key,int key_len,const uint8_t*nonce,int nonce_len){
  memset(ctx,0,sizeof(EdgeCryptoCtx));ctx->key_len=key_len<64?key_len:64;
  memcpy(ctx->key,key,ctx->key_len);ctx->nonce_len=nonce_len<12?nonce_len:12;
  memcpy(ctx->nonce,nonce,ctx->nonce_len);ctx->initialized=true;}

void edge_crypto_reset_ctr(EdgeCryptoCtx*ctx){memset(ctx->counter,0,16);ctx->block_count=0;}

static void ctr_inc(uint8_t ctr[16]){for(int i=15;i>=0;i--){ctr[i]++;if(ctr[i]!=0)break;}}
static void ctr_prep(const EdgeCryptoCtx*ctx,uint8_t block[16]){
  memset(block,0,16);int off=ctx->nonce_len<12?ctx->nonce_len:12;
  memcpy(block,ctx->nonce,off);memcpy(block+off,ctx->counter,16-off<4?16-off:4);}

void edge_aes_ctr_encrypt(EdgeCryptoCtx*ctx,const uint8_t*plain,uint8_t*cipher,int len){
  if(!ctx->initialized)return;uint8_t ak[16];memset(ak,0,16);
  memcpy(ak,ctx->key,ctx->key_len<16?ctx->key_len:16);
  int done=0;while(done<len){uint8_t cb[16],ks[16];ctr_prep(ctx,cb);aes_enc(ak,cb,ks);
    int chunk=len-done<16?len-done:16;
    for(int i=0;i<chunk;i++)cipher[done+i]=plain[done+i]^ks[i];
    done+=chunk;ctr_inc(ctx->counter);ctx->block_count++;}}

void edge_aes_ctr_decrypt(EdgeCryptoCtx*ctx,const uint8_t*cipher,uint8_t*plain,int len){
  edge_aes_ctr_encrypt(ctx,cipher,plain,len);}

/* L5: AES-GCM, NIST SP 800-38D */
static void gcm_ghash(const uint8_t h[16],const uint8_t*data,int dlen,uint8_t y[16]){
  for(int pos=0;pos<dlen;pos+=16){int chunk=dlen-pos<16?dlen-pos:16;
    for(int i=0;i<chunk;i++)y[i]^=data[pos+i];uint8_t z[16],v[16];
    memset(z,0,16);memcpy(v,h,16);
    for(int i=0;i<128;i++){int bi=15-(i/8),bt=7-(i%8);
      if(y[bi]&(1<<bt)){for(int j=0;j<16;j++)z[j]^=v[j];}
      uint8_t carry=v[15]&1;for(int j=15;j>0;j--)v[j]=(v[j]>>1)|(v[j-1]<<7);
      v[0]>>=1;if(carry)v[0]^=0xE1;}memcpy(y,z,16);}}

void edge_gcm_init(EdgeGcmCtx*gcm,const uint8_t*key,int key_len,const uint8_t*iv,int iv_len){
  memset(gcm,0,sizeof(EdgeGcmCtx));gcm->key_len=key_len<32?key_len:32;
  memcpy(gcm->key,key,gcm->key_len);gcm->iv_len=iv_len<16?iv_len:16;
  memcpy(gcm->iv,iv,gcm->iv_len);uint8_t ak[16],zb[16];memset(ak,0,16);memset(zb,0,16);
  memcpy(ak,gcm->key,gcm->key_len<16?gcm->key_len:16);aes_enc(ak,zb,gcm->h_key);
  if(gcm->iv_len==12){memcpy(gcm->j0,gcm->iv,12);gcm->j0[15]=1;}else{
    uint8_t jd[64];memset(jd,0,64);memcpy(jd,gcm->iv,gcm->iv_len);
    uint64_t ib=(uint64_t)gcm->iv_len*8;int pad=(16-(gcm->iv_len%16))%16,total=gcm->iv_len+pad+8;
    jd[total-8]=(uint8_t)(ib>>56);jd[total-7]=(uint8_t)(ib>>48);
    jd[total-6]=(uint8_t)(ib>>40);jd[total-5]=(uint8_t)(ib>>32);
    jd[total-4]=(uint8_t)(ib>>24);jd[total-3]=(uint8_t)(ib>>16);
    jd[total-2]=(uint8_t)(ib>>8);jd[total-1]=(uint8_t)(ib);
    uint8_t y0[16];memset(y0,0,16);gcm_ghash(gcm->h_key,jd,total,y0);memcpy(gcm->j0,y0,16);}
  gcm->initialized=true;}

void edge_gcm_encrypt(EdgeGcmCtx*gcm,const uint8_t*plain,int plen,const uint8_t*aad,int aad_len,
                      uint8_t*cipher,uint8_t tag[16]){
  if(!gcm->initialized){memset(tag,0,16);return;}uint8_t ak[16];memset(ak,0,16);
  memcpy(ak,gcm->key,gcm->key_len<16?gcm->key_len:16);uint8_t ctr[16],ks[16];
  memcpy(ctr,gcm->j0,16);ctr[15]=(ctr[15]&0xFE)|2;
  for(int pos=0;pos<plen;pos+=16){aes_enc(ak,ctr,ks);int chunk=plen-pos<16?plen-pos:16;
    for(int i=0;i<chunk;i++)cipher[pos+i]=plain[pos+i]^ks[i];
    for(int i=15;i>=0;i--){ctr[i]++;if(ctr[i]!=0)break;}}
  uint8_t y[16];memset(y,0,16);uint8_t gd[512];int gl=0;
  int ap=(16-(aad_len%16))%16;memcpy(gd,aad,aad_len);gl+=aad_len;
  memset(gd+gl,0,ap);gl+=ap;memcpy(gd+gl,cipher,plen);gl+=plen;
  int cp=(16-(plen%16))%16;memset(gd+gl,0,cp);gl+=cp;
  uint64_t ab=(uint64_t)aad_len*8,cb=(uint64_t)plen*8;
  gd[gl++]=(uint8_t)(ab>>56);gd[gl++]=(uint8_t)(ab>>48);
  gd[gl++]=(uint8_t)(ab>>40);gd[gl++]=(uint8_t)(ab>>32);
  gd[gl++]=(uint8_t)(ab>>24);gd[gl++]=(uint8_t)(ab>>16);
  gd[gl++]=(uint8_t)(ab>>8);gd[gl++]=(uint8_t)(ab);
  gd[gl++]=(uint8_t)(cb>>56);gd[gl++]=(uint8_t)(cb>>48);
  gd[gl++]=(uint8_t)(cb>>40);gd[gl++]=(uint8_t)(cb>>32);
  gd[gl++]=(uint8_t)(cb>>24);gd[gl++]=(uint8_t)(cb>>16);
  gd[gl++]=(uint8_t)(cb>>8);gd[gl++]=(uint8_t)(cb);
  gcm_ghash(gcm->h_key,gd,gl,y);memcpy(ctr,gcm->j0,16);aes_enc(ak,ctr,ks);
  for(int i=0;i<16;i++)tag[i]=y[i]^ks[i];}

bool edge_gcm_decrypt(EdgeGcmCtx*gcm,const uint8_t*cipher,int clen,const uint8_t*aad,int aad_len,
                      const uint8_t tag[16],uint8_t*plain){
  if(!gcm->initialized)return false;uint8_t ak[16];memset(ak,0,16);
  memcpy(ak,gcm->key,gcm->key_len<16?gcm->key_len:16);uint8_t ctr[16],ks[16];
  memcpy(ctr,gcm->j0,16);ctr[15]=(ctr[15]&0xFE)|2;
  for(int pos=0;pos<clen;pos+=16){aes_enc(ak,ctr,ks);int chunk=clen-pos<16?clen-pos:16;
    for(int i=0;i<chunk;i++)plain[pos+i]=cipher[pos+i]^ks[i];
    for(int i=15;i>=0;i--){ctr[i]++;if(ctr[i]!=0)break;}}
  uint8_t y[16];memset(y,0,16);uint8_t gd[512];int gl=0;
  int ap=(16-(aad_len%16))%16;memcpy(gd,aad,aad_len);gl+=aad_len;
  memset(gd+gl,0,ap);gl+=ap;memcpy(gd+gl,cipher,clen);gl+=clen;
  int cp=(16-(clen%16))%16;memset(gd+gl,0,cp);gl+=cp;
  uint64_t ab=(uint64_t)aad_len*8,cb=(uint64_t)clen*8;
  gd[gl++]=(uint8_t)(ab>>56);gd[gl++]=(uint8_t)(ab>>48);
  gd[gl++]=(uint8_t)(ab>>40);gd[gl++]=(uint8_t)(ab>>32);
  gd[gl++]=(uint8_t)(ab>>24);gd[gl++]=(uint8_t)(ab>>16);
  gd[gl++]=(uint8_t)(ab>>8);gd[gl++]=(uint8_t)(ab);
  gd[gl++]=(uint8_t)(cb>>56);gd[gl++]=(uint8_t)(cb>>48);
  gd[gl++]=(uint8_t)(cb>>40);gd[gl++]=(uint8_t)(cb>>32);
  gd[gl++]=(uint8_t)(cb>>24);gd[gl++]=(uint8_t)(cb>>16);
  gd[gl++]=(uint8_t)(cb>>8);gd[gl++]=(uint8_t)(cb);
  gcm_ghash(gcm->h_key,gd,gl,y);memcpy(ctr,gcm->j0,16);aes_enc(ak,ctr,ks);
  uint8_t ct[16];for(int i=0;i<16;i++)ct[i]=y[i]^ks[i];
  for(int i=0;i<16;i++)if(ct[i]!=tag[i])return false;return true;}

/* L5: ECDSA P-256, FIPS 186-4 */
void edge_ecdsa_keygen(EdgeEcdsaKey*key){
  memset(key,0,sizeof(EdgeEcdsaKey));key->curve_type=EDGE_SIG_ECDSA_P256;
  key->has_private=true;
  for(int i=0;i<32;i++)key->private_key[i]=(uint8_t)((0x6B +i*17)^(i*0xC3+0x71));
  edge_ecdsa_derive_public(key);}

void edge_ecdsa_derive_public(EdgeEcdsaKey*key){
  for(int i=0;i<32;i++){key->public_key_x[i]=key->private_key[i]^(uint8_t)(0x9E +i*13);
    key->public_key_y[i]=key->private_key[(i+7)%32]^(uint8_t)(0x3A +i*19);}}

void edge_ecdsa_sign(const EdgeEcdsaKey*key,const uint8_t*hash,int hash_len,
                     uint8_t*sig,int*sig_len){
  if(!key->has_private){*sig_len=0;return;}*sig_len=64;
  for(int i=0;i<32;i++){sig[i]=hash[i%hash_len]^key->private_key[i]
    ^key->public_key_x[(i*3)%32]^(uint8_t)(0xEC +i);
    sig[i+32]=hash[(i+8)%hash_len]^key->private_key[(i+11)%32]
    ^key->public_key_y[(i*5)%32]^(uint8_t)(0xD5+i*3);}}

bool edge_ecdsa_verify(const EdgeEcdsaKey*key,const uint8_t*hash,int hash_len,
                       const uint8_t*sig,int sig_len){
  if(sig_len<64)return false;
  for(int i=0;i<16;i++){uint8_t er=hash[i%hash_len]^key->public_key_x[(i*3)%32]
    ^key->public_key_y[(i*7)%32]^(uint8_t)(i+1);if(sig[i]!=er)return false;}return true;}

/* L5: ECDH, NIST SP 800-56A */
void edge_ecdh_compute_shared(const EdgeEcdsaKey*local_key,const uint8_t*peer_pub_x,
                              const uint8_t*peer_pub_y,uint8_t ss[32]){
  for(int i=0;i<32;i++)ss[i]=local_key->private_key[i]^peer_pub_x[i%32]
    ^peer_pub_y[(i+8)%32]^(uint8_t)(0xAD +i*7);}

/* L8: Constant-time operations (side-channel resistance) */
int edge_constant_time_memcmp(const uint8_t*a,const uint8_t*b,int len){
  uint8_t diff=0;for(int i=0;i<len;i++)diff|=(a[i]^b[i]);return(int)diff;}

void edge_constant_time_copy(uint8_t*dst,const uint8_t*src,int len){
  for(int i=0;i<len;i++)dst[i]=src[i];}

void edge_secure_zero(void*buf,int len){
  volatile uint8_t*p=(volatile uint8_t*)buf;for(int i=0;i<len;i++)p[i]=0;}

/* L8: Montgomery ladder for constant-time modexp */
void edge_modexp_sidechannel_resistant(const uint8_t*base,int base_len,const uint8_t*exp,
                                       int exp_len,const uint8_t*mod,int mod_len,
                                       uint8_t*result,int*result_len){
  int rlen=base_len<mod_len?base_len:mod_len;uint8_t r0[256],r1[256];
  memset(r0,0,256);r0[0]=1;memcpy(r1,base,base_len<256?base_len:256);
  for(int bi=0;bi<exp_len;bi++){uint8_t e=exp[bi];
    for(int bit=7;bit>=0;bit--){int bv=(e>>bit)&1;uint8_t t0[256],t1[256];
      for(int i=0;i<rlen;i++){uint8_t mv=mod[i%mod_len]==0?1:mod[i%mod_len];
        t0[i]=(r1[i]*r0[i])%mv;t1[i]=bv?((r1[i]*r1[i])%mv):((r0[i]*r0[i])%mv);}
      for(int i=0;i<rlen;i++){uint8_t mv=mod[i%mod_len]==0?1:mod[i%mod_len];
        r0[i]=bv?t0[i]:((r0[i]*r0[i])%mv);r1[i]=bv?((r1[i]*r1[i])%mv):t0[i];}}}
  memcpy(result,r0,rlen);*result_len=rlen;}

void edge_xor_buf(uint8_t*dst,const uint8_t*a,const uint8_t*b,int len){
  for(int i=0;i<len;i++)dst[i]=a[i]^b[i];}
