/* Copyright 2013-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PI/pi_p4info.h"
#include "p4info/p4info_struct.h"
#include "p4info/fields_int.h"
#include "p4info/actions_int.h"
#include "p4info/tables_int.h"

#include "unity/unity_fixture.h"

#include <string.h>
#include <Judy.h>

static pi_p4info_t *p4info;

TEST_GROUP(P4Info);

TEST_SETUP(P4Info) {
  p4info = calloc(1, sizeof(pi_p4info_t));
}

TEST_TEAR_DOWN(P4Info) {
  free(p4info);
}

TEST(P4Info, Fields) {
  const size_t num_fields = 3;
  const pi_p4_id_t f0 = 0, f1 = 1, f2 = 2;
  const size_t bw0 = 11, bw1 = 16, bw2 = 128;
  const char *n0 = "f0", *n1 = "f1", *n2 = "f2";

  pi_p4info_field_init(p4info, num_fields);

  // adding them out of order on purpose
  pi_p4info_field_add(p4info, f1, n1, bw1);
  pi_p4info_field_add(p4info, f0, n0, bw0);
  pi_p4info_field_add(p4info, f2, n2, bw2);

  TEST_ASSERT_EQUAL_UINT(f0, pi_p4info_field_id_from_name(p4info, n0));
  TEST_ASSERT_EQUAL_UINT(f1, pi_p4info_field_id_from_name(p4info, n1));
  TEST_ASSERT_EQUAL_UINT(f2, pi_p4info_field_id_from_name(p4info, n2));

  TEST_ASSERT_EQUAL_STRING(n0, pi_p4info_field_name_from_id(p4info, f0));
  TEST_ASSERT_EQUAL_STRING(n1, pi_p4info_field_name_from_id(p4info, f1));
  TEST_ASSERT_EQUAL_STRING(n2, pi_p4info_field_name_from_id(p4info, f2));

  TEST_ASSERT_EQUAL_UINT(bw0, pi_p4info_field_bitwidth(p4info, f0));
  TEST_ASSERT_EQUAL_UINT(bw1, pi_p4info_field_bitwidth(p4info, f1));
  TEST_ASSERT_EQUAL_UINT(bw2, pi_p4info_field_bitwidth(p4info, f2));

  pi_p4info_field_free(p4info);
}

TEST(P4Info, FieldsByte0Mask) {
  const size_t num_fields = 128;

  pi_p4info_field_init(p4info, num_fields);

  for (size_t i = 0; i < num_fields; i++) {
    char name[16];
    snprintf(name, sizeof(name), "f%zu", i);
    pi_p4info_field_add(p4info, i, name, i + 1);
  }

  TEST_ASSERT_EQUAL_HEX8(0x01, pi_p4info_field_byte0_mask(p4info, 0));
  TEST_ASSERT_EQUAL_HEX8(0x03, pi_p4info_field_byte0_mask(p4info, 1));
  TEST_ASSERT_EQUAL_HEX8(0x07, pi_p4info_field_byte0_mask(p4info, 2));
  TEST_ASSERT_EQUAL_HEX8(0x0f, pi_p4info_field_byte0_mask(p4info, 3));
  TEST_ASSERT_EQUAL_HEX8(0x1f, pi_p4info_field_byte0_mask(p4info, 4));
  TEST_ASSERT_EQUAL_HEX8(0x3f, pi_p4info_field_byte0_mask(p4info, 5));
  TEST_ASSERT_EQUAL_HEX8(0x7f, pi_p4info_field_byte0_mask(p4info, 6));
  TEST_ASSERT_EQUAL_HEX8(0xff, pi_p4info_field_byte0_mask(p4info, 7));
  for (size_t i = 8; i < num_fields; i++) {
    char mask;
    size_t bitwidth = i + 1;
    mask = (bitwidth % 8 == 0) ? 0xff : ((1 << (bitwidth % 8)) - 1);
    TEST_ASSERT_EQUAL_HEX8(mask, pi_p4info_field_byte0_mask(p4info, i));
  }

  pi_p4info_field_free(p4info);
}

TEST(P4Info, FieldsStress) {
  const size_t num_fields = 4096;

  pi_p4info_field_init(p4info, num_fields);

  char name[16];
  for (size_t i = 0; i < num_fields; i++) {
    snprintf(name, sizeof(name), "f%zu", i);
    pi_p4info_field_add(p4info, i, name, 1 + (i % 128));
  }

  for (size_t i = 0; i < num_fields; i++) {
    snprintf(name, sizeof(name), "f%zu", i);
    TEST_ASSERT_EQUAL_UINT(i, pi_p4info_field_id_from_name(p4info, name));
  }

  for (size_t i = 0; i < num_fields; i++) {
    snprintf(name, sizeof(name), "f%zu", i);
    TEST_ASSERT_EQUAL_STRING(name, pi_p4info_field_name_from_id(p4info, i));
  }

  for (size_t i = 0; i < num_fields; i++) {
    TEST_ASSERT_EQUAL_UINT(1 + (i % 128), pi_p4info_field_bitwidth(p4info, i));
  }

  pi_p4info_field_free(p4info);
}

typedef struct {
  pi_p4_id_t id;
  char *name;
  size_t num_params;
} adata_t;

TEST(P4Info, Actions) {
  const size_t num_actions = 2;
  const adata_t adata_0 = {0, "a0", 2};
  const adata_t adata_1 = {1, "a1", 0};

  const char *const param_names[2] = {"p0_0", "p0_1"};
  const size_t param_bws[2] = {18, 3};

  pi_p4info_action_init(p4info, num_actions);

  pi_p4info_action_add(p4info, adata_0.id, adata_0.name, adata_0.num_params);
  pi_p4info_action_add(p4info, adata_1.id, adata_1.name, adata_1.num_params);

  // out of order on purpose
  pi_p4info_action_add_param(p4info, adata_0.id, 1, param_names[1],
                             param_bws[1]);
  pi_p4info_action_add_param(p4info, adata_0.id, 0, param_names[0],
                             param_bws[0]);

  TEST_ASSERT_EQUAL_UINT(adata_0.id,
                         pi_p4info_action_id_from_name(p4info, adata_0.name));
  TEST_ASSERT_EQUAL_UINT(adata_1.id,
                         pi_p4info_action_id_from_name(p4info, adata_1.name));

  TEST_ASSERT_EQUAL_STRING(adata_0.name,
                           pi_p4info_action_name_from_id(p4info, adata_0.id));
  TEST_ASSERT_EQUAL_STRING(adata_1.name,
                           pi_p4info_action_name_from_id(p4info, adata_1.id));

  TEST_ASSERT_EQUAL_UINT(adata_0.num_params,
                         pi_p4info_action_num_params(p4info, adata_0.id));
  TEST_ASSERT_EQUAL_UINT(adata_1.num_params,
                         pi_p4info_action_num_params(p4info, adata_1.id));

  TEST_ASSERT_EQUAL_UINT(
      0,
      pi_p4info_action_param_id_from_name(p4info, adata_0.id, param_names[0]));
  TEST_ASSERT_EQUAL_UINT(
      1,
      pi_p4info_action_param_id_from_name(p4info, adata_0.id, param_names[1]));

  TEST_ASSERT_EQUAL_STRING(
      param_names[0],
      pi_p4info_action_param_name_from_id(p4info, adata_0.id, 0));
  TEST_ASSERT_EQUAL_STRING(
      param_names[1],
      pi_p4info_action_param_name_from_id(p4info, adata_0.id, 1));

  TEST_ASSERT_EQUAL_UINT(
      param_bws[0], pi_p4info_action_param_bitwidth(p4info, adata_0.id, 0));
  TEST_ASSERT_EQUAL_UINT(
      param_bws[1], pi_p4info_action_param_bitwidth(p4info, adata_0.id, 1));

  pi_p4info_action_free(p4info);
}

// unity uses a wrapper for the heap allocator, which does not cover strdup
char *unity_strdup(const char *s) {
  char *new_s = malloc(strlen(s) + 1);
  strcpy(new_s, s);
  return new_s;
}

#undef strdup
#define strdup unity_strdup

TEST(P4Info, ActionsStress) {
  const size_t num_actions = 4096;
  adata_t *adata = calloc(num_actions, sizeof(adata_t));

  char name[16];
  for (size_t i = 0; i < num_actions; i++) {
    adata[i].id = i;
    snprintf(name, sizeof(name), "a%zu", i);
    adata[i].name = strdup(name);
    adata[i].num_params = i % 32;
  }

  pi_p4info_action_init(p4info, num_actions);

  for (size_t i = 0; i < num_actions; i++) {
    pi_p4info_action_add(p4info, adata[i].id, adata[i].name,
                         adata[i].num_params);
  }

  for (size_t i = 0; i < num_actions; i++) {
    for (size_t p_id = 0; p_id < adata[i].num_params; p_id++) {
      snprintf(name, sizeof(name), "a%zu_p%zu", i, p_id);
      pi_p4info_action_add_param(p4info, adata[i].id, p_id, name, p_id);
    }
  }

  for (size_t i = 0; i < num_actions; i++) {
    TEST_ASSERT_EQUAL_UINT(adata[i].num_params,
                           pi_p4info_action_num_params(p4info, adata[i].id));
  }

  for (size_t i = 0; i < num_actions; i++) {
    for (size_t p_id = 0; p_id < adata[i].num_params; p_id++) {
      snprintf(name, sizeof(name), "a%zu_p%zu", i, p_id);

      TEST_ASSERT_EQUAL_UINT(
          p_id, pi_p4info_action_param_id_from_name(p4info, adata[i].id, name));

      TEST_ASSERT_EQUAL_STRING(
          name, pi_p4info_action_param_name_from_id(p4info, adata[i].id, p_id));

      TEST_ASSERT_EQUAL_UINT(
          p_id, pi_p4info_action_param_bitwidth(p4info, adata[i].id, p_id));
    }
  }

  pi_p4info_action_free(p4info);

  for (size_t i = 0; i < num_actions; i++) {
    free(adata[i].name);
  }
  free(adata);
}

typedef struct {
  pi_p4_id_t id;
  char name[16];
  size_t num_match_fields;
  size_t num_actions;
  pi_p4_id_t match_fields[32];
  pi_p4_id_t actions[32];
} tdata_t;

void gen_rand_ids(pi_p4_id_t *ids, pi_p4_id_t max, size_t num) {
  Pvoid_t set = (Pvoid_t) NULL;
  for (size_t i = 0; i < num; i++) {
    int Rc = 1;
    pi_p4_id_t v;
    while (Rc) {
      v = rand() % max;
      J1T(Rc, set, v);
    }
    J1S(Rc, set, v);
    ids[i] = v;
  }
  Word_t Rc_word;
  J1FA(Rc_word, set);
}

TEST(P4Info, TablesStress) {
  // tables are more complex than fields & actions, because tables reference
  // actions and fields
  size_t num_fields = 4096;
  size_t num_actions = 1024;
  size_t num_tables = 256;
  pi_p4info_field_init(p4info, num_fields);
  pi_p4info_action_init(p4info, num_actions);
  pi_p4info_table_init(p4info, num_tables);

  tdata_t *tdata = calloc(num_tables, sizeof(tdata_t));

  char name[16];
  for (size_t i = 0; i < num_fields; i++) {
    snprintf(name, sizeof(name), "f%zu", i);
    pi_p4info_field_add(p4info, i, name, 1 + i % 128);
  }
  for (size_t i = 0; i < num_actions; i++) {
    snprintf(name, sizeof(name), "a%zu", i);
    // no params to make things easier
    pi_p4info_action_add(p4info, i, name, 0);
  }

  size_t max_match_fields = sizeof(tdata[0].match_fields) / sizeof(pi_p4_id_t);
  size_t max_actions = sizeof(tdata[0].actions) / sizeof(pi_p4_id_t);

  for (size_t i = 0; i < num_tables; i++) {
    tdata[i].id = i;
    snprintf(tdata[i].name, sizeof(tdata[i].name), "t%zu", i);
    tdata[i].num_match_fields = rand() % (max_match_fields + 1);
    tdata[i].num_actions = rand() % (max_actions + 1);
    pi_p4info_table_add(p4info, tdata[i].id, tdata[i].name,
                        tdata[i].num_match_fields, tdata[i].num_actions);
    gen_rand_ids(tdata[i].match_fields, num_fields, tdata[i].num_match_fields);
    for (size_t j = 0; j < tdata[i].num_match_fields; j++) {
      pi_p4_id_t id = tdata[i].match_fields[j];
      pi_p4info_match_type_t match_type = (i + j) % PI_P4INFO_MATCH_TYPE_END;
      snprintf(name, sizeof(name), "f%zu", (size_t) id);
      // name and bw consistent with field initialization above
      pi_p4info_table_add_match_field(p4info, tdata[i].id, id, name, match_type,
                                      1 + j % 128);
    }
    gen_rand_ids(tdata[i].actions, num_actions, tdata[i].num_actions);
    for (size_t j = 0; j < tdata[i].num_actions; j++) {
      pi_p4_id_t id = tdata[i].actions[j];
      pi_p4info_table_add_action(p4info, tdata[i].id, id);
    }
  }

  for (size_t i = 0; i < num_tables; i++) {
    TEST_ASSERT_EQUAL_UINT(tdata[i].id,
                           pi_p4info_table_id_from_name(p4info, tdata[i].name));
    TEST_ASSERT_EQUAL_STRING(tdata[i].name,
                             pi_p4info_table_name_from_id(p4info, tdata[i].id));

    TEST_ASSERT_EQUAL_UINT(
        tdata[i].num_match_fields,
        pi_p4info_table_num_match_fields(p4info, tdata[i].id));
    size_t num;
    const pi_p4_id_t *ids =
        pi_p4info_table_get_match_fields(p4info, tdata[i].id, &num);
    TEST_ASSERT_EQUAL_UINT(tdata[i].num_match_fields, num);
    if (num > 0) {
      TEST_ASSERT_EQUAL_MEMORY(tdata[i].match_fields, ids,
                               sizeof(pi_p4_id_t) * num);
    }
    for (size_t j = 0; j < tdata[i].num_match_fields; j++) {
      TEST_ASSERT_TRUE(
          pi_p4info_table_is_match_field_of(p4info, tdata[i].id,
                                            tdata[i].match_fields[j]));
    }
    TEST_ASSERT_FALSE(
        pi_p4info_table_is_match_field_of(p4info, tdata[i].id, num_fields + 1));
    for (size_t j = 0; j < tdata[i].num_match_fields; j++) {
      TEST_ASSERT_EQUAL_UINT(
          j,
          pi_p4info_table_match_field_index(p4info, tdata[i].id,
                                            tdata[i].match_fields[j]));
    }
    TEST_ASSERT_EQUAL_UINT(
        (size_t) -1,
        pi_p4info_table_match_field_index(p4info, tdata[i].id, num_fields + 1));
    for (size_t j = 0; j < tdata[i].num_match_fields; j++) {
      pi_p4info_match_field_info_t finfo;
      pi_p4info_table_match_field_info(p4info, tdata[i].id, j, &finfo);
      TEST_ASSERT_EQUAL_UINT(tdata[i].match_fields[j], finfo.field_id);
      TEST_ASSERT_EQUAL_STRING(
          pi_p4info_field_name_from_id(p4info, tdata[i].match_fields[j]),
          finfo.name);
      pi_p4info_match_type_t match_type = (i + j) % PI_P4INFO_MATCH_TYPE_END;
      TEST_ASSERT_EQUAL_INT(match_type, finfo.match_type);
      TEST_ASSERT_EQUAL_UINT(1 + j % 128, finfo.bitwidth);
    }

    TEST_ASSERT_EQUAL_UINT(tdata[i].num_actions,
                           pi_p4info_table_num_actions(p4info, tdata[i].id));
    ids = pi_p4info_table_get_actions(p4info, tdata[i].id, &num);
    TEST_ASSERT_EQUAL_UINT(tdata[i].num_actions, num);
    if (num > 0) {
      TEST_ASSERT_EQUAL_MEMORY(tdata[i].actions, ids, sizeof(pi_p4_id_t) * num);
    }
    for (size_t j = 0; j < tdata[i].num_actions; j++) {
      TEST_ASSERT_TRUE(pi_p4info_table_is_action_of(p4info, tdata[i].id,
                                                    tdata[i].actions[j]));
    }
    TEST_ASSERT_FALSE(pi_p4info_table_is_action_of(p4info, tdata[i].id,
                                                   num_actions + 1));
  }

  pi_p4info_field_free(p4info);
  pi_p4info_action_free(p4info);
  pi_p4info_table_free(p4info);

  free(tdata);
}

TEST_GROUP_RUNNER(P4Info) {
  RUN_TEST_CASE(P4Info, Fields);
  RUN_TEST_CASE(P4Info, FieldsByte0Mask);
  RUN_TEST_CASE(P4Info, FieldsStress);
  RUN_TEST_CASE(P4Info, Actions);
  RUN_TEST_CASE(P4Info, ActionsStress);
  RUN_TEST_CASE(P4Info, TablesStress);
}

void test_p4info() {
  RUN_TEST_GROUP(P4Info);
}