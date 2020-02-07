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

    @file SEKManager.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SEKManager.h"
#include "RPCException.h"
#include "BLSCrypto.h"
#include "LevelDB.h"

#include <iostream>
#include <algorithm>

#include "sgxwallet_common.h"
#include "common.h"
#include "sgxwallet.h"

#include "ServerDataChecker.h"
#include "spdlog/spdlog.h"

bool case_insensitive_match(string s1, string s2) {
  //convert s1 and s2 into lower case strings
  transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
  transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
  return s1.compare(s2);
}

void gen_SEK(){

  vector<char> errMsg(1024,0);
  int err_status = 0;
  vector<uint8_t> encr_SEK(1024, 0);
  uint32_t enc_len = 0;

  //vector<char> SEK(65, 0);
  char SEK[65];
  memset(SEK, 0, 65);

  status = generate_SEK(eid, &err_status, errMsg.data(), encr_SEK.data(), &enc_len, SEK);
  if (status != SGX_SUCCESS ||  err_status != 0  ){
    throw RPCException(status, errMsg.data()) ;
  }

  vector<char> hexEncrKey(2 * enc_len + 1, 0);

  carray2Hex(encr_SEK.data(), enc_len, hexEncrKey.data());

  cout << "ATTENTION! THIS IS YOUR KEY FOR BACK UP. PLEASE COPY IT TO THE SAFE PLACE" << endl;
  cout << "key is " << SEK << endl;

  std::string confirm_str = "I confirm";
  std::string buffer;
  do{
    std::cout << " DO YOU CONFIRM THAT YOU COPIED THE KEY? (if you confirm type - I confirm)" << std::endl;
    std::getline(std::cin, buffer);
  } while (case_insensitive_match(confirm_str, buffer)); //(strcmp(confirm_str.c_str(), buffer.c_str()) != 0);

  system("reset");
  LevelDB::getLevelDb()->writeDataUnique("SEK", hexEncrKey.data());

}

void set_SEK(std::shared_ptr<std::string> hex_encr_SEK){
  vector<char> errMsg(1024,0);
  int err_status = 0;
  //vector<uint8_t> encr_SEK(1024, 0);

  uint8_t encr_SEK[BUF_LEN];
  memset(encr_SEK, 0, BUF_LEN);

  uint64_t len;

  if (!hex2carray(hex_encr_SEK->c_str(), &len, encr_SEK)){
    throw RPCException(INVALID_HEX, "Invalid encrypted SEK Hex");
  }

//  std::cerr << "encr hex key is " << *hex_encr_SEK << std::endl;
  std::cerr << "len is " << len << std::endl;

  status = set_SEK(eid, &err_status, errMsg.data(), encr_SEK, len );
  if ( status != SGX_SUCCESS || err_status != 0 ){
    cerr << "RPCException thrown" << endl;
    throw RPCException(status, errMsg.data()) ;
  }

  std::cerr << "status is " << status << std::endl;
  std::cerr << " aes key is " << errMsg.data() << std::endl;
//  for ( uint32_t i = 0; i < 1024; i++)
//     printf("%d ", errMsg[i]);
}

void enter_SEK(){
  vector<char> errMsg(1024,0);
  int err_status = 0;
  vector<uint8_t> encr_SEK(BUF_LEN, 0);
  uint32_t enc_len;

  std::string SEK;
  std::cout << "ENTER BACKUP KEY" << std::endl;
  std::cin >> SEK;
  while (!checkHex(SEK, 16)){
    std::cout << "KEY IS INVALID.TRY ONCE MORE" << std::endl;
    SEK = "";
    std::cin >> SEK;
  }
  if (DEBUG_PRINT)
    std::cerr << "your key is " << SEK << std::endl;

  status = set_SEK_backup(eid, &err_status, errMsg.data(), encr_SEK.data(), &enc_len, SEK.c_str() );
  if (status != SGX_SUCCESS){
    cerr << "RPCException thrown with status " << status << endl;
    throw RPCException(status, errMsg.data()) ;
  }

  vector<char> hexEncrKey(2 * enc_len + 1, 0);

  carray2Hex(encr_SEK.data(), enc_len, hexEncrKey.data());

  LevelDB::getLevelDb() -> deleteKey("SEK");
  LevelDB::getLevelDb() -> writeDataUnique("SEK", hexEncrKey.data());
}

void init_SEK(){
  std::shared_ptr<std::string> encr_SEK_ptr = LevelDB::getLevelDb()->readString("SEK");
  if (encr_SEK_ptr == nullptr){
    spdlog::info("SEK was not created yet. Going to create SEK");
    gen_SEK();
  }
  else{
    if (DEBUG_PRINT)
      spdlog::info("going to set SEK from db" );
    set_SEK(encr_SEK_ptr);
  }
}
