/*
 *    webu_auth.cpp
 *
 *    Password hashing and authentication utilities for Motion web server
 *    Uses libxcrypt for bcrypt password hashing (work factor 12)
 *
 *    Copyright 2026 Motion Project
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 */

#include "webu_auth.hpp"
#include <crypt.h>
#include <cstdlib>
#include <cstring>
#include <fstream>

/**
 * Generate bcrypt salt with work factor 12
 */
std::string cls_webu_auth::generate_salt() {
    /* Read random bytes for salt */
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) {
        return "";
    }

    unsigned char random_bytes[16];
    urandom.read(reinterpret_cast<char*>(random_bytes), sizeof(random_bytes));
    urandom.close();

    if (urandom.gcount() != sizeof(random_bytes)) {
        return "";
    }

    /* Base64 encode the random bytes for bcrypt salt
     * bcrypt uses a modified base64 alphabet: ./A-Za-z0-9 */
    static const char b64_alphabet[] =
        "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    char salt_b64[23];  /* 22 chars + null terminator */
    for (int i = 0; i < 22; i++) {
        /* Use 6 bits from random bytes to index into alphabet */
        int byte_idx = (i * 6) / 8;
        int bit_offset = (i * 6) % 8;

        unsigned int value;
        if (bit_offset <= 2) {
            value = (random_bytes[byte_idx] >> bit_offset) & 0x3F;
        } else {
            value = ((random_bytes[byte_idx] >> bit_offset) |
                    (random_bytes[byte_idx + 1] << (8 - bit_offset))) & 0x3F;
        }

        salt_b64[i] = b64_alphabet[value];
    }
    salt_b64[22] = '\0';

    /* Build bcrypt salt string: $2b$12$<22-char-base64> */
    std::string salt = "$2b$12$";
    salt += salt_b64;

    return salt;
}

/**
 * Hash password with bcrypt (work factor 12)
 */
std::string cls_webu_auth::hash_password(const std::string &password) {
    if (password.empty()) {
        return "";
    }

    /* Generate salt */
    std::string salt = generate_salt();
    if (salt.empty()) {
        return "";
    }

    /* Hash password using crypt_r (thread-safe) */
    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    char *hash = crypt_r(password.c_str(), salt.c_str(), &data);
    if (hash == nullptr) {
        return "";
    }

    std::string result(hash);

    /* Clear sensitive data */
    memset(&data, 0, sizeof(data));

    return result;
}

/**
 * Verify password against bcrypt hash
 */
bool cls_webu_auth::verify_password(const std::string &password,
                                     const std::string &hash) {
    if (password.empty() || hash.empty()) {
        return false;
    }

    /* Verify hash format */
    if (!is_bcrypt_hash(hash)) {
        return false;
    }

    /* Hash the password with the same salt (extracted from hash) */
    struct crypt_data data;
    memset(&data, 0, sizeof(data));

    char *result = crypt_r(password.c_str(), hash.c_str(), &data);
    if (result == nullptr) {
        memset(&data, 0, sizeof(data));
        return false;
    }

    /* Compare hashes (constant-time comparison) */
    bool match = (strcmp(result, hash.c_str()) == 0);

    /* Clear sensitive data */
    memset(&data, 0, sizeof(data));

    return match;
}

/**
 * Check if string is a bcrypt hash
 */
bool cls_webu_auth::is_bcrypt_hash(const std::string &str) {
    /* Bcrypt hashes are exactly 60 characters and start with $2b$ or $2a$ */
    if (str.length() != 60) {
        return false;
    }

    return (str.substr(0, 4) == "$2b$" || str.substr(0, 4) == "$2a$");
}

/**
 * Generate cryptographically secure random password
 */
std::string cls_webu_auth::generate_random_password() {
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "!@#$%^&*";
    const int length = 16;

    /* Read random bytes from /dev/urandom */
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) {
        return "";
    }

    unsigned char random_bytes[length];
    urandom.read(reinterpret_cast<char*>(random_bytes), length);
    urandom.close();

    if (urandom.gcount() != length) {
        return "";
    }

    /* Build password from random bytes */
    std::string password;
    password.reserve(length);

    for (int i = 0; i < length; i++) {
        password += charset[random_bytes[i] % (sizeof(charset) - 1)];
    }

    return password;
}
