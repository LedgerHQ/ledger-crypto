/*******************************************************************************
*  (c) 2019 Zondax GmbH
*  (c) 2021 CRO Protocol Labs ("Crypto.org")
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define CLA                  0x55

#define HDPATH_LEN_DEFAULT   5

#define HDPATH_0_DEFAULT     (0x80000000u | 0x2cu)
#define HDPATH_1_DEFAULT     (0x80000000u | 0x18au)
#define HDPATH_2_DEFAULT     (0x80000000u | 0u)
#define HDPATH_3_DEFAULT     (0u)
#define HDPATH_4_DEFAULT     (0u)

#define PK_LEN_SECP256K1     33u

typedef enum {
    addr_secp256k1 = 0,
} address_kind_e;

#define VIEW_ADDRESS_OFFSET_SECP256K1       PK_LEN_SECP256K1
#define VIEW_ADDRESS_LAST_PAGE_DEFAULT      0

#if defined(TARGET_NANOS)
#define MENU_MAIN_APP_LINE1                "Cronos POS"
#define MENU_MAIN_APP_LINE2                "Chain ready"
#else
#define MENU_MAIN_APP_LINE1                APPNAME
#define MENU_MAIN_APP_LINE2                "ready"
#endif

#define APPVERSION_LINE1                   "Version:"
#define APPVERSION_LINE2                   ("v" APPVERSION)

#define CRYPTO_BLOB_SKIP_BYTES              0
#define COIN_DEFAULT_CHAINID                "crypto-org-chain-mainnet-1"

// In non-expert mode, the app will convert from basecro to CRO
#define COIN_DEFAULT_DENOM_BASE             "basecro"
#define COIN_DEFAULT_DENOM_REPR             "CRO"
#define COIN_DEFAULT_DENOM_FACTOR           8

#ifdef __cplusplus
}
#endif
