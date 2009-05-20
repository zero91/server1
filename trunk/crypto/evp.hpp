#ifndef EVP_HPP_
#define EVP_HPP_
#include <openssl/evp.h>
class EVP {
 public:
  EVP *Create(const string &digestname) {
    OpenSSL_add_all_digests();
    const EVP_MD *md = EVP_get_digestbyname(digestname);
    if (md == NULL) {
      return NULL;
    }
    EVP_MD_CTX_init(&mdctx_);
    EVP_DigestInit_ex(&mdctx_, md, NULL);
  }
  void Update(const char *message, int message_length) {
    EVP_DigestUpdate(&mdctx_, message, message_length);
  }

  void Update(const string &message) {
    Update(message.c_str(), message.size());
  }

  void Finish() {
    EVP_DigestFinal_ex(&mdctx_, md_value_, md_len_);
  }
 private:
  EVP_MD_CTX mdctx_;
  unsigned char md_value_[EVP_MAX_MD_SIZE];
  unsigned int md_len_;
};
#endif  // EVP_HPP_
