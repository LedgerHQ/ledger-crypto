/*******************************************************************************
*   (c) 2018, 2019 Zondax GmbH
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "coin.h"
#include "app_mode.h"
#include "tx_display.h"
#include "tx_parser.h"
#include "parser_impl.h"
#include <zxmacros.h>

#define NUM_REQUIRED_ROOT_PAGES 6

const char *get_required_root_item(root_item_e i) {
    switch (i) {
        case root_item_chain_id:
            return "chain_id";
        case root_item_account_number:
            return "account_number";
        case root_item_sequence:
            return "sequence";
        case root_item_fee:
            return "fee";
        case root_item_memo:
            return "memo";
        case root_item_msgs:
            return "msgs";
        default:
            return "?";
    }
}

__Z_INLINE uint8_t get_root_max_level(root_item_e i) {
    switch (i) {
        case root_item_chain_id:
            return 2;
        case root_item_account_number:
            return 2;
        case root_item_sequence:
            return 2;
        case root_item_fee:
            return 1;
        case root_item_memo:
            return 2;
        case root_item_msgs:
            return 2;
        default:
            return 0;
    }
}

typedef struct {
    bool root_item_start_token_valid[NUM_REQUIRED_ROOT_PAGES];
    // token where the root_item starts (negative for non-existing)
    uint16_t root_item_start_token_idx[NUM_REQUIRED_ROOT_PAGES];

    // total items
    uint16_t total_item_count;
    // number of items the root_item contains
    uint8_t root_item_number_subitems[NUM_REQUIRED_ROOT_PAGES];

    uint8_t is_default_chain;
} display_cache_t;

display_cache_t display_cache;

parser_error_t tx_display_readTx(parser_context_t *ctx, const uint8_t *data, size_t dataLen) {
    CHECK_PARSER_ERR(parser_init(ctx, data, dataLen))
    CHECK_PARSER_ERR(_readTx(ctx, &parser_tx_obj))
    return parser_ok;
}

__Z_INLINE parser_error_t calculate_is_default_chainid() {
    display_cache.is_default_chain = false;

    // get chain_id
    char outKey[2];
    char outVal[27];
    uint8_t pageCount;
    INIT_QUERY_CONTEXT(outKey, sizeof(outKey),
                       outVal, sizeof(outVal),
                       0, get_root_max_level(root_item_chain_id))
    parser_tx_obj.query.item_index = 0;
    parser_tx_obj.query._item_index_current = 0;

    uint16_t ret_value_token_index;
    CHECK_PARSER_ERR(tx_traverse_find(
            display_cache.root_item_start_token_idx[root_item_chain_id],
            &ret_value_token_index))

    CHECK_PARSER_ERR(tx_getToken(
            ret_value_token_index,
            outVal, sizeof(outVal),
            0, &pageCount))

    if (strcmp(outVal, COIN_DEFAULT_CHAINID) != 0) {
        // If we don't match the default chainid, switch to expert mode
        display_cache.is_default_chain = true;
    }

    return parser_ok;
}

__Z_INLINE bool address_matches_own(char *addr) {
    if (parser_tx_obj.own_addr == NULL) {
        return false;
    }
    if (strcmp(parser_tx_obj.own_addr, addr) != 0) {
        return false;
    }
    return true;
}

parser_error_t tx_indexRootFields() {
    if (parser_tx_obj.flags.cache_valid) {
        return parser_ok;
    }

    // Clear cache
    MEMZERO(&display_cache, sizeof(display_cache_t));
    char tmp_key[70];
    char tmp_val[70];
    MEMZERO(&tmp_key, sizeof(tmp_key));
    MEMZERO(&tmp_val, sizeof(tmp_val));

    // Grouping references
    char reference_msg_type[40];
    MEMZERO(&reference_msg_type, sizeof(reference_msg_type));
    char reference_msg_from[70];
    MEMZERO(&reference_msg_from, sizeof(reference_msg_from));

    parser_tx_obj.filter_msg_type_count = 0;
    parser_tx_obj.filter_msg_from_count = 0;
    parser_tx_obj.flags.msg_type_grouping = 1;
    parser_tx_obj.flags.msg_from_grouping = 1;

    for (root_item_e root_item_idx = 0; root_item_idx < NUM_REQUIRED_ROOT_PAGES; root_item_idx++) {
        uint16_t req_root_item_key_token_idx = 0;

        parser_error_t err = object_get_value(
                &parser_tx_obj.json,
                ROOT_TOKEN_INDEX,
                get_required_root_item(root_item_idx),
                &req_root_item_key_token_idx);

        if (err == parser_no_data) {
            continue;
        }
        CHECK_PARSER_ERR(err)

        // Remember root item start token
        display_cache.root_item_start_token_valid[root_item_idx] = 1;
        display_cache.root_item_start_token_idx[root_item_idx] = req_root_item_key_token_idx;

        // Now count how many items can be found in this root item
        int32_t current_item_idx = 0;
        while (err == parser_ok) {
            INIT_QUERY_CONTEXT(tmp_key, sizeof(tmp_key),
                               tmp_val, sizeof(tmp_val),
                               0, get_root_max_level(root_item_idx))
            parser_tx_obj.query.item_index = current_item_idx;
            strncpy_s(parser_tx_obj.query.out_key,
                      get_required_root_item(root_item_idx),
                      parser_tx_obj.query.out_key_len);

            uint16_t ret_value_token_index;

            err = tx_traverse_find(
                    display_cache.root_item_start_token_idx[root_item_idx],
                    &ret_value_token_index);

            if (err != parser_ok) {
                continue;
            }

            uint8_t pageCount;
            CHECK_PARSER_ERR(tx_getToken(
                    ret_value_token_index,
                    parser_tx_obj.query.out_val,
                    parser_tx_obj.query.out_val_len,
                    0, &pageCount))

            switch (root_item_idx) {
                case root_item_memo: {
                    if (strlen(parser_tx_obj.query.out_val) == 0) {
                        err = parser_query_no_results;
                        continue;
                    }
                    break;
                }
                case root_item_msgs: {
                    // GROUPING: Message Type
                    if (parser_tx_obj.flags.msg_type_grouping && is_msg_type_field(tmp_key)) {
                        // First message, initialize expected type
                        if (parser_tx_obj.filter_msg_type_count == 0) {
                            if (strlen(tmp_val) >= sizeof(reference_msg_type)) {
                                return parser_unexpected_type;
                            }
                            strlcpy(reference_msg_type, tmp_val, sizeof(reference_msg_type));
                            parser_tx_obj.filter_msg_type_valid_idx = current_item_idx;
                        }

                        if (strcmp(reference_msg_type, tmp_val) != 0) {
                            // different values, so disable grouping
                            parser_tx_obj.flags.msg_type_grouping = 0;
                            parser_tx_obj.filter_msg_type_count = 0;
                        }

                        parser_tx_obj.filter_msg_type_count++;
                    }

                    // GROUPING: Message From
                    if (parser_tx_obj.flags.msg_from_grouping && is_msg_from_field(tmp_key)) {
                        // First message, initialize expected from
                        if (parser_tx_obj.filter_msg_from_count == 0) {
                            strlcpy(reference_msg_from, tmp_val, sizeof(reference_msg_from));
                            parser_tx_obj.filter_msg_from_valid_idx = current_item_idx;
                        }

                        if (strcmp(reference_msg_from, tmp_val) != 0) {
                            // different values, so disable grouping
                            parser_tx_obj.flags.msg_from_grouping = 0;
                            parser_tx_obj.filter_msg_from_count = 0;
                        }

                        parser_tx_obj.filter_msg_from_count++;
                    }
                    break;
                }
                default:
                    break;
            }

            display_cache.root_item_number_subitems[root_item_idx]++;
            current_item_idx++;
        }

        if (err != parser_query_no_results && err != parser_no_data) {
            return err;
        }

        display_cache.total_item_count += display_cache.root_item_number_subitems[root_item_idx];
    }

    parser_tx_obj.flags.cache_valid = 1;

    CHECK_PARSER_ERR(calculate_is_default_chainid());

    // turn off grouping if we are not in expert mode
    if (tx_is_expert_mode()) {
        parser_tx_obj.flags.msg_from_grouping = 0;
    }

    // check if from reference value matches the device address that will be signing
    parser_tx_obj.flags.msg_from_grouping_hide_all = 0;
    if (address_matches_own(reference_msg_from)){
        parser_tx_obj.flags.msg_from_grouping_hide_all = 1;
    }

    return parser_ok;
}

__Z_INLINE bool is_default_chainid() {
    CHECK_PARSER_ERR(tx_indexRootFields());
    return display_cache.is_default_chain;
}

bool tx_is_expert_mode() {
    return app_mode_expert() || is_default_chainid();
}

__Z_INLINE uint8_t get_subitem_count(root_item_e root_item) {
    CHECK_PARSER_ERR(tx_indexRootFields())
    if (display_cache.total_item_count == 0)
        return 0;

    int16_t tmp_num_items = display_cache.root_item_number_subitems[root_item];

    switch (root_item) {
        case root_item_chain_id:
        case root_item_sequence:
        case root_item_account_number:
            if (!tx_is_expert_mode()) {
                tmp_num_items = 0;
            }
            break;
        case root_item_msgs:
            // Remove grouped items from list
            if (parser_tx_obj.flags.msg_type_grouping == 1u && parser_tx_obj.filter_msg_type_count > 0) {
                tmp_num_items += 1; // we leave main type
                tmp_num_items -= parser_tx_obj.filter_msg_type_count;
            }
            if (parser_tx_obj.flags.msg_from_grouping == 1u && parser_tx_obj.filter_msg_from_count > 0) {
                if (!parser_tx_obj.flags.msg_from_grouping_hide_all) {
                    tmp_num_items += 1; // we leave main from
                }
                tmp_num_items -= parser_tx_obj.filter_msg_from_count;
            }
            break;
        case root_item_memo:
            break;
        case root_item_fee:
            if (!tx_is_expert_mode()) {
                tmp_num_items -= 1;     // Hide Gas field
            }
            break;
        default:
            break;
    }

    return tmp_num_items;
}

__Z_INLINE parser_error_t retrieve_tree_indexes(uint8_t display_index, root_item_e *root_item, uint8_t *subitem_index) {
    // Find root index | display_index idx -> item_index
    // consume indexed subpages until we get the item index in the subpage
    *root_item = 0;
    *subitem_index = 0;
    while (get_subitem_count(*root_item) == 0) {
        (*root_item)++;
    }

    for (uint16_t i = 0; i < display_index; i++) {
        (*subitem_index)++;
        const uint8_t subitem_count = get_subitem_count(*root_item);
        if (*subitem_index >= subitem_count) {
            // Advance root index and skip empty items
            *subitem_index = 0;
            (*root_item)++;
            while (get_subitem_count(*root_item) == 0){
                (*root_item)++;
            }
        }
    }

    if (*root_item > NUM_REQUIRED_ROOT_PAGES) {
        return parser_no_data;
    }

    return parser_ok;
}

parser_error_t tx_display_numItems(uint8_t *num_items) {
    *num_items = 0;
    CHECK_PARSER_ERR(tx_indexRootFields())

    *num_items = 0;
    for (root_item_e root_item = 0; root_item < NUM_REQUIRED_ROOT_PAGES; root_item++) {
        *num_items += get_subitem_count(root_item);
    }

    return parser_ok;
}

// This function assumes that the tx_ctx has been set properly
parser_error_t tx_display_query(uint16_t displayIdx,
                                char *outKey, uint16_t outKeyLen,
                                uint16_t *ret_value_token_index) {
    CHECK_PARSER_ERR(tx_indexRootFields())

    uint8_t num_items;
    CHECK_PARSER_ERR(tx_display_numItems(&num_items));

    if (displayIdx >= num_items) {
        return parser_display_idx_out_of_range;
    }

    root_item_e root_index = 0;
    uint8_t subitem_index = 0;
    CHECK_PARSER_ERR(retrieve_tree_indexes(displayIdx, &root_index, &subitem_index));

    // Prepare query
    char tmp_val[2];
    INIT_QUERY_CONTEXT(outKey, outKeyLen, tmp_val, sizeof(tmp_val),
                       0, get_root_max_level(root_index))
    parser_tx_obj.query.item_index = subitem_index;
    parser_tx_obj.query._item_index_current = 0;

    strncpy_s(outKey, get_required_root_item(root_index), outKeyLen);

    if (!display_cache.root_item_start_token_valid[root_index]) {
        parser_tx_obj.query.out_val = NULL;
        return parser_no_data;
    }

    CHECK_PARSER_ERR(tx_traverse_find(
            display_cache.root_item_start_token_idx[root_index],
            ret_value_token_index))

    return parser_ok;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

static const key_subst_t key_substitutions[] = {
        {"chain_id",                          "Chain ID"},
        {"account_number",                    "Account"},
        {"sequence",                          "Sequence"},
        {"memo",                              "Memo"},
        {"fee/amount",                        "Fee"},
        {"fee/gas",                           "Gas"},
        {"msgs/type",                         "Type"},

        // FIXME: Are these obsolete?? multisend?
        {"msgs/inputs/address",               "Source Address"},
        {"msgs/inputs/coins",                 "Source Coins"},
        {"msgs/outputs/address",              "Dest Address"},
        {"msgs/outputs/coins",                "Dest Coins"},

        // MsgSend
        {"msgs/value/from_address",           "From"},
        {"msgs/value/to_address",             "To"},
        {"msgs/value/amount",                 "Amount"},

        // MsgDelegate
        {"msgs/value/delegator_address",      "Delegator"},
        {"msgs/value/validator_address",      "Validator"},

        // MsgUndelegate
//        {"msgs/value/delegator_address", "Delegator"},
//        {"msgs/value/validator_address", "Validator"},

        // MsgBeginRedelegate
//        {"msgs/value/delegator_address", "Delegator"},
        {"msgs/value/validator_src_address",  "Validator Source"},
        {"msgs/value/validator_dst_address",  "Validator Dest"},

        // MsgSubmitProposal
        {"msgs/value/description",            "Description"},
        {"msgs/value/initial_deposit/amount", "Deposit Amount"},
        {"msgs/value/initial_deposit/denom",  "Deposit Denom"},
        {"msgs/value/proposal_type",          "Proposal"},
        {"msgs/value/proposer",               "Proposer"},
        {"msgs/value/title",                  "Title"},

        // MsgDeposit
        {"msgs/value/depositer",              "Sender"},
        {"msgs/value/proposal_id",            "Proposal ID"},
        {"msgs/value/amount",                 "Amount"},

        // MsgVote
        {"msgs/value/voter",                  "Description"},
//        {"msgs/value/proposal_id",              "Proposal ID"},
        {"msgs/value/option",                 "Option"},

        // MsgWithdrawDelegationReward
//        {"msgs/value/delegator_address", "Delegator"},      // duplicated
//        {"msgs/value/validator_address", "Validator"},      // duplicated
};

parser_error_t tx_display_make_friendly() {
    CHECK_PARSER_ERR(tx_indexRootFields())

    // post process keys
    for (size_t i = 0; i < array_length(key_substitutions); i++) {
        if (!strcmp(parser_tx_obj.query.out_key, key_substitutions[i].str1)) {
            strncpy_s(parser_tx_obj.query.out_key, key_substitutions[i].str2, parser_tx_obj.query.out_key_len);
            break;
        }
    }

    return parser_ok;
}

