#ifndef SECURITYMANAGER_H
#define SECURITYMANAGER_H

#include <string>
#include <cstdint>
#include <random>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <iostream>
using namespace std;

class SecurityManager
{
private:
    mt19937_64 rng_;

public:
    SecurityManager()
    {
        random_device rd;
        rng_.seed(rd());
    }
    string hash_password(const string &password)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];

        EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
        const EVP_MD *md = EVP_sha256();

        if (!mdctx || !md)
        {
            if (mdctx)
                EVP_MD_CTX_free(mdctx);
            return "";
        }

        EVP_DigestInit_ex(mdctx, md, NULL);
        EVP_DigestUpdate(mdctx, password.c_str(), password.length());
        EVP_DigestFinal_ex(mdctx, hash, NULL);
        EVP_MD_CTX_free(mdctx);

        stringstream ss;
        ss << hex << setfill('0');
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            ss << setw(2) << static_cast<int>(hash[i]);
        }

        string result = ss.str();

        if (result.length() != 64)
        {
            cerr << "[ERROR] Hash length incorrect: " << result.length()
                      << " (expected 64)" << endl;
        }

        return result;
    }
    bool verify_password(const string &password, const string &hash)
    {
        return hash_password(password) == hash;
    }
    string generate_session_id()
    {
        const char *hex_chars = "0123456789abcdef";
        string session_id;
        session_id.reserve(64);

        uniform_int_distribution<> dis(0, 15);
        for (int i = 0; i < 64; i++)
        {
            session_id += hex_chars[dis(rng_)];
        }

        return session_id;
    }
    void encrypt_data(const char *input, char *output, size_t size, const char *key, size_t key_len)
    {
        for (size_t i = 0; i < size; i++)
        {
            output[i] = input[i] ^ key[i % key_len];
        }
    }
    void decrypt_data(const char *input, char *output, size_t size, const char *key, size_t key_len)
    {
        encrypt_data(input, output, size, key, key_len);
    }
    bool is_valid_session_id(const string &session_id)
    {
        if (session_id.length() != 64)
            return false;

        for (char c : session_id)
        {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            {
                return false;
            }
        }

        return true;
    }
};

#endif 