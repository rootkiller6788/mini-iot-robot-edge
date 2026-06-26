/* test_core.c - mini-edge-ai unit tests */

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

static int tests_run=0,tests_passed=0;
#define TEST(n) do{tests_run++;printf("  TEST %-45s ",n);}while(0)
#define PASS() do{tests_passed++;printf("PASS\n");}while(0)
#define FAIL(m) do{printf("FAIL: %s\n",m);return 1;}while(0)
#define CHECK(c,m) if(!(c))FAIL(m)

