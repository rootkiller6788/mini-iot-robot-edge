/*
 * test_core.c - Core Unit Tests for mini-edge-ai
 * Tests all five sub-modules with correct API calls.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "edge_inference.h"
#include "edge_pipeline.h"
#include "edge_tpu_npu.h"
#include "inference_opt.h"
#include "model_conversion.h"

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %-45s ", name); } while(0)
#define PASS()     do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); return 1; } while(0)
#define CHECK(cond, msg) if (!(cond)) FAIL(msg)

/* ===== edge_inference core ===== */
static int t1(void) {
    TEST("infer_create/destroy");
    InferConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.input_shape.width=224;cfg.input_shape.height=224;
    cfg.input_shape.channels=3;cfg.num_classes=10;
    InferCtx* ctx=infer_create(&cfg);
    CHECK(ctx!=NULL,"infer_create NULL");
    infer_destroy(ctx);
    PASS(); return 0;
}
static int t2(void) {
    TEST("infer_load_model (missing file)");
    InferConfig cfg; memset(&cfg,0,sizeof(cfg));
    strncpy(cfg.model_path,"nosuch.tflite",MAX_MODEL_PATH-1);
    cfg.input_shape.width=224;cfg.input_shape.height=224;
    cfg.input_shape.channels=3;cfg.num_classes=10;
    InferCtx* ctx=infer_create(&cfg);
    int rc=infer_load_model(ctx);
    CHECK(rc!=0,"should fail on missing file");
    infer_destroy(ctx);
    PASS(); return 0;
}
static int t3(void) {
    TEST("infer_run (needs loaded model, returns -1)");
    InferConfig cfg; memset(&cfg,0,sizeof(cfg));
    cfg.input_shape.width=16;cfg.input_shape.height=16;
    cfg.input_shape.channels=1;cfg.input_shape.format=INPUT_FMT_FLOAT32;
    cfg.preproc=PREPROC_NONE;cfg.postproc=POSTPROC_SOFTMAX;
    cfg.num_classes=5;
    InferCtx* ctx=infer_create(&cfg);
    float in[256]; memset(in,0,sizeof(in));
    /* Without loaded model, infer_run returns -1 */
    int rc=infer_run(ctx,in,sizeof(in));
    CHECK(rc==-1,"infer_run should fail when model not loaded");
    infer_destroy(ctx);
    PASS(); return 0;
}
static int t4(void) {
    TEST("resize+normalize+softmax chain");
    uint8_t src[768],dst[192];
    memset(src,128,sizeof(src));
    int rc=preproc_resize_bilinear(src,16,16,3,dst,8,8);
    CHECK(rc==0,"resize failed");
    float norm[192],mean[3]={127.5f,127.5f,127.5f},stdv[3]={127.5f,127.5f,127.5f};
    rc=preproc_normalize_f32(dst,8,8,3,norm,mean,stdv);
    CHECK(rc==0,"normalize failed");
    float probs[10],logits[10]={1,2,3,4,5,6,7,8,9,10};
    rc=postproc_softmax_f32(logits,probs,10);
    CHECK(rc==0,"softmax failed");
    float sum=0;for(int i=0;i<10;i++)sum+=probs[i];
    CHECK(fabsf(sum-1)<0.01f,"softmax sum!=1");
    int arg=postproc_argmax_f32(probs,10);
    CHECK(arg>=0&&arg<10,"argmax");
    PASS(); return 0;
}

/* ===== L5: Kalman, Welford, TopK, Mahalanobis, Chi2, Ensemble ===== */
static int t5(void) {
    TEST("Kalman filter 1D");
    KalmanFilter1D kf;kalman1d_init(&kf,0,1,0.1f,0.5f);
    float p=kalman1d_predict(&kf,0);
    CHECK(p==0,"predict from 0");
    float u=kalman1d_update(&kf,1);
    CHECK(u>0&&u<1,"update toward 1");
    PASS(); return 0;
}
static int t6(void) {
    TEST("Welford online stats");
    OnlineStats s;online_stats_init(&s);
    for(int i=1;i<=5;i++)online_stats_push(&s,(float)i);
    CHECK(fabsf(online_stats_mean(&s)-3)<0.01f,"mean!=3");
    CHECK(online_stats_variance(&s)>0,"variance>0");
    PASS(); return 0;
}
static int t7(void) {
    TEST("TopK min-heap");
    TopKHeap tk;topk_init(&tk,5);
    for(int i=0;i<20;i++)topk_push(&tk,(float)i,i);
    float sc[10];int idx[10];
    int n=topk_get_sorted(&tk,sc,idx);
    CHECK(n==5,"k!=5");CHECK(sc[0]>=sc[4],"order");
    PASS(); return 0;
}
static int t8(void) {
    TEST("Mahalanobis anomaly detection");
    MahalanobisCtx* mc=mahalanobis_create(2);
    CHECK(mc!=NULL,"create");
    float data[20]={1,1,1.1f,1.1f,1.2f,1.2f,0.9f,0.9f,1,1,0.8f,0.8f,1.3f,1.3f,0.7f,0.7f,1.4f,1.4f,0.6f,0.6f};
    mahalanobis_fit(mc,data,10);
    float norm[2]={1,1},outlier[2]={10,10};
    float dn=mahalanobis_distance(mc,norm),dx=mahalanobis_distance(mc,outlier);
    CHECK(dx>dn,"outlier dist>normal");
    mahalanobis_destroy(mc);
    PASS(); return 0;
}
static int t9(void) {
    TEST("Chi-square test");
    int obs[4]={50,30,15,5};float exp[4]={0.5f,0.25f,0.15f,0.1f};
    float chi=chi_square_test(obs,exp,4,100);
    CHECK(chi>=0,"chi>=0");
    PASS(); return 0;
}
static int t10(void) {
    TEST("Ensemble prediction");
    float m1[3]={1,2,3},m2[3]={3,2,1};
    float* models[2]={m1,m2};float probs[3],w[2]={0.5f,0.5f};
    int rc=ensemble_predict_classification(models,2,3,w,probs);
    CHECK(rc==0,"ensemble");float s=0;
    for(int i=0;i<3;i++)s+=probs[i];
    CHECK(fabsf(s-1)<0.01f,"sum!=1");
    PASS(); return 0;
}

/* ===== edge_pipeline ===== */
static int t11(void) {
    TEST("pipeline lifecycle");
    PipelineCtx* p=pipeline_create();CHECK(p!=NULL,"create");
    pipeline_add_stage(p,STAGE_PREPROCESS,NULL,NULL);
    CHECK(pipeline_run(p)==0,"run");
    CHECK(pipeline_get_state(p)==PIPELINE_IDLE,"idle");
    pipeline_destroy(p);
    PASS(); return 0;
}
static int t12(void) {
    TEST("federated learning");
    PipelineCtx* p=pipeline_create();
    fed_learn_init(p,100);
    float samples[10];for(int i=0;i<10;i++)samples[i]=(float)i;
    fed_learn_train_local(p,samples,10,2);
    float w[100];CHECK(fed_learn_get_weights(p,w,100)==100,"weights");
    float c1[100],c2[100],gw[100];
    for(int i=0;i<100;i++){c1[i]=1;c2[i]=2;}
    const float* cw[2]={c1,c2};
    fed_learn_aggregate(NULL,2,cw,100,gw);
    CHECK(fabsf(gw[0]-1.5f)<0.01f,"aggregate avg");
    pipeline_destroy(p);
    PASS(); return 0;
}
static int t13(void) {
    TEST("priority queue");
    PriorityQueue pq;pq_init(&pq);
    pq_push(&pq,3,100,NULL,NULL);pq_push(&pq,1,50,NULL,NULL);pq_push(&pq,2,200,NULL,NULL);
    PriorityTask out;pq_pop(&pq,&out);
    CHECK(out.priority==1,"min-heap order");
    PASS(); return 0;
}
static int t14(void) {
    TEST("ring buffer write/read");
    RingBuffer rb;ringbuf_init(&rb);
    uint8_t src[10]={0,1,2,3,4,5,6,7,8,9},dst[10];
    CHECK(ringbuf_write(&rb,src,10)==10,"write");
    CHECK(ringbuf_read(&rb,dst,10)==10,"read");
    CHECK(memcmp(src,dst,10)==0,"data match");
    PASS(); return 0;
}
static int t15(void) {
    TEST("SHA-256 hash");
    SHA256Ctx ctx;sha256_init(&ctx);
    sha256_update(&ctx,(const uint8_t*)"test",4);
    sha256_final(&ctx);
    CHECK(ctx.finalized==1,"finalized");
    PASS(); return 0;
}
static int t16(void) {
    TEST("complementary filter");
    CompFilter cf;comp_filter_init(&cf,0.98f,0.01f);
    float a=comp_filter_update(&cf,0.5f,0.3f);
    CHECK(a>=0,"angle>=0");
    PASS(); return 0;
}

/* ===== edge_tpu_npu ===== */
static int t17(void) {
    TEST("accel create/destroy");
    AccelConfig acfg;memset(&acfg,0,sizeof(acfg));
    acfg.type=ACCEL_CORAL_EDGE_TPU;acfg.bus=ACCEL_BUS_USB;
    AccelCtx* a=accel_create(&acfg);CHECK(a!=NULL,"create");
    accel_destroy(a);
    PASS(); return 0;
}
static int t18(void) {
    TEST("accel_probe");
    AccelInfo infos[4];int n=accel_probe(infos,4);
    CHECK(n>0&&n<=4,"probe");
    PASS(); return 0;
}
static int t19(void) {
    TEST("roofline model");
    RooflineModel rm;roofline_init(&rm,100,20);
    float p=roofline_predict_tflops(&rm,500,100);
    CHECK(p>0&&p<=100,"prediction");
    const char* b=roofline_bound_type(&rm,10,100);
    CHECK(strcmp(b,"memory-bound")==0,"low AI mem-bound");
    PASS(); return 0;
}
static int t20(void) {
    TEST("accel mempool alloc/reset");
    AccelMemPool* mp=accel_mempool_create(1024*1024);
    CHECK(mp!=NULL,"create");
    void* a=accel_mempool_alloc(mp,4096);CHECK(a!=NULL,"alloc a");
    void* b=accel_mempool_alloc(mp,8192);CHECK(b!=NULL&&a!=b,"alloc b");
    accel_mempool_reset(mp);
    CHECK(accel_mempool_available(mp)==(size_t)(1024*1024),"full reset");
    accel_mempool_destroy(mp);
    PASS(); return 0;
}
static int t21(void) {
    TEST("tensor NHWC<->NCHW");
    float nhwc[48],nchw[48],nhwc2[48];
    for(int i=0;i<48;i++)nhwc[i]=(float)(i+1);
    tensor_nhwc_to_nchw(nhwc,2,3,4,2,nchw);
    tensor_nchw_to_nhwc(nchw,2,2,3,4,nhwc2);
    for(int i=0;i<48;i++)CHECK(fabsf(nhwc[i]-nhwc2[i])<0.01f,"roundtrip");
    PASS(); return 0;
}
static int t22(void) {
    TEST("power/energy estimation");
    PowerModel pm;power_model_init(&pm,4.6f,20,1300,50);
    float e=power_estimate_energy_uj(&pm,1000000ULL,100000ULL,1000ULL,100);
    CHECK(e>0,"energy>0");
    PASS(); return 0;
}

/* ===== inference_opt ===== */
static int t23(void) {
    TEST("TensorArena alloc/reset");
    TensorArena* a=arena_create(1024*1024);CHECK(a!=NULL,"create");
    void* p1=arena_alloc(a,4096);CHECK(p1!=NULL,"alloc1");
    void* p2=arena_alloc(a,4096);CHECK(p2!=NULL&&p1!=p2,"alloc2");
    arena_reset(a);CHECK(arena_available(a)==(size_t)(1024*1024),"reset");
    arena_destroy(a);
    PASS(); return 0;
}
static int t24(void) {
    TEST("INT8 quant/dequant");
    float src[64];int8_t q[64];float dst[64];
    for(int i=0;i<64;i++)src[i]=((float)i-32)*0.1f;
    float scale;int32_t zp;quant_calc_scale_zp(src,64,&scale,&zp);
    CHECK(scale>0,"scale");quantize_int8_asym(src,64,scale,zp,q);
    dequantize_int8_asym(q,64,scale,zp,dst);
    PASS(); return 0;
}
static int t25(void) {
    TEST("Winograd F23 input transform");
    float in[16],out[36];
    for(int i=0;i<16;i++)in[i]=(float)i;
    CHECK(winograd_f23_transform_input(in,1,4,4,out)==0,"winograd");
    PASS(); return 0;
}
static int t26(void) {
    TEST("GEMM micro 6x16");
    float A[48],B[128],C[96];
    for(int i=0;i<48;i++)A[i]=(float)(i%8)*0.1f;
    for(int i=0;i<128;i++)B[i]=(float)(i%16)*0.05f;
    memset(C,0,sizeof(C));
    CHECK(gemm_micro_6x16(6,16,8,A,8,B,16,C,16)==0,"gemm");
    PASS(); return 0;
}
static int t27(void) {
    TEST("cache-blocked GEMM");
    int M=64,N=64,K=32;
    float* A=calloc(M*K,sizeof(float));float* B=calloc(K*N,sizeof(float));
    float* C=calloc(M*N,sizeof(float));
    if(!A||!B||!C){free(A);free(B);free(C);FAIL("OOM");}
    for(int i=0;i<M*K;i++)A[i]=(float)(i%13)*0.1f;
    for(int i=0;i<K*N;i++)B[i]=(float)(i%11)*0.1f;
    int rc=gemm_blocked(M,N,K,A,K,B,N,C,N,32,32,16);
    CHECK(rc==0,"blocked");free(A);free(B);free(C);
    PASS(); return 0;
}
static int t28(void) {
    TEST("depthwise separable conv");
    float in[3072],kw[27],dw[2700],pw[3600];
    float pk[12];
    for(int i=0;i<3072;i++)in[i]=0.1f;
    for(int i=0;i<27;i++)kw[i]=0.1f;
    for(int i=0;i<12;i++)pk[i]=0.05f;
    CHECK(depthwise_conv2d_3x3(in,32,32,3,kw,NULL,1,dw)==0,"dw");
    CHECK(pointwise_conv2d_1x1(dw,30,30,3,4,pk,NULL,pw)==0,"pw");
    PASS(); return 0;
}
static int t29(void) {
    TEST("Huffman coding build/encode");
    uint32_t freqs[256]={0};
    freqs[0]=10; freqs[1]=5; freqs[2]=2; freqs[3]=1;
    HuffmanEncoder enc;
    int rc=huffman_build_tree(freqs,4,&enc);
    CHECK(rc==0,"build tree");
    /* Verify codes were assigned (non-zero lengths for all 4 symbols) */
    for(int i=0;i<4;++i) CHECK(enc.codes[i].code_len>0,"code assigned");
    /* Encode and verify output */
    int syms[]={0,1,0,2,3};
    uint8_t bs[64]; size_t bsB=0;
    rc=huffman_encode(&enc,syms,5,bs,&bsB);
    CHECK(rc==0&&bsB>0,"encode produces output");
    PASS(); return 0;
}
static int t30(void) {
    TEST("sparse-dense CSR matmul");
    float v[4]={1,2,3,4};int ci[4]={0,2,1,3},rp[4]={0,2,3,4};
    float B[8],C[6];
    for(int i=0;i<8;i++)B[i]=1;
    memset(C,0,sizeof(C));
    CHECK(sparse_dense_matmul_csr(v,ci,rp,3,4,B,2,C)==0,"csr");
    PASS(); return 0;
}
static int t31(void) {
    TEST("im2col + col2im");
    float in[784],col[6084],grad[784];
    for(int i=0;i<784;i++)in[i]=(float)(i%256)/256;
    CHECK(im2col(in,28,28,1,3,3,1,0,col)==0,"im2col");
    CHECK(col2im_gradient(col,28,28,1,3,3,1,0,grad)==0,"col2im");
    PASS(); return 0;
}

/* ===== model_conversion ===== */
static int t32(void) {
    TEST("convert lifecycle");
    ConvertConfig cc;memset(&cc,0,sizeof(cc));
    strncpy(cc.input_path,"m.onnx",MAX_CONVERT_PATH-1);
    strncpy(cc.output_path,"m.tflite",MAX_CONVERT_PATH-1);
    cc.src=SRC_FRAMEWORK_PYTORCH;cc.dst=DST_FRAMEWORK_ONNX;cc.quant=QUANT_NONE;
    ConvertCtx* c=convert_create(&cc);CHECK(c!=NULL,"create");
    CHECK(convert_run(c)==0,"run");
    char log[1024];CHECK(convert_get_log(c,log,sizeof(log))>0,"log");
    convert_destroy(c);
    PASS(); return 0;
}
static int t33(void) {
    TEST("BN fold + ConvReLU fuse");
    float w[9],b[3],g[3],bt[3],m[3],v[3];
    for(int i=0;i<9;i++)w[i]=0.5f;
    for(int i=0;i<3;i++){b[i]=0.1f;g[i]=1.5f;bt[i]=0.2f;m[i]=0;v[i]=1;}
    CHECK(graph_opt_fold_batchnorm(w,b,g,bt,m,v,1e-5f,3)==0,"BN fold");
    CHECK(graph_fuse_conv_relu_check(3,0)==1,"fuse");
    PASS(); return 0;
}
static int t34(void) {
    TEST("channel pruning L1");
    ChannelPrunePlan p;channel_prune_init(&p,8);
    float w[216];for(int i=0;i<216;i++)w[i]=(float)((i*7+3)%100)*0.01f;
    channel_prune_compute_l1(w,8,3,3,3,&p);
    int keep[8];CHECK(channel_prune_select(&p,0.25f,keep)==6,"25% prune");
    channel_prune_destroy(&p);
    PASS(); return 0;
}
static int t35(void) {
    TEST("FLOPs/param counting");
    CHECK(count_params_conv2d(3,16,3,3,1)==448ULL,"conv params");
    CHECK(count_flops_conv2d(3,16,3,3,112,112)>0,"flops>0");
    uint64_t layers[3]={100,200,300};
    CHECK(count_total_params(layers,3)==600ULL,"total");
    PASS(); return 0;
}
static int t36(void) {
    TEST("magnitude pruning + sparsity");
    float w[100];for(int i=0;i<100;i++)w[i]=(float)((i%20)-10)*0.1f;
    int n=magnitude_prune_weights(w,100,0.5f);
    CHECK(n>0,"pruned");float s=compute_sparsity(w,100,1e-6f);
    CHECK(s>=0&&s<=1,"sparsity");
    PASS(); return 0;
}
static int t37(void) {
    TEST("SQNR bit-width");
    float d[256];for(int i=0;i<256;i++)d[i]=sinf((float)i*0.1f);
    float s8=compute_sqnr_uniform(d,256,8);
    float s4=compute_sqnr_uniform(d,256,4);
    CHECK(s8>s4,"8-bit SQNR > 4-bit SQNR");
    PASS(); return 0;
}

int main(void) {
    printf("mini-edge-ai  --  Core Unit Tests\n");
    printf("============================================================\n");

    t1(); t2(); t3(); t4(); t5(); t6(); t7(); t8(); t9(); t10();
    t11(); t12(); t13(); t14(); t15(); t16();
    t17(); t18(); t19(); t20(); t21(); t22();
    t23(); t24(); t25(); t26(); t27(); t28(); t29(); t30(); t31();
    t32(); t33(); t34(); t35(); t36(); t37();

    printf("============================================================\n");
    printf("%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}