/*
 * Copyright (c) 2009, Xiliu Tang (xiliu.tang@gmail.com)
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above 
 *       copyright notice, this list of conditions and the following 
 *       disclaimer in the documentation and/or other materials provided 
 *       with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project Website http://code.google.com/p/server1/
 */



#ifndef EVP_HPP_
#define EVP_HPP_
#include <openssl/evp.h>
class EVP {
 public:
  static EVP *CreateMD5() {
    return Create("md5");
  }

  void Update(const char *message, int message_length) {
    EVP_DigestUpdate(&mdctx_, message, message_length);
  }

  void Update(const string &message) {
    Update(message.c_str(), message.size());
  }

  void Finish() {
    EVP_DigestFinal_ex(&mdctx_, md_value_, &md_len_);
  }

  template <class Type>
  inline Type digest() const {
    Type ret;
    for (int i = 0; i < min(md_len_, (unsigned int)sizeof(Type)); ++i) {
      ret = (ret) << 8 | md_value_[i];
    }
    return ret;
  }
  void Reset() {
    EVP_DigestInit_ex(&mdctx_, md_, NULL);
    md_len_ = 0;
  }
 private:
  EVP() : md_len_(0) {
  }

  static EVP *Create(const char *digestname) {
    OpenSSL_add_all_digests();
    const EVP_MD *md = EVP_get_digestbyname(digestname);
    if (md == NULL) {
      return NULL;
    }
    EVP *evp = new EVP;
    evp->md_ = md;
    EVP_MD_CTX_init(&evp->mdctx_);
    EVP_DigestInit_ex(&evp->mdctx_, md, NULL);
    return evp;
  }
  EVP_MD_CTX mdctx_;
  unsigned char md_value_[EVP_MAX_MD_SIZE];
  unsigned int md_len_;
  const EVP_MD *md_;
};

template <>
inline string EVP::digest<string>() const {
  static const char hex[] = {
    '0',
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    'a',
    'b',
    'c',
    'd',
    'e',
    'f',};
  char ret[33] = { 0 };
  for (int i = 0; i < md_len_; ++i) {
    const uint8 x = md_value_[i];
    ret[2 * i] = hex[(x >> 4) & 0xF];
    ret[2 * i + 1] = hex[x & 0xF];
  }
  return ret;
}
#endif  // EVP_HPP_
