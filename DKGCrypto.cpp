/*
    Copyright (C) 2019-Present SKALE Labs

    This file is part of sgxwallet.

    sgxwallet is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    sgxwallet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with sgxwallet.  If not, see <https://www.gnu.org/licenses/>.

    @file DKGCrypto.cpp
    @author Stan Kladko
    @date 2019
*/

#include "DKGCrypto.h"
#include "BLSCrypto.h"
#include "sgxwallet.h"
#include <iostream>

#include <memory>
#include <memory>
#include "SGXWalletServer.hpp"
#include "RPCException.h"

//#include <libBLS/libff/libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>

#include "spdlog/spdlog.h"

std::vector<std::string> SplitString(const char* koefs, const char symbol){
  std::string str(koefs);
  std::string delim;
  delim.push_back(symbol);
  std::vector<std::string> G2_strings;
  size_t prev = 0, pos = 0;
  do
  {
    pos = str.find(delim, prev);
    if (pos == std::string::npos) pos = str.length();
    std::string token = str.substr(prev, pos-prev);
    if (!token.empty()) {
      std::string koef(token.c_str());
      G2_strings.push_back(koef);
    }
    prev = pos + delim.length();
  }
  while (pos < str.length() && prev < str.length());

  return G2_strings;
}

template<class T>
std::string ConvertToString(T field_elem, int base = 10) {
  mpz_t t;
  mpz_init(t);

  field_elem.as_bigint().to_mpz(t);

  char arr[mpz_sizeinbase (t, base) + 2];

  char * tmp = mpz_get_str(arr, base, t);
  mpz_clear(t);

  std::string output = tmp;

  return output;
}

std::string gen_dkg_poly( int _t){
    char *errMsg = (char *)calloc(1024, 1);
    int err_status = 0;
    uint8_t* encrypted_dkg_secret = (uint8_t*) calloc(DKG_MAX_SEALED_LEN, 1);;

    uint32_t enc_len = 0;

    status = gen_dkg_secret (eid, &err_status, errMsg, encrypted_dkg_secret, &enc_len, _t);
    if ( err_status != 0){
        throw RPCException(-666, errMsg ) ;
    }

    if (DEBUG_PRINT) {
      spdlog::info("gen_dkg_secret, status {}", err_status, " err msg ", errMsg);

      /*   std::cerr << "encr raw poly: " << std::endl;
         for ( int i = 0 ; i < 3050; i++)
           printf(" %d ", encrypted_dkg_secret[i] );*/

      spdlog::info("in DKGCrypto encr len is {}", enc_len);
    }

    char *hexEncrPoly = (char *) calloc(DKG_MAX_SEALED_LEN * 2 + 1, 1);//(4*BUF_LEN, 1);

    carray2Hex(encrypted_dkg_secret, DKG_MAX_SEALED_LEN, hexEncrPoly);
    std::string result(hexEncrPoly);



    free(errMsg);
    free(encrypted_dkg_secret);
    free(hexEncrPoly);

    return result;
}

std::vector <std::vector<std::string>> get_verif_vect(const char* encryptedPolyHex, int t, int n){

  char* errMsg1 = (char*) calloc(1024,1);
  int err_status = 0;

  if (DEBUG_PRINT) {
    // std::cerr << "got encr poly " << encryptedPolyHex << std::endl;
    spdlog::info("got encr poly size {}", std::char_traits<char>::length(encryptedPolyHex));
  }

  char* public_shares = (char*)calloc(10000, 1);

  uint64_t enc_len = 0;

  uint8_t* encr_dkg_poly = (uint8_t*) calloc(DKG_MAX_SEALED_LEN, 1);
  if (!hex2carray2(encryptedPolyHex, &enc_len, encr_dkg_poly, 6100)){
      throw RPCException(INVALID_HEX, "Invalid encryptedPolyHex");
  }

  if (DEBUG_PRINT) {
    spdlog::info("enc len {}", enc_len);
    /*std::cerr << "encr raw poly: " << std::endl;
    for ( int i = 0 ; i < 3050; i++)
      printf(" %d ", encr_dkg_poly[i] );*/
  }

  uint32_t len;
  status = get_public_shares(eid, &err_status, errMsg1, encr_dkg_poly, len, public_shares, t, n);
  if ( err_status != 0){
    throw RPCException(-666, errMsg1 );
  }

  if (DEBUG_PRINT) {
    spdlog::info("err msg is {}", errMsg1);

    spdlog::info("public_shares:");
    spdlog::info("{}", public_shares);
//    std::cerr << "public_shares:" << std::endl;
//    std::cerr << public_shares << std::endl;
    spdlog::info("get_public_shares status: {}", err_status);
    //printf("\nget_public_shares status: %d error %s \n\n", err_status, errMsg1);
  }

  std::vector <std::string> G2_strings = SplitString( public_shares, ',');
  std::vector <std::vector <std::string>> pub_shares_vect;
  for ( int i = 0; i < G2_strings.size(); i++){
    std::vector <std::string> koef_str = SplitString(G2_strings.at(i).c_str(), ':');
    pub_shares_vect.push_back(koef_str);
  }

  free(errMsg1);
  free(public_shares);
  free(encr_dkg_poly);

  return pub_shares_vect;
}

std::string get_secret_shares(const std::string& polyName, const char* encryptedPolyHex, const std::vector<std::string>& publicKeys, int t, int n){
  char* errMsg1 = (char*) calloc(1024,1);
  int err_status = 0;

  uint64_t enc_len = 0;

  uint8_t* encr_dkg_poly = (uint8_t*) calloc(DKG_MAX_SEALED_LEN, 1);
  if(!hex2carray2(encryptedPolyHex, &enc_len, encr_dkg_poly, 6100)){
      throw RPCException(INVALID_HEX, "Invalid encryptedPolyHex");
  }

  status = set_encrypted_dkg_poly(eid, &err_status, errMsg1, encr_dkg_poly);
  if ( status != SGX_SUCCESS || err_status!=0){
    throw RPCException(-666, errMsg1 );
  }

  std::string result;
  char *hexEncrKey = (char *) calloc(2 * BUF_LEN, 1);

  for ( int i = 0; i < n; i++){
    uint8_t encrypted_skey[BUF_LEN];
    uint32_t dec_len;

    char cur_share[193];
    char s_shareG2[320];
    std::string pub_keyB = publicKeys.at(i);//publicKeys.substr(128*i, 128*i + 128);
    if (DEBUG_PRINT) {
      spdlog::info("pub_keyB is {}", pub_keyB);
    }
    char pubKeyB[129];
    strncpy(pubKeyB, pub_keyB.c_str(), 128);
    pubKeyB[128] = 0;
    get_encr_sshare(eid, &err_status, errMsg1, encrypted_skey, &dec_len,
                   cur_share, s_shareG2, pubKeyB, t, n, i + 1 );
    if (err_status != 0){
      throw RPCException(-666, errMsg1);
    }

    result += cur_share;

    //uint32_t enc_len = BUF_LEN;
    if (DEBUG_PRINT) {
      spdlog::info("dec len is {}", dec_len);
    }
    carray2Hex(encrypted_skey, dec_len, hexEncrKey);

    std::string DHKey_name = "DKG_DH_KEY_" + polyName + "_" + std::to_string(i) + ":";

   // std::cerr << "hexEncrKey: " << hexEncrKey << std::endl;
    writeDataToDB(DHKey_name, hexEncrKey);

    std::string shareG2_name = "shareG2_" + polyName + "_" + std::to_string(i) + ":";
    if (DEBUG_PRINT) {
      spdlog::info("name to write to db is {}", DHKey_name);
      spdlog::info("name to write to db is {}", shareG2_name);
      spdlog::info("s_shareG2: {}", s_shareG2);
    }
    writeDataToDB(shareG2_name, s_shareG2);

    if (DEBUG_PRINT) {
      spdlog::info("errMsg: {}", errMsg1);
      // std::cerr << "iteration " << i <<" result length is " << result.length() << std::endl ;
      // std::cerr << "iteration " << i <<" share length is " << strlen(cur_share) << std::endl;
      // std::cerr << "iteration " << i <<" share is " << cur_share << std::endl;
    }
  }
  //result += '\0';

  free(encr_dkg_poly);
  free(errMsg1);
  free(hexEncrKey);

  return result;
}

bool VerifyShares(const char* publicShares, const char* encr_sshare, const char * encryptedKeyHex, int t, int n, int ind ){
    char* errMsg1 = (char*) calloc(1024,1);
    int err_status = 0;

    uint64_t dec_key_len ;
    uint8_t encr_key[BUF_LEN];
    if (!hex2carray(encryptedKeyHex, &dec_key_len, encr_key)){
        throw RPCException(INVALID_HEX, "Invalid encryptedPolyHex");
    }
    int result;
    if (DEBUG_PRINT) {
      // std::cerr << "encryptedKeyHex " << encryptedKeyHex << std::endl;
      // std::cerr << "dec_key_len " << dec_key_len << std::endl;


      // std::cerr << "encr_sshare length is " << strlen(encr_sshare) << std::endl; std::cerr << "public shares " << publicShares << std::endl;
      spdlog::info("publicShares length is {}", std::char_traits<char>::length(publicShares));
    }
    char pshares[8193];
    strncpy(pshares, publicShares, strlen(publicShares) + 1);
    //std::cerr << "pshares " << pshares << std::endl;


    dkg_verification(eid, &err_status, errMsg1, pshares, encr_sshare, encr_key, dec_key_len, t, ind, &result);
    if (result == 2){
      throw RPCException(INVALID_HEX, "Invalid public shares");
    }

    if (DEBUG_PRINT) {
      spdlog::info("errMsg1: {}", errMsg1);
      spdlog::info("result is: {}", result);
    }

    free(errMsg1);

    return result;
}

bool CreateBLSShare( const std::string& BLSKeyName, const char * s_shares, const char * encryptedKeyHex){
  if (DEBUG_PRINT) {
    spdlog::info("ENTER CreateBLSShare");
  }
  char* errMsg1 = (char*) calloc(1024,1);
  int err_status = 0;

  uint64_t dec_key_len ;
  uint8_t encr_bls_key[BUF_LEN];
  uint8_t encr_key[BUF_LEN];
  if (!hex2carray(encryptedKeyHex, &dec_key_len, encr_key)){
      throw RPCException(INVALID_HEX, "Invalid encryptedKeyHex");
  }

  uint32_t enc_bls_len = 0;

  //std::cerr << "BEFORE create_bls_key IN ENCLAVE " << std::endl;
  create_bls_key(eid, &err_status, errMsg1, s_shares, encr_key, dec_key_len, encr_bls_key, &enc_bls_len);
  //std::cerr << "AFTER create_bls_key IN ENCLAVE er msg is  " << errMsg1 << std::endl;
  if ( err_status != 0){
     spdlog::info("ERROR IN ENCLAVE");
     throw RPCException(ERROR_IN_ENCLAVE, "Create BLS private key failed in enclave");
  }
  else {
    char *hexBLSKey = (char *) calloc(2 * BUF_LEN, 1);
    //std::cerr << "BEFORE carray2Hex" << std::endl;
      //std::cerr << "enc_bls_len " << enc_bls_len << std::endl;
    carray2Hex(encr_bls_key, enc_bls_len, hexBLSKey);
   // std::cerr << "BEFORE WRITE BLS KEY TO DB" << std::endl;
    writeDataToDB(BLSKeyName, hexBLSKey);
    if (DEBUG_PRINT) {
      spdlog::info("hexBLSKey length is {}", std::char_traits<char>::length(hexBLSKey));
      spdlog::info("bls key {}", BLSKeyName, " is ", hexBLSKey );
    }
    free(hexBLSKey);
    return true;
  }

}

std::vector<std::string> GetBLSPubKey(const char * encryptedKeyHex){
    char* errMsg1 = (char*) calloc(1024,1);
    int err_status = 0;

    uint64_t dec_key_len ;
    uint8_t encr_bls_key[BUF_LEN];
    uint8_t encr_key[BUF_LEN];
    if (!hex2carray(encryptedKeyHex, &dec_key_len, encr_key)){
        throw RPCException(INVALID_HEX, "Invalid encryptedKeyHex");
    }
//    for ( int i = 0; i < BUF_LEN; i++ )
//      std::cerr << encr_key[i] << " ";

    char pub_key[320];
    if (DEBUG_PRINT) {
      spdlog::info("dec_key_len is {}", dec_key_len);
    }
    get_bls_pub_key(eid, &err_status, errMsg1, encr_key, dec_key_len, pub_key);
    if ( err_status != 0){
      throw RPCException(ERROR_IN_ENCLAVE, "Failed to get BLS public key in enclave");
    }
    std::vector<std::string> pub_key_vect = SplitString(pub_key, ':');

    if (DEBUG_PRINT) {
      spdlog::info("errMsg1 is {}", errMsg1);
      spdlog::info("pub key is ");
      for (int i = 0; i < 4; i++)
        spdlog::info("{}", pub_key_vect.at(i));
    }
    return pub_key_vect;
}

std::string decrypt_DHKey(const std::string& polyName, int ind){

  char* errMsg1 = (char*) calloc(1024,1);
  int err_status = 0;

  std::string DH_key_name = polyName + "_" + std::to_string(ind) + ":";
  std::shared_ptr<std::string> hexEncrKey_ptr = readFromDb(DH_key_name, "DKG_DH_KEY_");
  if (DEBUG_PRINT) {
    spdlog::info("encr DH key is {}", *hexEncrKey_ptr);
  }

  char *hexEncrKey = (char *) calloc(2 * BUF_LEN, 1);

  uint64_t DH_enc_len = 0;
  uint8_t encrypted_DHkey[BUF_LEN];
  if (!hex2carray(hexEncrKey_ptr->c_str(), &DH_enc_len, encrypted_DHkey)){
     throw RPCException(INVALID_HEX, "Invalid hexEncrKey");
  }

  char DHKey[ECDSA_SKEY_LEN];

  decrypt_key(eid, &err_status, errMsg1, encrypted_DHkey, DH_enc_len, DHKey);
  if (err_status != 0){
    free(hexEncrKey);
    throw RPCException(ERROR_IN_ENCLAVE, "decrypt key failed in enclave");
  }

  free(errMsg1);
  free(hexEncrKey);

  return DHKey;
}

std::vector<std::string> mult_G2(const std::string& x){
    std::vector<std::string> result(4);
    libff::init_alt_bn128_params();
    libff::alt_bn128_Fr el(x.c_str());
    libff::alt_bn128_G2 elG2 = el * libff::alt_bn128_G2::one();
    elG2.to_affine_coordinates();
    result[0] = ConvertToString(elG2.X.c0);
    result[1] = ConvertToString(elG2.X.c1);
    result[2] = ConvertToString(elG2.Y.c0);
    result[3] = ConvertToString(elG2.Y.c1);
    return result;
}
